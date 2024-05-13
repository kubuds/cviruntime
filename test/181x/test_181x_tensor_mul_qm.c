#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>

#include "test_cvikernel_util.h"
#include "test_tf_quant_util.h"

// #define ENABLE_DEBUG_MSG

#define MIN_EXEC_TESTS  20

typedef struct {
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int relu_enable;
  int8_t *input1_data;
  int8_t *input2_data;
  int8_t *output_data;
  uint32_t multiplier;
  int8_t right_shift;
  float float_multiplier;
  int retry_cnt;
} elt_mul_test_param_t;

void elt_mul_ref(elt_mul_test_param_t *p_param)
{
  int input_n = p_param->input_n;
  int input_c = p_param->input_c;
  int input_h = p_param->input_h;
  int input_w = p_param->input_w;
  int32_t output_multiplier = p_param->multiplier;
  int8_t output_rshift = p_param->right_shift;
  int8_t *input1_data = p_param->input1_data;
  int8_t *input2_data = p_param->input2_data;
  int8_t *output_data = p_param->output_data;

  int32_t quantized_activation_min = -128;
  int32_t quantized_activation_max = 127;

  int size = input_n * input_c * input_h * input_w;
#ifdef ENABLE_DEBUG_MSG
  printf("elt_mul_ref:\n");
  printf("  shape (%d, %d, %d, %d)\n", input_n, input_c, input_h, input_w);
#endif
  for (int i = 0; i < size; ++i) {
    const int32_t input1_val = input1_data[i];
    const int32_t input2_val = input2_data[i];
    const int32_t unclamped_result = MultiplyByQuantizedMultiplier(
        input1_val * input2_val, output_multiplier, output_rshift);
    const int32_t clamped_output =
        MIN(quantized_activation_max,
                 MAX(quantized_activation_min, unclamped_result));

#ifdef ENABLE_DEBUG_MSG
    printf("  [%d] unclamped_result %d,  clamped_output %d\n", i,
           unclamped_result, clamped_output);
#endif

    output_data[i] = clamped_output;
  }
}

void calc_elt_mul_float_multiplier(elt_mul_test_param_t *p_param)
{
  int input_n = p_param->input_n;
  int input_c = p_param->input_c;
  int input_h = p_param->input_h;
  int input_w = p_param->input_w;
  int8_t *input1_data = p_param->input1_data;
  int8_t *input2_data = p_param->input2_data;

  int output_min = INT_MAX;
  int output_max = INT_MIN;

#ifdef ENABLE_DEBUG_MSG
  printf("calc_elt_mul_float_multiplier =>\n");
#endif

  int size = input_n * input_c * input_h * input_w;
  for (int i = 0; i < size; ++i) {
    const int32_t input1_val = input1_data[i];
    const int32_t input2_val = input2_data[i];

    const int32_t val = input1_val * input2_val;

    output_max = MAX(val, output_max);
    output_min = MIN(val, output_min);
  }

  // Since int8 ranges from -128 to 127, we need to squeeze the accumulator
  // MIN/MAX fit in those ranges correspondingly as much as possible.
  if (abs(output_max) > abs(output_min)) {
    p_param->float_multiplier = 127.0f / abs(output_max);
  } else {
    p_param->float_multiplier = 128.0f / abs(output_min);
  }

#ifdef ENABLE_DEBUG_MSG
  printf("  output_accu_min %d, output_accu_max %d, output_multiplier %f\n",
         output_min, output_max, p_param->float_multiplier);
#endif

#ifdef ENABLE_DEBUG_MSG
  printf("<= calc_elt_mul_float_multiplier\n");
#endif
}

