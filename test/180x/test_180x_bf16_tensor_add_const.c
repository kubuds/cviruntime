#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>

#include "test_cvikernel_util.h"

static void tl_add_const_ref(
    uint16_t *ref_low,
    uint16_t *a_low,
    uint16_t b,
    uint64_t size, int relu_enable)
{
  for (uint64_t i = 0; i < size; i++) {
    float ta = cvk_convert_bf16_fp32(a_low[i]);
    float tb = cvk_convert_bf16_fp32(b);
    float res = ta + tb;
    if(relu_enable && res <0)
        res = 0;
    ref_low[i] = cvk_convert_fp32_bf16(res);
  }
}

static int test_tl_add_const(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx, int eu_align)
{
  int ret = 0;
  int n = 3;
  int c = 9; // 39 -> 9 for 180x
  int h = 7;
  int w = 37;
  cvk_tl_shape_t tl_shape;
  tl_shape.n = n;
  tl_shape.c = c;
  tl_shape.h = h;
  tl_shape.w = w;

  cvk_fmt_t fmt_type = CVK_FMT_BF16;
  uint32_t size = n * c * h  * w;
  uint32_t data_size = size * (fmt_type == CVK_FMT_BF16 ? 2 : 1);
  uint16_t *a_low_data = (uint16_t *)malloc(data_size);
  uint16_t *ref_low_data = (uint16_t *)malloc(data_size);
  if (!a_low_data || !ref_low_data) {
    ret = -1;
    goto fail_exit;
  }

  cvk_tl_t *tl_a_low = cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, tl_shape, fmt_type, eu_align);
  cvk_tl_t *tl_res_low = cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, tl_shape, fmt_type, eu_align);
  if (!tl_a_low || !tl_res_low) {
    ret = -1;
    goto fail_exit_2;
  }

  uint16_t b = cvk_convert_fp32_bf16(-3);

  for(int relu_enable = 0; relu_enable < 2; relu_enable++) {

    for (uint64_t i = 0; i < size; i++) {
      a_low_data[i] = cvk_convert_fp32_bf16(i);
    }

    tl_add_const_ref(ref_low_data,
                     a_low_data,
                     b, size,relu_enable);

    tensor_copy_s2d_g2l(rt_handle, cvk_ctx, tl_a_low, (uint8_t*) a_low_data);

    cvk_tiu_add_param_t p4;
    p4.res_high = 0;
    p4.res_low = tl_res_low;
    p4.a_high = 0;
    p4.a_low = tl_a_low;
    p4.b_is_const = 1;
    p4.b_const.val = b;
//    p4.b_const.is_signed = b_is_signed;
//    p4.rshift_bits = rshift_bits;
    p4.relu_enable = relu_enable;
    cvk_ctx->ops->tiu_add(cvk_ctx, &p4);

//    uint8_t *res_high_data = tensor_copy_l2g_d2s(rt_handle, cvk_ctx, tl_res_high);
    uint16_t *res_low_data = (uint16_t *) tensor_copy_l2g_d2s(rt_handle, cvk_ctx, tl_res_low);
    for (uint64_t i = 0; i < size; i++) {
      if (res_low_data[i] != ref_low_data[i]) {
        fprintf(stderr, "comparing failed at res_low_data[%" PRIu64 "], got %d, exp %d\n",
                i, res_low_data[i], ref_low_data[i]);
        ret = -1;
        break;
      }
    }

    free(res_low_data);
  }

fail_exit_2:
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_res_low);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_a_low);

fail_exit:
  free(a_low_data);
  free(ref_low_data);

  return ret;
}

int main(int argc, char **argv)
{
  int ret = 0;
  CVI_RT_HANDLE rt_handle = NULL;
  cvk_context_t *cvk_ctx = NULL;

  if (!argc)
    return -1;
  if (!argv)
    return -1;

  CVI_RT_Init(&rt_handle);
  cvk_ctx = CVI_RT_RegisterKernel(rt_handle, CMDBUF_SIZE);
  if (!rt_handle || !cvk_ctx) {
    printf("%s fail\n", __FILENAME__);
    return -1;
  }


  ret |= test_tl_add_const(rt_handle, cvk_ctx, 0);
  ret |= test_tl_add_const(rt_handle, cvk_ctx, 1);

  CVI_RT_UnRegisterKernel(cvk_ctx);
  CVI_RT_DeInit(rt_handle);

  return ret;
}
