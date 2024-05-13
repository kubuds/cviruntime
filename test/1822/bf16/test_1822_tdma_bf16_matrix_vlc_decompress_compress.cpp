#include "../1822_test_util.h"

typedef bmk1822_tdma_tg2l_matrix_copy_decompressed_param_t decompress_param_t;
typedef bmk1822_tdma_l2tg_matrix_copy_compressed_param_t compress_param_t;

typedef struct{
  decompress_param_t dec_p;
  compress_param_t com_p;
} param_t;

static void __print_param(const char *tag, FILE *f, param_t *p)
{
  fprintf(
      f, "%s: (%u, %u, %u, %u) => %s\n",
      tag,
      p->dec_p.dst->shape.n, p->dec_p.dst->shape.c, p->dec_p.dst->shape.w, p->dec_p.dst->shape.col,
      (p->dec_p.dst->fmt == FMT_I8)? "signed": "unsigned");
}

#define print_param(f, p) __print_param(__func__, f, p)

typedef struct {
  mg_shape_t src_shape;
  ml_shape_t dst_shape;
} case_t;

static case_t g_cases[] = {
  {
    { 0, 1 },
    { 0, 1, 1, 1 },
  },
  {
    { 0, 2 },
    { 0, 1, 2, 2 },
  },
  {
    { 0, 2 },
    { 0, 2, 1, 2 },
  },
  {
    { 0, 7 },
    { 0, 1, 7, 7 },
  },
#ifndef ENABEL_SIMPLE_BMK1822_VLC_TEST
  {
    { 0, 7 },
    { 0, 2, 4, 7 },
  },
  {
    { 0, 7 },
    { 0, 7, 1, 7 },
  },
  {
    { 0, 17 },
    { 0, 1, 17, 17 },
  },
  {
    { 0, 17 },
    { 0, 3, 7, 17 },
  },
  {
    { 0, 17 },
    { 0, 17, 1, 17 },
  },
  {
    { 0, 60 },
    { 0, 1, 60, 60 },
  },
  {
    { 0, 60 },
    { 0, 30, 2, 60 },
  },
  {
    { 0, 60 },
    { 0, 60, 1, 60 },
  }
#endif /* ifndef ENABEL_SIMPLE_BMK1822_VLC_TEST*/
};

static void test_param_g2l(bmctx_t *ctx, bmk_ctx_t *bmk, param_t *p, u16 *src_data,
  CommandInfo* cmd_info)
{
  print_param(stderr, p);
  u64 size = ml_shape_size(&p->dec_p.dst->shape);
  u64 bytesize = size * bytesize_of_fmt(p->dec_p.dst->fmt);
  int is_signed = (p->dec_p.dst->fmt == FMT_I8);

  u16 *gmem_data;
  size_t bs_size;
  size_t data_type = (p->dec_p.dst->fmt == FMT_BF16) ? 1 : 0;

  gmem_data = (u16* ) vlc_compress((u8* )src_data, bytesize, is_signed, data_type, &bs_size, cmd_info, NULL);

  //1. send compressed one to gaddr and decompress from gaddr to local
  put_compressed_mg_gmem(ctx, p->dec_p.src, (u8* ) gmem_data, bs_size);
  bmk1822_tdma_g2l_matrix_copy_decompressed(bmk, &p->dec_p);
  test_submit(ctx);

  //2. decompress from sram
  bmk1822_tdma_l2g_matrix_copy_compressed(bmk, &p->com_p);
  test_submit(ctx);

  //3. get final data
  size_t bs_buf_size = get_out_bs_buf_size(bytesize, data_type);
  u16 *dst_data = (u16* )get_compressed_mg_gmem(ctx, p->com_p.dst, bs_buf_size);

  for (u64 i = 0; i < bs_size / 2; i++) {
    if (dst_data[i] != gmem_data[i]) {
      fprintf(stderr, "vlc compress comparing failed at dst[%" PRIx64 "], got %d, exp %d\n",
              i, dst_data[i], gmem_data[i]);
      exit(-1);
    }
  }

  free(dst_data);
  free(gmem_data);
}

static void destroy_param_g2l(bmctx_t *ctx, bmk_ctx_t *bmk, param_t *p)
{
  free_compressed_mg_gmem(ctx, p->dec_p.src);
  free_compressed_mg_gmem(ctx, p->com_p.dst);
  free_ml(bmk, p->dec_p.dst);
}

static void test_one_case(bmctx_t *ctx, bmk_ctx_t *bmk, case_t *c)
{
  fmt_t fmts[] = { FMT_BF16 };
  u8 fmts_sz = sizeof(fmts) / sizeof(fmts[0]);

  for (u32 row = 1; row < 13; row += 2) {
    c->src_shape.row = row;
    c->dst_shape.n = row;
    for (int dst_align = 0; dst_align < 2; dst_align++) {
      for (u8 fmt_i = 0; fmt_i < fmts_sz; fmt_i++) {
        fmt_t fmt = fmts[fmt_i];
        param_t p;
        memset(&p, 0, sizeof(p));
        //put compressed data to gaddr ->decompress to local -> compress to gaddr

        int is_signed = (fmt == FMT_I8);
        int data_type = (fmt == FMT_BF16) ? 1 : 0;

        CommandInfo cmd_info;
        memset(&cmd_info, 0, sizeof(CommandInfo));
        cmd_info.signedness = is_signed;
        cmd_info.is_bfloat16 = data_type;
        cmd_info.bias0 = 127;

        // <! not support bias0/1 setting compress by hw
        //get_vlc_compressed_meta(src_data, size, fmt, &bs_size, &cmd_info);

        //1. alloc decompress
        p.dec_p.src = alloc_vlc_compressed_mg_gmem(ctx, c->src_shape, fmt, &cmd_info);
        p.dec_p.dst = alloc_ml_bf16(bmk, c->dst_shape, fmt, dst_align);

        u64 size = ml_shape_size(&p.dec_p.dst->shape);
        u16 *src_data = (u16 *)malloc(sizeof(u16) * size);
        vlc_init_testdata(src_data, size, fmt == FMT_I8, fmt == FMT_BF16);

        assert(p.dec_p.dst);

        //2. alloc compress
        p.com_p.src = p.dec_p.dst; //alloc_tl(bmk, c->lmem_shape, fmt, align);
        p.com_p.dst = alloc_vlc_compressed_mg_gmem(ctx, c->src_shape, fmt, &cmd_info);

        //3. test: the seqence like below:
        //3.1 put compressed data to gaddr
        //3.2 decompress to local
        //3.3 compress to gaddr
        //printf ("row %u is_align %d fmt %d\n", row, dst_align, fmt);
        test_param_g2l(ctx, bmk, &p, src_data, &cmd_info);
        destroy_param_g2l(ctx, bmk, &p);
        free(src_data);
      }
    }
  }
}

int main()
{
  bmctx_t ctx;
  bmk_ctx_t *bmk;
  test_init(&ctx, &bmk);

  u32 nr_cases = sizeof(g_cases) / sizeof(g_cases[0]);
  for (u32 i = 0; i < nr_cases; i++)
    test_one_case(&ctx, bmk, &g_cases[i]);

  test_exit(&ctx);
  return 0;
}