int simple_test(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx)
{
  int ret = 0;

  // TFL: QuantizedMulOpTest.NoActivationInt8
  int size = 4;
  int8_t input1_data[4] = {-102, 25, 115, 89};
  int8_t input2_data[4] = {77, 51, 115, 102};
  int8_t ref_output_data[4] = {-62, 10, 104, 71};
  int8_t output_data[4];
  uint32_t output_multiplier = 1077952640;
  int8_t output_rshift = 6;  // change to right shift

  elt_mul_test_param_t test_param;
  memset(&test_param, 0, sizeof(test_param));

  test_param.input_n = 1;
  test_param.input_c = 1;
  test_param.input_h = 1;
  test_param.input_w = 4;
  test_param.input1_data = input1_data;
  test_param.input2_data = input2_data;
  test_param.output_data = output_data;
  test_param.multiplier = output_multiplier;
  test_param.right_shift = output_rshift;
  elt_mul_ref(&test_param);

  for (int i = 0; i < size; ++i) {
    if (output_data[i] != ref_output_data[i]) {
      printf("  Error ! output_data[%d] = %d != %d\n", i, output_data[i],
             ref_output_data[i]);
      ret = -1;
    }
  }

  cvk_tl_shape_t tl_shape = {1, 1, 1, size};
  cvk_tl_t *tl_a = cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, tl_shape, CVK_FMT_I8, /*align=*/1);
  cvk_tl_t *tl_b = cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, tl_shape, CVK_FMT_I8, /*align=*/1);
  cvk_tl_t *tl_res = cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, tl_shape, CVK_FMT_I8, /*align=*/1);

  tensor_copy_s2d_g2l(rt_handle, cvk_ctx, tl_a, (uint8_t *)input1_data);
  tensor_copy_s2d_g2l(rt_handle, cvk_ctx, tl_b, (uint8_t *)input2_data);

  {
    cvk_tiu_mul_qm_param_t p1;
    p1.res_high = NULL;
    p1.res_low = tl_res;
    p1.a = tl_a;
    p1.b_is_const = 0;
    p1.b = tl_b;
    p1.rshift_bits = output_rshift;
    p1.relu_enable = 0;
    p1.multiplier = output_multiplier;
    cvk_ctx->ops->tiu_mul_qm(cvk_ctx, &p1);
  }

  int8_t *res_tiu_data =
      (int8_t *)tensor_copy_l2g_d2s(rt_handle, cvk_ctx, tl_res);
  for (int i = 0; i < size; ++i) {
    if (res_tiu_data[i] != ref_output_data[i]) {
      printf("  Error ! result[%d] %d != %d\n", i, res_tiu_data[i],
             ref_output_data[i]);
      ret = -1;
    }
  }

  free(res_tiu_data);

  // Reserver order
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_res);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_b);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_a);

  return ret;
}

int choose_from_range(int table[], int size, int index)
{
  if (index >= size) {
    return 0;
  }

  int val = table[index];
  if (index < (size - 1)) {
    int range = MAX(table[index + 1] - table[index] - 1, 1);
    val += rand() % range;
  }

  return val;
}

bool check_valid_test_param(cvk_context_t *cvk_ctx, elt_mul_test_param_t *p_param)
{
  uint32_t input_n = p_param->input_n;
  uint32_t input_c = p_param->input_c;
  uint32_t input_h = p_param->input_h;
  uint32_t input_w = p_param->input_w;

  // input1, input2, output
  uint32_t total_needed_size = 3 * input_n * input_c * input_h * input_w;

  uint32_t lmem_size_per_lane = cvk_ctx->info.lmem_size;
  uint32_t total_lmem_size = cvk_ctx->info.lmem_size * cvk_ctx->info.npu_num;

  if (total_needed_size > total_lmem_size) {
    return false;
  }

  cvk_tl_shape_t input_shape = {input_n, input_c, input_h, input_w};

  uint32_t needed_size =
      3 * cvk_ctx->ops->lmem_tensor_to_size(cvk_ctx, input_shape, CVK_FMT_I8, /*eu_align=*/1);

  // Skip invalid shape
  if (needed_size > lmem_size_per_lane) {
    return false;
  }

  return true;
}

void fill_random_data_s8(int8_t *input_data, int size)
{
  for (int i = 0; i < size; ++i) {
    int is_satured = ((rand() % 1000) == 1) ? 1 : 0;
    int is_sign = rand() % 2 ? 1 : -1;

    if (is_satured && is_sign) {
      input_data[i] = -128;
    } else if (is_satured) {
      input_data[i] = 127;
    } else {
      input_data[i] = is_sign * rand() % 128;
    }
  }
}

void dump_test_param(elt_mul_test_param_t *p_param, bool dump_content)
{
  printf("Dump test parameter:\n");
  printf("  input_n %d\n", p_param->input_n);
  printf("  input_c %d\n", p_param->input_c);
  printf("  input_h %d\n", p_param->input_h);
  printf("  input_w %d\n", p_param->input_w);
  printf("  multiplier %d\n", p_param->multiplier);
  printf("  right_shift %d\n", p_param->right_shift);

  if (dump_content) {
    printf("input1_data(%d, %d, %d, %d) :\n", p_param->input_n,
           p_param->input_c, p_param->input_h, p_param->input_w);
    int in = p_param->input_n;
    int ic = p_param->input_c;
    int ih = p_param->input_h;
    int iw = p_param->input_w;
    for (int i = 0; i < in; ++i) {
      for (int j = 0; j < ic; ++j) {
        for (int k = 0; k < ih; ++k) {
          for (int l = 0; l < iw; ++l) {
            int offset = i * (ic * ih * iw) + j * (ih * iw) + k * iw + l;
            printf("%d, ", p_param->input1_data[offset]);
          }
          printf("\n");
        }
      }
    }
    printf("\n\n");

    printf("input2_data(%d, %d, %d, %d) :\n", p_param->input_n,
           p_param->input_c, p_param->input_h, p_param->input_w);
    for (int i = 0; i < in; ++i) {
      for (int j = 0; j < ic; ++j) {
        for (int k = 0; k < ih; ++k) {
          for (int l = 0; l < iw; ++l) {
            int offset = i * (ic * ih * iw) + j * (ih * iw) + k * iw + l;
            printf("%d, ", p_param->input2_data[offset]);
          }
          printf("\n");
        }
      }
    }
    printf("\n\n");
  }
}

