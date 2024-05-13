#include "1880v2_test_util.h"

typedef bmk1880v2_tdma_l2tg_tensor_copy_cw_transposed_param_t param_t;

static void __print_param(const char *tag, FILE *f, param_t *p)
{
  fprintf(
      f, "%s: (%u, %u, %u, %u) => (%u, %u, %u, %u)\n",
      tag,
      p->src->shape.n, p->src->shape.c, p->src->shape.h, p->src->shape.w,
      p->dst->shape.n, p->dst->shape.c, p->dst->shape.h, p->dst->shape.w);
}

#define print_param(f, p) __print_param(__func__, f, p)

typedef struct {
  tl_shape_t src_shape;
  tg_shape_t dst_shape;
} case_t;

static case_t g_cases[] = {
  {
    { 1, 1, 1, 1 },
    { 1, 1, 1, 1 },
  }, {
    { 1, 1, 1, 2 },
    { 1, 2, 1, 1 },
  }, {
    { 1, 1, 7, 2 },
    { 1, 2, 7, 1 },
  }, {
    { 1,  1, 17, 13 },
    { 1, 13, 17,  1 },
  }, {
    { 1,  1, 10, 60 },
    { 1, 60, 10,  1 },
  }, {
    { 1, 2, 1, 1 },
    { 1, 1, 1, 2 },
  }, {
    {  2, 17, 1,  4 },
    {  2,  4, 1, 17 },
  }, {
    {  2, 17, 3,  4 },
    {  2,  4, 3, 17 },
  }, {
    {  3, 16, 7,  1 },
    {  3,  1, 7, 16 },
  }, {
    {  3, 39, 17, 23 },
    {  3, 23, 17, 39 },
  }, {
    {  3, 36,  16, 20 },
    {  3, 20,  16, 36 },
  }, {
    {  5, 39, 17, 23 },
    {  5, 23, 17, 39 },
  }, {
    { 20, 35,  2,  2 },
    { 20,  2,  2, 35 },
  }, {
    { 20, 35,  3,  2 },
    { 20,  2,  3, 35 },
  }    
};

static void l2tg_tensor_copy_cw_transposed_ref(
    param_t *p, u8 ref_data[], u8 src_data[])
{
  tl_shape_t s = p->src->shape;
  u32 n = s.n;
  u32 c = s.c;
  u32 h = s.h;
  u32 w = s.w;

  for (u32 ni = 0; ni < n; ni++) {
    for (u32 ci = 0; ci < c; ci++) {
      for (u32 hi = 0; hi < h; hi++) {
        for (u32 wi = 0; wi < w; wi++) {
          u32 src_i = ni * c * h * w + ci * h * w + hi * w + wi;
          u32 dst_i = ni * c * h * w + wi * h * c + hi * c + ci;
          ref_data[dst_i] = src_data[src_i];
        }
      }
    }
  }
}

static void test_param_l2g(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, param_t *p)
{
  print_param(stderr, p);
  u64 size = tl_shape_size(&p->src->shape);

  u8 *src_data = (u8 *)malloc(sizeof(u8) * size);
  for (u64 i = 0; i < size; i++)
    src_data[i] = 200 + i;

  put_tensor_g2l(ctx, bmk, p->src, src_data);
  bmk1880v2_tdma_l2g_tensor_copy_cw_transposed(bmk, p);
  test_submit(ctx);
  u8 *dst_data = get_tg_gmem(ctx, p->dst);

  u8 *ref_data = (u8 *)malloc(sizeof(u8) * size);
  l2tg_tensor_copy_cw_transposed_ref(p, ref_data, src_data);

  for (u64 i = 0; i < size; i++) {
    if (dst_data[i] != ref_data[i]) {
      fprintf(stderr, "comparing failed at dst[%" PRIu64 "], got %d, exp %d\n",
              i, dst_data[i], ref_data[i]);
      exit(-1);
    }
  }

  free(src_data);
  free(dst_data);
  free(ref_data);
}


static void destroy_param_l2g(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, param_t *p)
{
  free_tg_gmem(ctx, p->dst);
  free_tl(bmk, p->src);
}

static void test_one_case(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, case_t *c)
{
  for (int src_align = 0; src_align < 2; src_align++) {
    param_t p;
    memset(&p, 0, sizeof(p));

    p.src = alloc_tl(bmk, c->src_shape, FMT_I8, src_align);
    p.dst = alloc_tg_gmem(ctx, c->dst_shape, FMT_I8);
    test_param_l2g(ctx, bmk, &p);
    destroy_param_l2g(ctx, bmk, &p);

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
