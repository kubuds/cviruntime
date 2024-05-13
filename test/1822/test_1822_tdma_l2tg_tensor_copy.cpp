#include "1822_test_util.h"

typedef bmk1822_tdma_l2tg_tensor_copy_param_t param_t;

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
    { 1, 1, 2, 1 },
  }, {
    { 1, 1, 7, 2 },
    { 1, 1, 2, 7 },
  }, {
    { 1, 1, 17, 13 },
    { 1, 1, 13, 17 },
  }, {
    { 1, 1, 10, 60 },
    { 1, 1, 120, 5 },
  }, {
    { 1, 2, 1, 1 },
    { 1, 1, 1, 2 },
  }, {
    { 2, 17, 1,  4 },
    { 2,  1, 4, 17 },
  }, {
    { 2, 17, 1,  4 },
    { 2,  1, 17, 4 },
  }, {
    { 3, 16, 1, 1 },
    { 3,  1, 2, 8 },
  }, {
    { 3, 39, 17, 23 },
    { 3, 17, 39, 23 },
  }, {
    { 3, 36, 16,  20 },
    { 3, 18,  1, 640 },
  }, {
    { 5, 39, 17, 23 },
    { 5, 17, 39, 23 },
  }, {
    { 20, 35,  2, 2 },
    { 20,  7, 10, 2 },
  }    
};

static void l2tg_tensor_copy_ref(param_t *p, u8 ref_data[], u8 src_data[])
{
  u64 size = tl_shape_size(&p->src->shape);

  for (u64 i = 0; i < size; i++)
    ref_data[i] = src_data[i];
}

static void test_param_l2g(bmctx_t *ctx, bmk_ctx_t *bmk, param_t *p)
{
  print_param(stderr, p);
  u64 size = tl_shape_size(&p->src->shape);

  u8 *src_data = (u8 *)malloc(sizeof(u8) * size);
  for (u64 i = 0; i < size; i++)
    src_data[i] = 200 + i;

  put_tensor_g2l(ctx, bmk, p->src, src_data);
  bmk1822_tdma_l2g_tensor_copy(bmk, p);
  test_submit(ctx);
  u8 *dst_data = get_tg_gmem(ctx, p->dst);

  u8 *ref_data = (u8 *)malloc(sizeof(u8) * size);
  l2tg_tensor_copy_ref(p, ref_data, src_data);

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


static void destroy_param_l2g(bmctx_t *ctx, bmk_ctx_t *bmk, param_t *p)
{
  free_tg_gmem(ctx, p->dst);
  free_tl(bmk, p->src);
}

static void test_one_case(bmctx_t *ctx, bmk_ctx_t *bmk, case_t *c)
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
  bmctx_t ctx;
  bmk_ctx_t *bmk;
  test_init(&ctx, &bmk);

  u32 nr_cases = sizeof(g_cases) / sizeof(g_cases[0]);
  for (u32 i = 0; i < nr_cases; i++)
    test_one_case(&ctx, bmk, &g_cases[i]);

  test_exit(&ctx);
  return 0;
}