int run_compare_elt_mul(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx,
                        elt_mul_test_param_t *p_param)
{
  int ret = 0;

  int input_n = p_param->input_n;
  int input_c = p_param->input_c;
  int input_h = p_param->input_h;
  int input_w = p_param->input_w;

  int input_size = input_n * input_c * input_h * input_w;
  int8_t *input1_data = (int8_t *)malloc(input_size);
  int8_t *input2_data = (int8_t *)malloc(input_size);
  int8_t *output_data = (int8_t *)malloc(input_size);

  p_param->input1_data = input1_data;
  p_param->input2_data = input2_data;
  p_param->output_data = output_data;

#ifdef ENABLE_DEBUG_MSG
  printf("    run_compare_elt_mul => \n");
#endif

  int retry_cnt = p_param->retry_cnt;
  do {
    fill_random_data_s8(input1_data, input_size);
    fill_random_data_s8(input2_data, input_size);

    p_param->float_multiplier = 100.0;  // should be < 1.0
    calc_elt_mul_float_multiplier(p_param);

    if (p_param->float_multiplier > 0.f && p_param->float_multiplier < 1.0) {
      break;
    }

  } while (--retry_cnt);

  if (p_param->float_multiplier >= 1.0) {
    printf("    run_compare_elt_mul: unable to find valid multiplier\n");
    free(input1_data);
    free(input2_data);
    free(output_data);
    return -1;
  }

  uint32_t base_multiplier = 0;
  int base_shift = 0;
  QuantizeMultiplierSmallerThanOne(p_param->float_multiplier, &base_multiplier,
                                   &base_shift);

  // multipliers typically range in [2^30 ; 2^31 - 1].
  // Values in [0, 2^30 - 1] are normally unused, but harmless.
  // Thus a good way to randomize multipliers is to subtract from them
  // a random value smaller than 2^30 but still significant compared to it.
  uint32_t output_multiplier = base_multiplier - (rand() % (1 << 26));

  int right_shift = base_shift - 1 + (rand() % 4);
  int8_t output_right_shift = truncate_rshift((int8_t)right_shift, /*allow_lshift*/1);

#ifdef ENABLE_DEBUG_MSG
  printf("      multiplier_data %d, shift_data %d\n", output_multiplier,
         output_right_shift);
#endif

  p_param->multiplier = output_multiplier;
  p_param->right_shift = output_right_shift;

  elt_mul_ref(p_param);

  cvk_tl_shape_t input_shape = {input_n, input_c, input_h, input_w};

  cvk_tl_t *tl_input1 =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, input_shape, CVK_FMT_I8, /*eu_aign=*/1);

  cvk_tl_t *tl_input2 =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, input_shape, CVK_FMT_I8, /*eu_aign=*/1);

  cvk_tl_t *tl_output =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, input_shape, CVK_FMT_I8, /*eu_aign=*/1);

  if (tl_input1 == NULL) {
    printf("    fail to alloc tl_input1 (%d, %d, %d, %d)\n", input_n, input_c,
           input_h, input_w);
    return -1;
  }
  if (tl_input2 == NULL) {
    printf("    fail to alloc tl_input2 (%d, %d, %d, %d)\n", input_n, input_c,
           input_h, input_w);
    return -1;
  }
  if (tl_output == NULL) {
    printf("    fail to alloc tl_output (%d, %d, %d, %d)\n", input_n, input_c,
           input_h, input_w);
    return -1;
  }

  tensor_copy_s2d_g2l(rt_handle, cvk_ctx, tl_input1, (uint8_t *)input1_data);
  tensor_copy_s2d_g2l(rt_handle, cvk_ctx, tl_input2, (uint8_t *)input2_data);

  {
    cvk_tiu_mul_qm_param_t p1;
    p1.res_high = NULL;
    p1.res_low = tl_output;
    p1.a = tl_input1;
    p1.b_is_const = 0;
    p1.b = tl_input2;
    p1.rshift_bits = (uint8_t)output_right_shift;
    p1.relu_enable = 0;
    p1.multiplier = output_multiplier;
    cvk_ctx->ops->tiu_mul_qm(cvk_ctx, &p1);
  }


  CVI_RT_Submit(cvk_ctx);

