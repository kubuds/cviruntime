#include "../1880v2_test_util.h"
#include "1880v2_bf16_util.h"

typedef bmk1880v2_tdma_tg2l_tensor_copy_nc_transposed_param_t param_t;

static void __print_param(const char *tag, FILE *f, param_t *p)
{
  fprintf(
      f, "%s: (%u, %u, %u, %u) => (%u, %u, %u, %u)\n",
      tag,
      p->src->shape.n, p->src->shape.c, p->src->shape.h, p->src->shape.w,
      p->dst->shape.n, p->dst->shape.c, p->dst->shape.h, p->dst->shape.w);
}

#define print_param(f, p) __print_param(__func__, f, p)

static fmt_type input_fmt[] = {
 {FMT_BF16, FMT_BF16},
 {FMT_I8, FMT_BF16},
 {FMT_U8, FMT_BF16},
};

typedef struct {
  tg_shape_t src_shape;
  tl_shape_t dst_shape;
} case_t;

static case_t g_cases[] = {
  {
    { 1, 1, 1, 1 },
    { 1, 1, 1, 1 },
  }, {
    { 1, 1, 1, 2 },
    { 1, 1, 1, 2 },
  }, {
    { 1, 1, 1, 2 },
    { 1, 1, 2, 1 },
  }, {
    { 1, 1, 7, 2 },
    { 1, 1, 7, 2 },
  }, {
    { 1, 1, 7, 2 },
    { 1, 1, 2, 7 },
  }, {
    { 1, 1, 17, 13 },
    { 1, 1, 17, 13 },
  }, {
    { 1, 1, 17, 13 },
    { 1, 1, 13, 17 },
  }, {
    { 1, 1, 10, 60 },
    { 1, 1, 10, 60 },
  }, {
    { 1, 1, 10, 60 },
    { 1, 1, 2, 300 },
  }, {
    { 1, 1, 10, 60 },
    { 1, 1, 3, 200 },
  }, {
    { 1, 1, 10, 60 },
    { 1, 1, 4, 150 },
  }, {
    { 1, 1, 10, 60 },
    { 1, 1, 5, 120 },
  }, {
    { 1, 1, 10, 60 },
    { 1, 1, 60, 10 },
  }, {
    { 1, 1, 120, 5 },
    { 1, 1, 120, 5 },
  }, {
    { 1, 2, 1, 1 },
    { 2, 1, 1, 1 },
  }, {
    { 2, 17, 1, 4 },
    { 17, 2, 1, 4 },
  }, {
    { 2, 17, 1, 4 },
    { 17, 2, 2, 2 },
  }, {
    { 2, 17, 1, 4 },
    { 17, 2, 4, 1 },
  }, {
    { 17, 2, 2, 2 },
    { 2, 17, 2, 2 },
  }, {
    { 17, 2, 4, 1 },
    { 2, 17, 4, 1 },
  }, {
    {  3, 16, 1, 1 },
    { 16,  3, 1, 1 },
  }, {
    { 3, 39, 23, 17 },
    { 39, 3, 23, 17 },
  }, {
    { 3, 39, 17, 23 },
    { 39, 3, 17, 23 },
  }, {
    { 3, 36,  16, 20 },
    { 36, 3,  16, 20 },
  }, {
    { 3, 36,  16, 20 },
    { 36, 3,  2, 160 },
  }, {
    { 3, 36,  16, 20 },
    { 36, 3,  4, 80 },
  }, {
    { 3, 36,  16, 20 },
    { 36, 3,  8, 40 },
  }, {
    { 3, 36,  16, 20 },
    { 36, 3,  20, 16 },
  }, {
    { 3, 36,  16, 20 },
    { 36, 3,  32, 10 },
  }, {
    { 3, 36,  16, 20 },
    { 36, 3,  64, 5 },
  }, {
    { 5, 39, 17, 23 },
    { 39, 5, 17, 23 },
  }, {
    { 20, 35, 2, 2 },
    { 35, 20, 2, 2 },
  }, {
    { 35, 20, 2, 2 },
    { 20, 35, 2, 2 },
  }, {
    { 36, 3, 160, 2 },
    { 3, 36, 160, 2 },
  }, {
    { 36, 3, 160, 2 },
    { 3, 36, 2, 160 },
  }, {
    { 36, 3, 160, 2 },
    { 3, 36, 4, 80 },
  }, {
    { 36, 3, 160, 2 },
    { 3, 36, 8, 40 },
  }, {
    { 36, 3, 160, 2 },
    { 3, 36, 10, 32 },
  }, {
    { 36, 3, 160, 2 },
    { 3, 36, 20, 16 },
  }, {
    { 39, 5, 23, 17 },
    { 5, 39, 23, 17 },
  }    
};

