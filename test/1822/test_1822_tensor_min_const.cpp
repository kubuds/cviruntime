#include "1822_test_util.h"

static void tl_min_const_ref(s8 *a, s8 b, s8 *max, u64 size)
{
  for (u64 i = 0; i < size; i++) {
    if (a[i] > b)
      max[i] = b;
    else
      max[i] = a[i];
  }
}

static void test_tl_min_const(bmctx_t *ctx, bmk_ctx_t *bk_ctx, int eu_align)
{
  int n = 3;
  int c = 39;
  int h = 7;
  int w = 37;

  tl_shape_t tl_shape;
  tl_shape.n = n;
  tl_shape.c = c;
  tl_shape.h = h;
  tl_shape.w = w;

  u64 size = n * c * h * w;
  s8 *a_data = (s8 *)xmalloc(size);
  for (u64 i = 0; i < size; i++)
    a_data[i] = (s8)(i % 256);

  s8 b = 47;

  s8 *ref_data = (s8 *)xmalloc(size);
  tl_min_const_ref(a_data, b, ref_data, size);

  tl_t *tl_a = alloc_tl(bk_ctx, tl_shape, FMT_I8, eu_align);
  tl_t *tl_min = alloc_tl(bk_ctx, tl_shape, FMT_I8, eu_align);

  put_tensor_g2l(ctx, bk_ctx, tl_a, (u8 *)a_data);
  bmk1822_tiu_element_wise_min_param_t p7;
  p7.min = tl_min;
  p7.a = tl_a;
  p7.b_is_const = 1;
  p7.b_const.val = b;
  p7.b_const.is_signed = 1;
  bmk1822_tiu_element_wise_min(bk_ctx, &p7);
  u8 *min_data = get_tensor_l2g(ctx, bk_ctx, tl_min);

  for (u64 i = 0; i < size; i++) {
    if ((s8)min_data[i] != ref_data[i]) {
      fprintf(stderr, "comparing failed at ofmap_data[%" PRIu64 "], got %x, exp %x\n",
             i, min_data[i], ref_data[i]);
      exit(-1);
    }
  }

  free_tl(bk_ctx, tl_min);
  free_tl(bk_ctx, tl_a);

  free(a_data);
  free(ref_data);
  free(min_data);
}

int main()
{
  bmctx_t ctx;
  bmk_ctx_t *bk_ctx;
  test_init(&ctx, &bk_ctx);

  test_tl_min_const(&ctx, bk_ctx, 0);
  test_tl_min_const(&ctx, bk_ctx, 1);

  test_exit(&ctx);
  return 0;
}