#ifdef ENABLE_DEBUG_MSG
  printf("      compare result:\n");
#endif
  int8_t *tiu_output_data =
      (int8_t *)tensor_copy_l2g_d2s(rt_handle, cvk_ctx, tl_output);
  for (int i = 0; i < input_n; ++i) {
    for (int j = 0; j < input_c; ++j) {
      for (int k = 0; k < input_h; ++k) {
        for (int l = 0; l < input_w; ++l) {
          int offset = i * (input_c * input_h * input_w) +
                       j * (input_h * input_w) + k * input_w + l;
          if (tiu_output_data[offset] != output_data[offset]) {
            printf("        [ni=%d][oci=%d][ohi=%d][owi=%d] output %d(tiu) != "
                   "%d(ref)\n",
                   i, j, k, l, tiu_output_data[offset], output_data[offset]);
            ret = -1;
            break;
          }
        }
      }
    }
  }

  free(tiu_output_data);

  if (ret) {
    dump_test_param(p_param, /*dump_content=*/true);
  }

  // Reverse order
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_output);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_input2);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_input1);

  free(input1_data);
  free(input2_data);
  free(output_data);

#ifdef ENABLE_DEBUG_MSG
  printf("    <= run_compare_elt_mul, ret %d\n", ret);
#endif

  return ret;
}

int random_test(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx)
{
  int ret = 0;

#if 0
  int input_n_range[] = {1};
  int input_c_range[] = {1};
  int input_h_range[] = {1};
  int input_w_range[] = {1};
#else
  int input_n_range[] = {1,   2, 4095 - 32};
  int input_c_range[] = {1, 512, 4095 - 32};
  int input_h_range[] = {1, 512, 4095 - 32};
  int input_w_range[] = {1, 512, 4095 - 32};
#endif

  const int input_n_range_size =
      sizeof(input_n_range) / sizeof(input_n_range[0]);
  const int input_c_range_size =
      sizeof(input_c_range) / sizeof(input_c_range[0]);
  const int input_h_range_size =
      sizeof(input_h_range) / sizeof(input_h_range[0]);
  const int input_w_range_size =
      sizeof(input_w_range) / sizeof(input_w_range[0]);

  int random_seed = clock();
  srand(random_seed);

  const int retry_test_count = 100;
  bool stop_at_first_error = true;

  int executed_tests = 0;
  int failed_tests = 0;

  printf("1822-mul-qm: random Test =>\n");
  for (int m = 0; m < retry_test_count; ++m) {
    for (int i = 0; i < input_n_range_size; ++i) {
      int input_n = choose_from_range(input_n_range, input_n_range_size, i);

      for (int j = 0; j < input_c_range_size; ++j) {
        int input_c = choose_from_range(input_c_range, input_c_range_size, j);

        for (int k = 0; k < input_h_range_size; ++k) {
          int input_h = choose_from_range(input_h_range, input_h_range_size, k);

          for (int l = 0; l < input_w_range_size; ++l) {
            int input_w =
                choose_from_range(input_w_range, input_w_range_size, l);

            elt_mul_test_param_t test_param;
            memset(&test_param, 0, sizeof(test_param));
            test_param.input_n = input_n;
            test_param.input_c = input_c;
            test_param.input_h = input_h;
            test_param.input_w = input_w;
            test_param.retry_cnt = 5;

            bool is_valid_param = check_valid_test_param(cvk_ctx, &test_param);
            if (is_valid_param == false)
              continue;

            int ret2 = run_compare_elt_mul(rt_handle, cvk_ctx, &test_param);
            failed_tests = ret2 ? failed_tests + 1 : failed_tests;
            ret |= ret2;
            executed_tests++;

#ifdef ENABLE_DEBUG_MSG
            printf("  [%d] random test: input shape (%d, %d, %d, %d), ret %d\n",
                   executed_tests, current_testinput_n, input_c, input_h, input_w, ret2);
#endif
          }

          // Stop at first error
          if (ret && stop_at_first_error) {
            break;
          }
        }

        // Stop at first error
        if (ret && stop_at_first_error) {
          break;
        }
      }

      // Stop at first error
      if (ret && stop_at_first_error) {
        break;
      }
    }

    // Stop at first error
    if (ret && stop_at_first_error) {
      break;
    }

    if (executed_tests >= MIN_EXEC_TESTS) {
      break;
    }
  }

  printf("<= 1822-mul-qm: random test, total %d, failed %d, ret %d\n",
         executed_tests, failed_tests, ret);

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

  ret = simple_test(rt_handle, cvk_ctx);
  ret |= random_test(rt_handle, cvk_ctx);

  CVI_RT_UnRegisterKernel(cvk_ctx);
  CVI_RT_DeInit(rt_handle);

  return ret;
}
