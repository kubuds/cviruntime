#include "1880v2_test_util.h"

typedef bmk1880v2_tdma_tg2l_tensor_fill_constant_param_t param_t;

static void __print_param(const char *tag, FILE *f, param_t *p)
{
  fprintf(
      f, "%s: %u => (%u, %u, %u, %u)\n",
      tag, p->constant,
      p->dst->shape.n, p->dst->shape.c, p->dst->shape.h, p->dst->shape.w);
}

#define print_param(f, p) __print_param(__func__, f, p)

typedef struct {
  u8 constant;
  tl_shape_t dst_shape;
} case_t;

static case_t g_cases[] = {
  {
    37, { 1, 1, 1, 1 }
  }, {
    39, { 1, 1, 1, 2 }
  }, {
    23, { 1, 1, 2, 1 }
  }, {
    19, { 1, 1, 7, 2 }
  }, {
    17, { 1, 1, 2, 7 }
  }, {
    13, { 1, 1, 17, 13 }
  }, {
    11, { 1, 1, 13, 17 }
  }, {
    7, { 1, 1, 10, 60 }
  }, {
    9, { 1, 1, 120, 5 }
  }, {
    2, { 1, 2, 1, 1 }
  }, {
    3, { 1, 1, 1, 2 }
  }, {
    5, { 2, 17, 1,  4 }
  }, {
    41, { 2,  1, 4, 17 }
  }, {
    5, { 2, 17, 1,  4 }
  }, {
    9, { 2,  1, 17, 4 }
  }, {
    17, { 3, 16, 1, 1 }
  }, {
    26, { 3,  1, 2, 8 }
  }, {
    103, { 3, 39, 17, 23 }
  }, {
    255, { 3, 17, 39, 23 }
  }, {
    254, { 3, 36, 16,  20 }
  }, {
    127, { 3, 18,  1, 640 }
  }, {
    128, { 5, 39, 17, 23 }
  }, {
    129, { 5, 17, 39, 23 }
  }, {
    55, { 20, 35,  2, 2 }
  }, {
    1, { 20,  7, 10, 2 }
  }    
};

static void tg2l_tensor_fill_constant_ref(param_t *p, u8 ref_data[])
{
  u64 size = tl_shape_size(&p->dst->shape);

  for (u64 i = 0; i < size; i++)
    ref_data[i] = p->constant;
}

static void test_param_tg2l(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, param_t *p)
{
  print_param(stderr, p);
  u64 size = tl_shape_size(&p->dst->shape);

  bmk1880v2_tdma_tg2l_tensor_fill_constant(bmk, p);
  test_submit(ctx);
  u8 *dst_data = get_tensor_l2g(ctx, bmk, p->dst);

  u8 *ref_data = (u8 *)malloc(sizeof(u8) * size);
  tg2l_tensor_fill_constant_ref(p, ref_data);

  for (u64 i = 0; i < size; i++) {
    if (dst_data[i] != ref_data[i]) {
      fprintf(stderr, "comparing failed at dst[%" PRIu64 "], got %d, exp %d\n",
              i, dst_data[i], ref_data[i]);
      exit(-1);
    }
  }

  free(dst_data);
  free(ref_data);
}

static void destroy_param_tg2l(bmk_ctx_t *bmk, param_t *p)
{
  free_tl(bmk, p->dst);
}

static void test_one_case(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk, case_t *c)
{
  for (int dst_align = 0; dst_align < 2; dst_align++) {
    param_t p;
    memset(&p, 0, sizeof(p));
    p.constant = c->constant;
    p.dst = alloc_tl(bmk, c->dst_shape, FMT_I8, dst_align);

    test_param_tg2l(ctx, bmk, &p);
    destroy_param_tg2l(bmk, &p);
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