static void tg2l_tensor_copy_nc_transposed_ref(
    param_t *p, u16 ref_data[], u16 src_data[])
{
  tg_shape_t s = p->src->shape;
  u32 n = s.n;
  u32 c = s.c;
  u32 hw = s.h * s.w;
  for (u32 ni = 0; ni < n; ni++) {
    for (u32 ci = 0; ci < c; ci++) {
      for (u32 hwi = 0; hwi < hw; hwi++) {
        u32 src_i = ni * c * hw + ci * hw + hwi;
        u32 dst_i = ci * n * hw + ni * hw + hwi;
        if(p->src->fmt == FMT_BF16 && p->dst->fmt == FMT_BF16)
          ref_data[dst_i] = src_data[src_i];
        else {
          u8* u8src_data = (u8*)src_data;
          u8 sign = p->src->fmt == FMT_I8 ? 1 : 0;
          ref_data[dst_i] = convert_int8_bf16(u8src_data[src_i], sign);
        }
      }
    }
  }
}

static void test_param_g2l(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, param_t *p)
{
  print_param(stderr, p);
  u64 size = tl_shape_size(&p->dst->shape);

  u16 *u16src_data = (u16 *)malloc(sizeof(u16) * size);
  u8 *u8src_data = (u8 *)malloc(sizeof(u8) * size);
  u8 *src_data;
  if(p->src->fmt == FMT_BF16) {
    float val = -100;
    for(u64 i = 0; i < size; i++) {
      u16src_data[i] = generate_bf16_corner_val(val);
      val += 0.1;
    }
    src_data = (u8*)u16src_data;
  } else {
    for(u64 i = 0; i < size; i++) {
      u8src_data[i] = 200 + i;
    }
    src_data = u8src_data;
  }

  put_tg_bf16_gmem(ctx, p->src, (u8*) src_data);
  bmk1880v2_tdma_g2l_bf16_tensor_copy_nc_transposed(bmk, p);
  test_submit(ctx);
  u16 *dst_data = (u16 *) get_bf16_tensor_l2g(ctx, bmk, p->dst, p->dst->fmt);

  u16 *ref_data = (u16 *)malloc(sizeof(u16) * size);
  tg2l_tensor_copy_nc_transposed_ref(p, ref_data, (u16*) src_data);

  for (u64 i = 0; i < size; i++) {
    if (dst_data[i] != ref_data[i]) {
      fprintf(stderr, "comparing failed at dst[%" PRIu64 "], got %x, exp %x\n",
              i, dst_data[i], ref_data[i]);
      exit(-1);
    }
  }

  free(u8src_data);
  free(u16src_data);
  free(dst_data);
  free(ref_data);
}


static void destroy_param_g2l(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, param_t *p)
{
  free_tl(bmk, p->dst);
  free_tg_gmem(ctx, p->src);
}

static void test_one_case(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, case_t *c)
{
  u32 nr_fmt = sizeof(input_fmt) / sizeof(input_fmt[0]);
  for (u32 i = 0; i < nr_fmt; i++) {
    for (int dst_align = 0; dst_align < 2; dst_align++) {
      param_t p;
      memset(&p, 0, sizeof(p));
  
      p.src = alloc_tg_bf16_gmem(ctx, c->src_shape, input_fmt[i].src_fmt);
      p.dst = alloc_tl(bmk, c->dst_shape, input_fmt[i].dst_fmt, dst_align);
      test_param_g2l(ctx, bmk, &p);
      destroy_param_g2l(ctx, bmk, &p);
  
    }
  }
}

int main()
{
  CVI_RT_HANDLE ctx;
  bmk_ctx_t *bmk;
  test_init(&ctx, &bmk);

  u32 nr_cases = sizeof(g_cases) / sizeof(g_cases[0]);
  for (u32 i = 0; i < nr_cases; i++)
    test_one_case(&ctx, bmk, &g_cases[i]);

  test_exit(&ctx);
  return 0;
}
