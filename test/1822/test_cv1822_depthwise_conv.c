#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <time.h>
#include "test_cvikernel_util.h"
#include "test_tf_quant_util.h"
#include "test_native_ref.h"

#define TEST_CASE_NAME    "test_cv1822_dw_conv"

// #define ENABLE_DEBUG_MSG
// #define ENABLE_FULL_REGRESSION
// #define ENABLE_TV_GEN_PATTERN

#define MIN_EXEC_TESTS  20

typedef struct {
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kw;
  int kh;
  int dh;
  int dw;
  int pad_top;
  int pad_bot;
  int pad_left;
  int pad_right;
  int ins_h;
  int ins_h_last;
  int ins_w;
  int ins_w_last;
  int stride_h;
  int stride_w;
  int output_c;
  int output_h;
  int output_w;
  int has_bias;
  int relu_enable;
  int8_t *input_data;
  int8_t *filter_data;
  int8_t *output_data;
  int32_t *bias_data;
  uint32_t *multiplier_data;
  int8_t *shift_data;
  float float_multiplier;
  int retry_cnt;
} dw_conv_test_param_t;


static inline int Offset(cvk_tl_shape_t shape, int i0, int i1, int i2, int i3)
{
  // return n * (shape.c * shape.h * shape.w) + c * (shape.h * shape.w) + h *
  // shape.w + w;
  int dims_data[4] = {shape.n, shape.c, shape.h, shape.w};
  return ((i0 * dims_data[1] + i1) * dims_data[2] + i2) * dims_data[3] + i3;
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

void fill_random_data_s32(int32_t *input_data, int size)
{
  for (int i = 0; i < size; ++i) {
    int is_satured = ((rand() % 1000) == 1) ? 1 : 0;
    int is_sign = rand() % 2 ? 1 : -1;

    if (is_satured && is_sign) {
      input_data[i] = INT_MIN;
    } else if (is_satured) {
      input_data[i] = INT_MAX;
    } else {
      input_data[i] = is_sign * rand() % 128;
    }
  }
}

void convert_nhwc_to_nchw(cvk_tl_shape_t tl_shape, int8_t *src, int8_t *dst)
{
  // NHWC
  uint32_t src_shape_n = tl_shape.n;
  uint32_t src_shape_h = tl_shape.c;
  uint32_t src_shape_w = tl_shape.h;
  uint32_t src_shape_c = tl_shape.w;
  uint32_t src_stride_c = 1;
  uint32_t src_stride_w = src_shape_c * src_stride_c;
  uint32_t src_stride_h = src_shape_w * src_stride_w;
  uint32_t src_stride_n = src_shape_h * src_stride_h;

  // NCHW
  // uint32_t dst_shape_n = src_shape_n;
  uint32_t dst_shape_c = src_shape_c;
  uint32_t dst_shape_h = src_shape_h;
  uint32_t dst_shape_w = src_shape_w;
  uint32_t dst_stride_w = 1;
  uint32_t dst_stride_h = dst_shape_w * dst_stride_w;
  uint32_t dst_stride_c = dst_shape_h * dst_stride_h;
  uint32_t dst_stride_n = dst_shape_c * dst_stride_c;

  printf("convert_nhwc_to_nchw:\n");
  printf("  src shape (%d, %d, %d, %d), stride (%d, %d, %d, %d)\n", src_shape_n,
         src_shape_c, src_shape_h, src_shape_w, src_stride_n, src_stride_c,
         src_stride_h, src_stride_w);
  printf("  dst shape (%d, %d, %d, %d), stride (%d, %d, %d, %d)\n", src_shape_n,
         dst_shape_c, dst_shape_h, dst_shape_w, dst_stride_n, dst_stride_c,
         dst_stride_h, dst_stride_w);

  for (uint32_t i = 0; i < src_shape_n; ++i) {
    for (uint32_t j = 0; j < src_shape_h; ++j) {
      for (uint32_t k = 0; k < src_shape_w; ++k) {
        for (uint32_t l = 0; l < src_shape_c; ++l) {
          uint32_t src_offset = i * src_stride_n + j * src_stride_h +
                           k * src_stride_w + l * src_stride_c;
          uint32_t dst_offset = i * dst_stride_n + j * dst_stride_h +
                           k * dst_stride_w + l * dst_stride_c;
          dst[dst_offset] = src[src_offset];
        }
      }
    }
  }
}

int test_nhwc_to_nchw(void)
{
  int ret = 0;

  cvk_tl_shape_t shape = {2, 2, 2, 2};
  int size = shape.n * shape.c * shape.h * shape.w;

  int8_t src[2 * 2 * 2 * 2] = {1,  5,  2,  6,  3,  7,  4,  8,
                           11, 15, 12, 16, 13, 17, 14, 18};

  int8_t dst[2 * 2 * 2 * 2] = {0};
  int8_t ref_dst[2 * 2 * 2 * 2] = {1,  2,  3,  4,  5,  6,  7,  8,
                               11, 12, 13, 14, 15, 16, 17, 18};

  convert_nhwc_to_nchw(shape, src, dst);
  for (int i = 0; i < size; ++i) {
    if (dst[i] != ref_dst[i]) {
      printf("Error ! dst[%d] %d != %d(expected)\n", i, dst[i], ref_dst[i]);
      ret = -1;
    }
  }

  cvk_tl_shape_t input_shape = {/*n=*/1, /*h=*/5, /*w=*/6, /*c=*/8};
  int input_size =
      input_shape.n * input_shape.c * input_shape.h * input_shape.w;
  int8_t nhwc_input_data[240] = {
      103,  85,   -96,  120,  105,  -72,  33,   -50,  -104, 12,   -57,  -80,
      12,   126,  117,  127,  119,  119,  -88,  57,   120,  123,  117,  -100,
      -4,   76,   76,   -52,  -92,  -127, -21,  -100, 106,  35,   74,   96,
      117,  0,    39,   76,   -119, -36,  89,   -74,  111,  46,   45,   -26,
      65,   61,   62,   -7,   -28,  -20,  39,   -84,  -85,  -51,  52,   76,
      -120, -47,  -58,  95,   -117, -90,  -104, 126,  82,   82,   49,   -96,
      -47,  67,   115,  -3,   -120, 41,   -16,  -96,  -31,  -75,  67,   -115,
      75,   -119, -81,  -24,  -3,   -11,  -14,  -4,   37,   75,   53,   107,
      65,   78,   -58,  52,   46,   -128, 39,   53,   -87,  36,   -98,  -12,
      -1,   70,   117,  18,   -41,  96,   21,   78,   -71,  -124, 64,   82,
      -63,  82,   1,    112,  50,   -23,  100,  -20,  117,  20,   12,   -88,
      -93,  67,   -90,  -70,  -63,  79,   87,   125,  -63,  -43,  80,   -52,
      -66,  -125, 109,  -73,  -39,  104,  -78,  89,   -64,  116,  29,   71,
      -7,   124,  -38,  -111, 84,   75,   21,   24,   12,   59,   106,  49,
      -55,  46,   65,   -28,  64,   15,   -31,  -75,  17,   7,    -109, -25,
      -115, -38,  7,    23,   71,   -37,  111,  119,  -95,  -89,  17,   -27,
      -8,   -29,  -125, 58,   -42,  -29,  -87,  109,  75,   -17,  -49,  92,
      7,    30,   -86,  -98,  26,   -8,   -61,  -41,  39,   7,    48,   55,
      63,   125,  -13,  56,   -107, 105,  -70,  1,    105,  14,   -89,  0,
      83,   -10,  9,    11,   127,  -14,  -108, 90,   -15,  26,   -101, -1};
  int8_t input_data[240];
  convert_nhwc_to_nchw(input_shape, nhwc_input_data, input_data);
  printf("NCHW input_data[%d] = {\n", input_size);
  for (int i = 0; i < input_size; ++i) {
    printf("%d, ", input_data[i]);
    if (i && ((i % 16) == 0)) {
      printf("\n");
    }
  }
  printf("};\n\n");

  cvk_tl_shape_t filter_shape = {1, 3, 3, 8};
  int filter_size =
      filter_shape.n * filter_shape.c * filter_shape.h * filter_shape.w;
  int8_t nhwc_filter_data[72] = {
      103,  85,  -96, 120, 105,  -72,  33,   -50,  -104, 12,  -57, -80,
      12,   126, 117, 127, 119,  119,  -88,  57,   120,  123, 117, -100,
      -4,   76,  76,  -52, -92,  -127, -21,  -100, 106,  35,  74,  96,
      117,  0,   39,  76,  -119, -36,  89,   -74,  111,  46,  45,  -26,
      65,   61,  62,  -7,  -28,  -20,  39,   -84,  -85,  -51, 52,  76,
      -120, -47, -58, 95,  -117, -90,  -104, 126,  82,   82,  49,  -96};
  int8_t filter_data[72];
  convert_nhwc_to_nchw(filter_shape, nhwc_filter_data, filter_data);
  printf("NCHW filter_data[%d] = {\n", filter_size);
  for (int i = 0; i < filter_size; ++i) {
    printf("%d, ", filter_data[i]);
    if (i && ((i % 16) == 0)) {
      printf("\n");
    }
  }
  printf("}\n\n");

  cvk_tl_shape_t output_shape = {1, 3, 4, 8};
  int output_size =
      output_shape.n * output_shape.c * output_shape.h * output_shape.w;
  int8_t nhwc_output_data[96] = {
      127,  127,  69,   34,  36,   127,  127,  127,  -101, -65,  39,   13,
      26,   6,    127,  -67, 60,   123,  31,   17,   3,    -128, -58,  -64,
      -128, 26,   -128, -21, 72,   55,   127,  94,   -46,  -128, -37,  1,
      -6,   109,  98,   -14, -11,  48,   -128, -3,   -50,  37,   -20,  79,
      -94,  -36,  127,  19,  3,    -18,  -40,  -115, 24,   124,  -128, -1,
      -52,  -123, -54,  -1,  -62,  95,   127,  24,   10,   -74,  127,  -128,
      -2,   111,  106,  4,   3,    -128, 127,  127,  -30,  98,   -21,  -1,
      -11,  -12,  58,   -72, -128, 127,  30,   32,   -85,  -11,  -35,  34};
  int8_t output_data[96] = {0};
  convert_nhwc_to_nchw(output_shape, nhwc_output_data, output_data);
  printf("NCHW output_data[%d] = {\n", output_size);
  for (int i = 0; i < output_size; ++i) {
    printf("%d, ", output_data[i]);
    if (i && ((i % 16) == 0)) {
      printf("\n");
    }
  }
  printf("};\n\n");

  return ret;
}

int simple_nhwc_dw_conv_test(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx)
{
  int ret = 0;

  const int stride_width = 1;
  const int stride_height = 1;
  const int dilation_width_factor = 1;
  const int dilation_height_factor = 1;
  const int pad_width = 0;
  const int pad_height = 0;
  const int depth_multiplier = 1;
  const int input_offset = 0;   // symmetric
  const int output_offset = 0;  // symmetric
  const int output_activation_min = -128;
  const int output_activation_max = 127;

  if (rt_handle == NULL) {
    return -1;
  }
  if (cvk_ctx == NULL) {
    return -1;
  }

  cvk_tl_shape_t input_shape = {/*n=*/1, /*h=*/5, /*w=*/6, /*c=*/8};
  int8_t input_data[240] = {
      103,  85,   -96,  120,  105,  -72,  33,   -50,  -104, 12,   -57,  -80,
      12,   126,  117,  127,  119,  119,  -88,  57,   120,  123,  117,  -100,
      -4,   76,   76,   -52,  -92,  -127, -21,  -100, 106,  35,   74,   96,
      117,  0,    39,   76,   -119, -36,  89,   -74,  111,  46,   45,   -26,
      65,   61,   62,   -7,   -28,  -20,  39,   -84,  -85,  -51,  52,   76,
      -120, -47,  -58,  95,   -117, -90,  -104, 126,  82,   82,   49,   -96,
      -47,  67,   115,  -3,   -120, 41,   -16,  -96,  -31,  -75,  67,   -115,
      75,   -119, -81,  -24,  -3,   -11,  -14,  -4,   37,   75,   53,   107,
      65,   78,   -58,  52,   46,   -128, 39,   53,   -87,  36,   -98,  -12,
      -1,   70,   117,  18,   -41,  96,   21,   78,   -71,  -124, 64,   82,
      -63,  82,   1,    112,  50,   -23,  100,  -20,  117,  20,   12,   -88,
      -93,  67,   -90,  -70,  -63,  79,   87,   125,  -63,  -43,  80,   -52,
      -66,  -125, 109,  -73,  -39,  104,  -78,  89,   -64,  116,  29,   71,
      -7,   124,  -38,  -111, 84,   75,   21,   24,   12,   59,   106,  49,
      -55,  46,   65,   -28,  64,   15,   -31,  -75,  17,   7,    -109, -25,
      -115, -38,  7,    23,   71,   -37,  111,  119,  -95,  -89,  17,   -27,
      -8,   -29,  -125, 58,   -42,  -29,  -87,  109,  75,   -17,  -49,  92,
      7,    30,   -86,  -98,  26,   -8,   -61,  -41,  39,   7,    48,   55,
      63,   125,  -13,  56,   -107, 105,  -70,  1,    105,  14,   -89,  0,
      83,   -10,  9,    11,   127,  -14,  -108, 90,   -15,  26,   -101, -1};

  cvk_tl_shape_t filter_shape = {1, 3, 3, 8};
  int8_t filter_data[72] = {
      103,  85,  -96, 120, 105,  -72,  33,   -50,  -104, 12,  -57, -80,
      12,   126, 117, 127, 119,  119,  -88,  57,   120,  123, 117, -100,
      -4,   76,  76,  -52, -92,  -127, -21,  -100, 106,  35,  74,  96,
      117,  0,   39,  76,  -119, -36,  89,   -74,  111,  46,  45,  -26,
      65,   61,  62,  -7,  -28,  -20,  39,   -84,  -85,  -51, 52,  76,
      -120, -47, -58, 95,  -117, -90,  -104, 126,  82,   82,  49,  -96};

  int32_t bias_data[8] = {812, 670, -746, 938, 827, -558, 265, -384};

  uint32_t output_multiplier[8] = {1155460505, 1210948247, 1203328687, 1166122678,
                              1155273687, 1196350022, 1169748238, 1183287581};

  int8_t output_rshift[8] = {-7, -6, -6, -9, -8, -6, -6, -7};

  cvk_tl_shape_t output_shape = {1, 3, 4, 8};
  int8_t output_data[96] = {0};
  int8_t ref_output_data[96] = {
      127,  127,  69,   34,  36,   127,  127,  127,  -101, -65,  39,   13,
      26,   6,    127,  -67, 60,   123,  31,   17,   3,    -128, -58,  -64,
      -128, 26,   -128, -21, 72,   55,   127,  94,   -46,  -128, -37,  1,
      -6,   109,  98,   -14, -11,  48,   -128, -3,   -50,  37,   -20,  79,
      -94,  -36,  127,  19,  3,    -18,  -40,  -115, 24,   124,  -128, -1,
      -52,  -123, -54,  -1,  -62,  95,   127,  24,   10,   -74,  127,  -128,
      -2,   111,  106,  4,   3,    -128, 127,  127,  -30,  98,   -21,  -1,
      -11,  -12,  58,   -72, -128, 127,  30,   32,   -85,  -11,  -35,  34};

  const int batches = input_shape.n;
  // const int output_depth = 8;
  const int input_height = input_shape.c;
  const int input_width = input_shape.h;
  const int input_depth = input_shape.w;
  const int filter_height = filter_shape.c;
  const int filter_width = filter_shape.h;
  const int output_height = output_shape.c;
  const int output_width = output_shape.h;

  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
          for (int m = 0; m < depth_multiplier; ++m) {
            const int output_channel = m + in_channel * depth_multiplier;
            const int in_x_origin = (out_x * stride_width) - pad_width;
            const int in_y_origin = (out_y * stride_height) - pad_height;
            int32_t acc = 0;
            for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
              for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
                const int in_x = in_x_origin + dilation_width_factor * filter_x;
                const int in_y =
                    in_y_origin + dilation_height_factor * filter_y;
                // Zero padding by omitting the areas outside the image.
                const bool is_point_inside_image =
                    (in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height);
                if (is_point_inside_image) {
                  int32_t input_val = input_data[Offset(input_shape, batch, in_y,
                                                    in_x, in_channel)];
                  int32_t filter_val = filter_data[Offset(
                      filter_shape, 0, filter_y, filter_x, output_channel)];
                  acc += filter_val * (input_val + input_offset);

                  printf("  [batch=%d][out_y=%d][out_x=%d][in_channel=%d][m=%d]"
                         "[filter_y=%d][filter_x=%d] acc(%d) += %d * (%d + %d) "
                         "= %d\n",
                         batch, out_y, out_x, in_channel, m, filter_y, filter_x,
                         acc - filter_val * (input_val + input_offset),
                         filter_val, input_val, input_offset, acc);
                }
              }
            }
            if (1 /*bias_data*/) {
              acc += bias_data[output_channel];
            }

            printf("  [batch=%d][out_y=%d][out_x=%d][output_channel=%d] acc = "
                   "%d, bias %d\n",
                   batch, out_y, out_x, output_channel, acc,
                   bias_data[output_channel]);

            acc = MultiplyByQuantizedMultiplier(
                acc, output_multiplier[output_channel],
                output_rshift[output_channel]);

            printf("  [batch=%d][out_y=%d][out_x=%d][output_channel=%d] acc = "
                   "%d, multiplier %d, shift %d\n",
                   batch, out_y, out_x, output_channel, acc,
                   output_multiplier[output_channel],
                   output_rshift[output_channel]);

            acc += output_offset;
            acc = MAX(acc, output_activation_min);
            acc = MIN(acc, output_activation_max);

            printf("  [batch=%d][out_y=%d][out_x=%d][output_channel=%d] acc = "
                   "%d\n",
                   batch, out_y, out_x, output_channel, acc);

            {
              int x = Offset(output_shape, batch, out_y, out_x, output_channel);
              if (x >= 96) {
                printf("Error ! shape=(%d, %d, %d, %d), batch %d, out_y %d, "
                       "out_x %d, output_channel %d, offset %d\n",
                       output_shape.n, output_shape.c, output_shape.h,
                       output_shape.w, batch, out_y, out_x, output_channel, x);
              }
            }

            output_data[Offset(output_shape, batch, out_y, out_x,
                               output_channel)] = acc;
          }
        }
      }
    }
  }

  int output_size =
      output_shape.n * output_shape.c * output_shape.h * output_shape.w;
  for (int i = 0; i < output_size; ++i) {
    if (output_data[i] != ref_output_data[i]) {
      printf("  output_data[%d] = %d != %d\n", i, output_data[i],
             ref_output_data[i]);
      ret = -1;
    }
  }

  return ret;
}

typedef struct {
  int stride_width;
  int stride_height;
  int dilation_width_factor;
  int dilation_height_factor;
  int padding_width;
  int padding_height;
  int depth_multiplier;
} DwConvParams;

void dw_conv_per_channel_ref(const dw_conv_test_param_t *p_param)
{
  const int input_offset = 0;   // symmetric
  const int output_offset = 0;  // symmetric
  const int output_activation_min = -128;
  const int output_activation_max = 127;

  const int stride_width = p_param->stride_w;
  const int stride_height = p_param->stride_h;
  const int dilation_width_factor = 1;   // params.dilation_width_factor;
  const int dilation_height_factor = 1;  // params.dilation_height_factor;
  const int pad_width = p_param->pad_left;
  const int pad_height = p_param->pad_top;
  const int depth_multiplier = 1;  // params.depth_multiplier;

  const int batches = p_param->input_n;
  const int input_height = p_param->input_h;
  const int input_width = p_param->input_w;
  const int input_depth = p_param->input_c;
  const int filter_height = p_param->kh;
  const int filter_width = p_param->kw;
  const int output_depth = p_param->output_c;
  const int output_height = p_param->output_h;
  const int output_width = p_param->output_w;
  int8_t *input_data = p_param->input_data;
  int8_t *filter_data = p_param->filter_data;
  int8_t *output_data = p_param->output_data;
  int32_t *bias_data = p_param->has_bias ? p_param->bias_data : NULL;
  uint32_t *output_multiplier = p_param->multiplier_data;
  int8_t *output_rshift = p_param->shift_data;

  cvk_tl_shape_t input_shape = {
      batches, input_depth, input_height, input_width};
  cvk_tl_shape_t filter_shape = {
      output_depth, input_depth, filter_height, filter_width};
  cvk_tl_shape_t output_shape = {
      batches, output_depth, output_height, output_width};

#ifdef ENABLE_DEBUG_MSG
  printf("dw_conv_per_channel_ref =>\n");
  printf("  input shape (n=%d, c=%d, h=%d, w=%d)\n", batches, input_depth,
         input_height, input_width);
  // printf("  filter shape (oc=%d, kh=%d, kw=%d\n",
  //       );
  printf("  output shape (n=%d, c=%d, h=%d, w=%d)\n", batches, output_depth,
         output_height, output_width);
  printf("  stride_h %d, stride_w %d\n", stride_height, stride_width);
#endif

  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
          for (int m = 0; m < depth_multiplier; ++m) {
            const int output_channel = m + in_channel * depth_multiplier;
            const int in_x_origin = (out_x * stride_width) - pad_width;
            const int in_y_origin = (out_y * stride_height) - pad_height;
            int32_t acc = 0;
            for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
              for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
                const int in_x = in_x_origin + dilation_width_factor * filter_x;
                const int in_y =
                    in_y_origin + dilation_height_factor * filter_y;
                // Zero padding by omitting the areas outside the image.
                const bool is_point_inside_image =
                    (in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height);
                if (is_point_inside_image) {
                  int32_t input_val = input_data[Offset(input_shape, batch,
                                                    in_channel, in_y, in_x)];
                  int32_t filter_val = filter_data[Offset(
                      filter_shape, 0, output_channel, filter_y, filter_x)];
                  acc += filter_val * (input_val + input_offset);

#ifdef ENABLE_DEBUG_MSG
                  printf("  [batch=%d][out_y=%d][out_x=%d][in_channel=%d][m=%d]"
                         "[filter_y=%d][filter_x=%d] acc(%d) += %d * (%d + %d) "
                         "= %d, in_x_origin %d, in_x %d\n",
                         batch, out_y, out_x, in_channel, m, filter_y, filter_x,
                         acc - filter_val * (input_val + input_offset),
                         filter_val, input_val, input_offset, acc, in_x_origin,
                         in_x);
#endif
                }
              }
            }
            if (bias_data) {
              acc += bias_data[output_channel];
            }

#ifdef ENABLE_DEBUG_MSG
            printf("  [batch=%d][out_y=%d][out_x=%d][output_channel=%d] acc = "
                   "%d, bias %d\n",
                   batch, out_y, out_x, output_channel, acc,
                   bias_data ? bias_data[output_channel] : 0);
#endif

            acc = MultiplyByQuantizedMultiplier(
                acc, output_multiplier[output_channel],
                output_rshift[output_channel]);

#ifdef ENABLE_DEBUG_MSG
            printf("  [batch=%d][out_y=%d][out_x=%d][output_channel=%d] acc = "
                   "%d, multiplier %d, shift %d\n",
                   batch, out_y, out_x, output_channel, acc,
                   output_multiplier[output_channel],
                   output_rshift[output_channel]);
#endif

            acc += output_offset;
            acc = MAX(acc, output_activation_min);
            acc = MIN(acc, output_activation_max);

#ifdef ENABLE_DEBUG_MSG
            printf("  [batch=%d][out_y=%d][out_x=%d][output_channel=%d] acc = "
                   "%d\n",
                   batch, out_y, out_x, output_channel, acc);
#endif

            output_data[Offset(output_shape, batch, output_channel, out_y,
                               out_x)] = acc;
          }
        }
      }
    }
  }

#ifdef ENABLE_DEBUG_MSG
  printf("<= dw_conv_per_channel_ref\n");
#endif
}

void calc_dw_conv_float_multiplier(dw_conv_test_param_t *p_param)
{
  const int input_offset = 0;  // symmetric

  const int stride_width = p_param->stride_w;
  const int stride_height = p_param->stride_h;
  const int dilation_width_factor = 1;   // params.dilation_width_factor;
  const int dilation_height_factor = 1;  // params.dilation_height_factor;
  const int pad_width = p_param->pad_left;
  ;
  const int pad_height = p_param->pad_top;
  const int depth_multiplier = 1;  // params.depth_multiplier;

  const int batches = p_param->input_n;
  const int input_height = p_param->input_h;
  const int input_width = p_param->input_w;
  const int input_depth = p_param->input_c;
  const int filter_height = p_param->kh;
  const int filter_width = p_param->kw;
  const int output_depth = p_param->output_c;
  const int output_height = p_param->output_h;
  const int output_width = p_param->output_w;
  int8_t *input_data = p_param->input_data;
  int8_t *filter_data = p_param->filter_data;
  int32_t *bias_data = p_param->has_bias ? p_param->bias_data : NULL;

  cvk_tl_shape_t input_shape = {
      batches, input_depth, input_height, input_width};
  cvk_tl_shape_t filter_shape = {
      output_depth, input_depth, filter_height, filter_width};

  int output_accu_min = INT_MAX;
  int output_accu_max = INT_MIN;

  // printf("calc_dw_conv_float_multiplier =>\n");
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
          for (int m = 0; m < depth_multiplier; ++m) {
            const int output_channel = m + in_channel * depth_multiplier;
            const int in_x_origin = (out_x * stride_width) - pad_width;
            const int in_y_origin = (out_y * stride_height) - pad_height;
            int32_t acc = 0;
            for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
              for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
                const int in_x = in_x_origin + dilation_width_factor * filter_x;
                const int in_y =
                    in_y_origin + dilation_height_factor * filter_y;
                // Zero padding by omitting the areas outside the image.
                const bool is_point_inside_image =
                    (in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height);
                if (is_point_inside_image) {
                  int32_t input_val = input_data[Offset(input_shape, batch,
                                                    in_channel, in_y, in_x)];
                  int32_t filter_val = filter_data[Offset(
                      filter_shape, 0, output_channel, filter_y, filter_x)];
                  acc += filter_val * (input_val + input_offset);

                  // printf("
                  // [batch=%d][out_y=%d][out_x=%d][in_channel=%d][m=%d]"
                  //        "[filter_y=%d][filter_x=%d] acc(%d) += %d * (%d +
                  //        %d) = %d\n",
                  //         batch, out_y, out_x, in_channel, m, filter_y,
                  //         filter_x, acc - filter_val * (input_val +
                  //         input_offset), filter_val, input_val, input_offset,
                  //         acc);
                }
              }
            }
            if (bias_data) {
              acc += bias_data[output_channel];
            }

            output_accu_max = MAX(acc, output_accu_max);
            output_accu_min = MIN(acc, output_accu_min);

            // printf("  [batch=%d][out_y=%d][out_x=%d][output_channel=%d] acc =
            // %d, MIN = %d, MAX = %d\n",
            //        batch, out_y, out_x, output_channel, acc,
            //        output_accu_min, output_accu_max);
          }
        }
      }
    }
  }

  // Since int8 ranges from -128 to 127, we need to squeeze the accumulator
  // MIN/MAX fit in those ranges correspondingly as much as possible.
  if (abs(output_accu_max) > abs(output_accu_min)) {
    p_param->float_multiplier = 127.0f / abs(output_accu_max);
  } else {
    p_param->float_multiplier = 128.0f / abs(output_accu_min);
  }

#ifdef ENABLE_DEBUG_MSG
  printf("  output_accu_min %d, output_accu_max %d, output_multiplier %f\n",
         output_accu_min, output_accu_max, p_param->float_multiplier);
#endif

  // printf("<= calc_dw_conv_float_multiplier\n");
}

static int simple_test(CVI_RT_HANDLE *rt_handler, cvk_context_t *cvk_ctx)
{
  int ret = 0;

  const int batches = 1;
  const int input_depth = 8;
  const int input_height = 5;
  const int input_width = 6;
  cvk_tl_shape_t input_shape =
      {batches, input_depth, input_height, input_width};
  int8_t input_data[240] = {
      /* ic = 0 */
      103, -104, 119, -4, 106, -119, 65, -85, -117, -47, -31, -3, 65, -87, -41,
      -63, 117, -63, -66, -64, 84, -55, 17, 71, -8, 75, 26, 63, 105, 127,

      /* ic = 1 */
      85, 12, 119, 76, 35, -36, 61, -51, -90, 67, -75, -11, 78, 36, 96, 82, 20,
      79, -125, 116, 75, 46, 7, -37, -29, -17, -8, 125, 14, -14,

      /* ic = 2 */
      -96, -57, -88, 76, 74, 89, 62, 52, -104, 115, 67, -14, -58, -98, 21, 1,
      12, 87, 109, 29, 21, 65, -109, 111, -125, -49, -61, -13, -89, -108,

      /* ic = 3 */
      120, -80, 57, -52, 96, -74, -7, 76, 126, -3, -115, -4, 52, -12, 78, 112,
      -88, 125, -73, 71, 24, -28, -25, 119, 58, 92, -41, 56, 0, 90,

      /* ic = 4 */
      105, 12, 120, -92, 117, 111, -28, -120, 82, -120, 75, 37, 46, -1, -71, 50,
      -93, -63, -39, -7, 12, 64, -115, -95, -42, 7, 39, -107, 83, -15,

      /* ic = 5 */
      -72, 126, 123, -127, 0, 46, -20, -47, 82, 41, -119, 75, -128, 70, -124,
      -23, 67, -43, 104, 124, 59, 15, -38, -89, -29, 30, 7, 105, -10, 26,

      /* ic = 6 */
      33, 117, 117, -21, 39, 45, 39, -58, 49, -16, -81, 53, 39, 117, 64, 100,
      -90, 80, -78, -38, 106, -31, 7, 17, -87, -86, 48, -70, 9, -101,

      /* ic = 7 */
      -50, 127, -100, -100, 76, -26, -84, 95, -96, -96, -24, 107, 53, 18, 82,
      -20, -70, -52, 89, -111, 49, -75, 23, -27, 109, -98, 55, 1, 11, -1};

  const int kernel_height = 3;
  const int kernel_width = 3;
  cvk_tl_shape_t filter_shape_tiu =
      {1, input_depth, kernel_height, kernel_width};
  cvk_tl_shape_t filter_shape_dma =
      {1, input_depth, kernel_height, kernel_width};
  // Global memory layout: OcKhKw
  int8_t filter_data[72] = {
      103,  -104, 119,  -4,  106, -119, 65,   -85,  -117, 85,  12,  119,
      76,   35,   -36,  61,  -51, -90,  -96,  -57,  -88,  76,  74,  89,
      62,   52,   -104, 120, -80, 57,   -52,  96,   -74,  -7,  76,  126,
      105,  12,   120,  -92, 117, 111,  -28,  -120, 82,   -72, 126, 123,
      -127, 0,    46,   -20, -47, 82,   33,   117,  117,  -21, 39,  45,
      39,   -58,  49,   -50, 127, -100, -100, 76,   -26,  -84, 95,  -96};

  int32_t bias_data[8] = {812, 670, -746, 938, 827, -558, 265, -384};

  uint32_t output_multiplier[8] = {
      1155460505, 1210948247, 1203328687, 1166122678,
      1155273687, 1196350022, 1169748238, 1183287581};

  // Change to right shift
  int8_t output_rshift[8] = {7, 6, 6, 9, 8, 6, 6, 7};

  uint8_t chl_quan_data[8 * 4 + 8 * 4 + 8];
  pack_chl_quan_param(8, /*has_bias=*/true, bias_data, output_multiplier,
                      output_rshift, chl_quan_data);

  const int output_height = 3;
  const int output_width = 4;
  cvk_tl_shape_t output_shape =
      {batches, input_depth, output_height, output_width};
  int8_t ref_output_data[96] = {
      /* oc = 0 */
      127, -101, 60, -128, -46, -11, -94, 24, -62, -2, -30, -128,

      /* oc = 1 */
      127, -65, 123, 26, -128, 48, -36, 124, 95, 111, 98, 127,

      /* oc = 2 */
      69, 39, 31, -128, -37, -128, 127, -128, 127, 106, -21, 30,

      /* oc = 3 */
      34, 13, 17, -21, 1, -3, 19, -1, 24, 4, -1, 32,

      /* oc = 4 */
      36, 26, 3, 72, -6, -50, 3, -52, 10, 3, -11, -85,

      /* oc = 5 */
      127, 6, -128, 55, 109, 37, -18, -123, -74, -128, -12, -11,

      /* oc = 6 */
      127, 127, -58, 127, 98, -20, -40, -54, 127, 127, 58, -35,

      /* oc = 7 */
      127, -67, -64, 94, -14, 79, -115, -1, -128, 127, -72, 34};

  cvk_tl_t *tl_input =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, input_shape, CVK_FMT_I8,
                                      /*eu_aign=*/1);

  cvk_tl_t *tl_filter_dma =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, filter_shape_dma, CVK_FMT_I8,
                                      /*eu_align=*/1);

  cvk_tl_t *tl_output =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, output_shape, CVK_FMT_I8,
                                      /*eu_align=*/1);

  cvk_tl_shape_t chl_quan_shape_dma = {1, 8, 1, 9};
  cvk_tl_shape_t chl_quan_shape_tiu = {1, 8, 1, 1};
  cvk_tl_t *tl_chl_quan_dma =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, chl_quan_shape_dma, CVK_FMT_U8,
                                      /*eu_align*/ 0);

  tensor_copy_s2d_g2l(rt_handler, cvk_ctx, tl_chl_quan_dma, chl_quan_data);
  tensor_copy_s2d_g2l(rt_handler, cvk_ctx, tl_input, (uint8_t *)input_data);
  tensor_copy_s2d_g2l(rt_handler, cvk_ctx, tl_filter_dma,
                           (uint8_t *)filter_data);

  {
   cvk_tl_t tl_filter_tiu;
    memset(&tl_filter_tiu, 0, sizeof(tl_filter_tiu));
    tl_filter_tiu.start_address = tl_filter_dma->start_address;
    tl_filter_tiu.fmt = tl_filter_dma->fmt;
    tl_filter_tiu.shape.n = filter_shape_tiu.n;
    tl_filter_tiu.shape.c = filter_shape_tiu.c;
    tl_filter_tiu.shape.h = filter_shape_tiu.h;
    tl_filter_tiu.shape.w = filter_shape_tiu.w;
    tl_filter_tiu.stride =
        cvk_ctx->ops->tl_default_stride(cvk_ctx, tl_filter_tiu.shape,
                                        CVK_FMT_I8, /*eu_align=*/1);

    cvk_tl_t tl_chl_quan_tiu;
    memset(&tl_chl_quan_tiu, 0, sizeof(tl_chl_quan_tiu));
    tl_chl_quan_tiu.start_address = tl_chl_quan_dma->start_address;
    tl_chl_quan_tiu.fmt = tl_chl_quan_dma->fmt;
    tl_chl_quan_tiu.shape.n = chl_quan_shape_tiu.n;
    tl_chl_quan_tiu.shape.c = chl_quan_shape_tiu.c;
    tl_chl_quan_tiu.shape.h = chl_quan_shape_tiu.h;
    tl_chl_quan_tiu.shape.w = chl_quan_shape_tiu.w;
    tl_chl_quan_tiu.stride = cvk_ctx->ops->tl_default_stride(
        cvk_ctx, tl_chl_quan_tiu.shape, CVK_FMT_I8, /*eu_align=*/0);

    cvk_tiu_depthwise_convolution_param_t param;
    memset(&param, 0, sizeof(param));
    param.ofmap = tl_output;
    param.ifmap = tl_input;
    param.weight = &tl_filter_tiu;
    param.chl_quan_param = &tl_chl_quan_tiu;
    param.dilation_h = 1;
    param.dilation_w = 1;
    param.stride_h = 1;
    param.stride_w = 1;
    param.has_bias = 1;
    cvk_ctx->ops->tiu_depthwise_convolution(cvk_ctx, &param);
  }

  CVI_RT_Submit(cvk_ctx);

  printf("dw-conv simple :compare tiu and golden\n");
  int8_t *conv_output_data =
      (int8_t *)tensor_copy_l2g_d2s(rt_handler, cvk_ctx, tl_output);
  for (int i = 0; i < (int)sizeof(ref_output_data); i++) {
    if (conv_output_data[i] != ref_output_data[i]) {
      printf("  output_data[%d] %d != %d\n", i, conv_output_data[i],
             ref_output_data[i]);
      ret = -1;
    }
  }

  free(conv_output_data);

  int8_t output_data[96] = {0};
  memset(output_data, 0, sizeof(output_data));

  dw_conv_test_param_t params;
  memset(&params, 0, sizeof(params));
  params.input_n = batches;
  params.input_c = input_depth;
  params.input_h = input_height;
  params.input_w = input_width;
  params.kh = kernel_height;
  params.kw = kernel_width;
  params.output_c = input_depth;
  params.output_h = output_height;
  params.output_w = output_width;
  params.stride_w = 1;
  params.stride_h = 1;
  params.input_data = input_data;
  params.filter_data = filter_data;
  params.output_data = output_data;
  params.has_bias = 1;
  params.bias_data = bias_data;
  params.multiplier_data = output_multiplier;
  params.shift_data = output_rshift;

  dw_conv_per_channel_ref(&params);

  printf("dw-conv simple: compare ref and golden\n");
  int output_size =
      output_shape.n * output_shape.c * output_shape.h * output_shape.w;
  for (int i = 0; i < output_size; ++i) {
    if (output_data[i] != ref_output_data[i]) {
      printf("  output_data[%d] = %d != %d\n", i, output_data[i],
             ref_output_data[i]);
      ret = -1;
    }
  }

  // Reverse order
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_chl_quan_dma);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_output);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_filter_dma);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_input);

  return ret;
}

#if 0
static int simple_lshift_test(CVI_RT_HANDLE *bm_ctx, cvk_context_t *cvk_ctx)
{
  int ret = 0;

  (void)bm_ctx;
  (void)cvk_ctx;

DepthwiseConvPerChannelTest:
  input_shape (1, 6, 5, 8)
  filter_shape (1, 3, 3, 8)
  output_shape (1, 6, 5, 8)
  input offset 0, output_offset 0,  output_multiplier 0x55663ec39de0, output_activation_min -128, output_activation_max 127
input_data[240] = {
105, -72, 33, -50, -104, 12, -57, -80, 12, 126, 117, 127, 119, 119, -88, 57, 120,
123, 117, -100, -4, 76, 76, -52, -92, -127, -21, -100, 106, 35, 74, 96, 117,
0, 39, 76, -119, -36, 89, -74, 111, 46, 45, -26, 65, 61, 62, -7, -28,
-20, 39, -84, -85, -51, 52, 76, -120, -47, -58, 95, -117, -90, -104, 126, 82,
82, 49, -96, -47, 67, 115, -3, -120, 41, -16, -96, -31, -75, 67, -115, 75,
-119, -81, -24, -3, -11, -14, -4, 37, 75, 53, 107, 65, 78, -58, 52, 46,
-128, 39, 53, -87, 36, -98, -12, -1, 70, 117, 18, -41, 96, 21, 78, -71,
-124, 64, 82, -63, 82, 1, 112, 50, -23, 100, -20, 117, 20, 12, -88, -93,
67, -90, -70, -63, 79, 87, 125, -63, -43, 80, -52, -66, -125, 109, -73, -39,
104, -78, 89, -64, 116, 29, 71, -7, 124, -38, -111, 84, 75, 21, 24, 12,
59, 106, 49, -55, 46, 65, -28, 64, 15, -31, -75, 17, 7, -109, -25, -115,
-38, 7, 23, 71, -37, 111, 119, -95, -89, 17, -27, -8, -29, -125, 58, -42,
-29, -87, 109, 75, -17, -49, 92, 7, 30, -86, -98, 26, -8, -61, -41, 39,
7, 48, 55, 63, 125, -13, 56, -107, 105, -70, 1, 105, 14, -89, 0, 83,
-10, 9, 11, 127, -14, -108, 90, -15, 26, -101, -1, 118, 122, -127, -120, };
filter_data[72] = {
105, -72, 33, -50, -104, 12, -57, -80, 12, 126, 117, 127, 119, 119, -88, 57, 120,
123, 117, -100, -4, 76, 76, -52, -92, -127, -21, -100, 106, 35, 74, 96, 117,
0, 39, 76, -119, -36, 89, -74, 111, 46, 45, -26, 65, 61, 62, -7, -28,
-20, 39, -84, -85, -51, 52, 76, -120, -47, -58, 95, -117, -90, -104, 126, 82,
82, 49, -96, -47, 67, 115, -3, };
bias_data[8] = {
827, -558, 265, -384, -805, 94, -443, -624, };
output_multiplier[8] = {
1610079514, 1623689331, 1597316687, 1626449885, 1593210715, 1596467067, 1646211112, 1593791522, };
output_shift[8] = {
9, -2, 7, -7, 4, 10, 8, -5, };

  return ret;
}
#endif

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

void dump_test_param(dw_conv_test_param_t *p_param, bool dump_content)
{
  printf("Dump test parameter:\n");
  printf("  input_n %d\n", p_param->input_n);
  printf("  input_c %d\n", p_param->input_c);
  printf("  input_h %d\n", p_param->input_h);
  printf("  input_w %d\n", p_param->input_w);
  printf("  kw %d\n", p_param->kw);
  printf("  kh %d\n", p_param->kh);
  printf("  dh %d\n", p_param->dh);
  printf("  dw %d\n", p_param->dw);
  printf("  pad_top %d\n", p_param->pad_top);
  printf("  pad_bot %d\n", p_param->pad_bot);
  printf("  pad_left %d\n", p_param->pad_left);
  printf("  pad_right %d\n", p_param->pad_right);
  printf("  ins_h %d\n", p_param->ins_h);
  printf("  ins_h_last %d\n", p_param->ins_h_last);
  printf("  ins_w %d\n", p_param->ins_w);
  printf("  ins_w_last %d\n", p_param->ins_w_last);
  printf("  stride_h %d\n", p_param->stride_h);
  printf("  stride_w %d\n", p_param->stride_w);
  printf("  output_c %d\n", p_param->output_c);
  printf("  output_h %d\n", p_param->output_h);
  printf("  output_w %d\n", p_param->output_w);
  printf("  has_bias %d\n", p_param->has_bias);
  printf("  relu_enable %d\n", p_param->relu_enable);

  if (dump_content) {
    printf("input_data(%d, %d, %d, %d) :\n", p_param->input_n, p_param->input_c,
           p_param->input_h, p_param->input_w);
    int in = p_param->input_n;
    int ic = p_param->input_c;
    int ih = p_param->input_h;
    int iw = p_param->input_w;
    for (int i = 0; i < in; ++i) {
      for (int j = 0; j < ic; ++j) {
        for (int k = 0; k < ih; ++k) {
          for (int l = 0; l < iw; ++l) {
            int offset = i * (ic * ih * iw) + j * (ih * iw) + k * iw + l;
            printf("%d, ", p_param->input_data[offset]);
          }
          printf("\n");
        }
      }
    }
    printf("\n\n");

    printf("kener_data (%d, %d, %d)\n", p_param->output_c, p_param->kh,
           p_param->kw);
    int kh = p_param->kh;
    int kw = p_param->kw;
    for (int i = 0; i < ic; ++i) {
      for (int j = 0; j < kh; ++j) {
        for (int k = 0; k < kw; ++k) {
          int offset = i * (kh * kw) + j * kw + k;
          printf("%d, ", p_param->filter_data[offset]);
        }
      }
      printf("\n");
    }
    printf("\n\n");

    if (p_param->has_bias) {
      printf("bias_data:\n");
      for (int i = 0; i < ic; ++i) {
        printf("%d, ", p_param->bias_data[i]);
      }
      printf("\n\n");
    }

    printf("multiplier_data:\n");
    for (int i = 0; i < ic; ++i) {
      printf("%d, ", p_param->multiplier_data[i]);
    }
    printf("\n\n");

    printf("shift_data:\n");
    for (int i = 0; i < ic; ++i) {
      printf("%d, ", p_param->shift_data[i]);
    }
    printf("\n\n");
  }
}

int run_compare_dw_conv(CVI_RT_HANDLE *rt_handler, cvk_context_t *cvk_ctx,
                        dw_conv_test_param_t *p_param)
{
  int ret = 0;

  if (rt_handler == NULL || cvk_ctx == NULL) {
    return -1;
  }

  int in = p_param->input_n;
  int ic = p_param->input_c;
  int ih = p_param->input_h;
  int iw = p_param->input_w;
  int oc = p_param->output_c;
  int oh = p_param->output_h;
  int ow = p_param->output_w;
  int kh = p_param->kh;
  int kw = p_param->kw;
  int dh = p_param->dh;
  int dw = p_param->dw;
  int pad_top = p_param->pad_top;
  int pad_bot = p_param->pad_bot;
  int pad_left = p_param->pad_left;
  int pad_right = p_param->pad_right;
  int ins_h = p_param->ins_h;
  int ins_last_h = p_param->ins_h_last;
  int ins_w = p_param->ins_w;
  int ins_last_w = p_param->ins_w_last;
  int stride_h = p_param->stride_h;
  int stride_w = p_param->stride_w;
  int has_bias = p_param->has_bias;
  int relu_enable = p_param->relu_enable;

  int input_size = in * ic * iw * ih;
  int8_t *input_data = (int8_t *)malloc(input_size);

  int kernel_size = oc * ic * kh * kw;
  int8_t *kernel_data = (int8_t *)malloc(kernel_size);

  int output_size = in * oc * oh * ow;
  int8_t *output_data = (int8_t *)malloc(output_size);
  if (!input_data || !kernel_data || !output_data) {
    free(input_data);
    free(kernel_data);
    free(output_data);
    return -1;
  }

  memset(output_data, 0, output_size);

  int32_t *bias_data = (int32_t *)malloc(sizeof(int32_t) * oc);
  uint32_t *multiplier_data = (uint32_t *)malloc(sizeof(uint32_t) * oc);
  int8_t *shift_data = (int8_t *)malloc(oc);

  p_param->input_data = input_data;
  p_param->filter_data = kernel_data;
  p_param->output_data = output_data;
  p_param->has_bias = has_bias;
  p_param->bias_data = bias_data;
  p_param->multiplier_data = multiplier_data;
  p_param->shift_data = shift_data;

#ifdef ENABLE_DEBUG_MSG
  printf("    run_compare_dw_conv =>\n");
  printf("      input (n=%d, ic=%d, h=%d, w=%d), kernel (oc=%d, ic=%d, h=%d, "
         "w=%d), output (, c=%d, h=%d, w=%d), has_bias %d\n",
         in, ic, ih, iw, oc, ic, kh, kw, oc, oh, ow, has_bias);
#endif

  int retry_cnt = p_param->retry_cnt;
  do {
    fill_random_data_s8(input_data, input_size);
    fill_random_data_s8(kernel_data, kernel_size);
    if (has_bias) {
      fill_random_data_s32(bias_data, oc);
    }

    p_param->float_multiplier = 100.0;  // should be < 1.0
    calc_dw_conv_float_multiplier(p_param);

    if (p_param->float_multiplier > 0.f && p_param->float_multiplier < 1.0) {
      break;
    }

  } while (--retry_cnt);

  if (p_param->float_multiplier >= 1.0) {
    printf("    run_compare_dw_conv: unable to find valid multiplier\n");
    free(input_data);
    free(kernel_data);
    free(output_data);
    free(bias_data);
    free(multiplier_data);
    free(shift_data);
    return -1;
  }

  uint32_t base_multiplier = 0;
  int base_shift = 0;
  QuantizeMultiplierSmallerThanOne(p_param->float_multiplier, &base_multiplier,
                                   &base_shift);

  for (int i = 0; i < oc; ++i) {
    // multipliers typically range in [2^30 ; 2^31 - 1].
    // Values in [0, 2^30 - 1] are normally unused, but harmless.
    // Thus a good way to randomize multipliers is to subtract from them
    // a random value smaller than 2^30 but still significant compared to it.
    p_param->multiplier_data[i] = base_multiplier - (rand() % (1 << 26));

    int right_shift = base_shift - 1 + (rand() % 4);
    p_param->shift_data[i] =
        truncate_rshift((int8_t)right_shift, /*allow_lshift*/1);

#ifdef ENABLE_DEBUG_MSG
    printf("      [oc=%d] multiplier_data %d, shift_data %d\n", i,
           p_param->multiplier_data[i], p_param->shift_data[i]);
#endif
  }

  dw_conv_per_channel_ref(p_param);

  const int chl_quan_size_dma =
      p_param->has_bias ? 9 : 5;  // bias(4) + multiplier(4) + shift(1)
  const int cal_data_size = oc * chl_quan_size_dma;
  uint8_t *chl_quan_data = (uint8_t *)malloc(cal_data_size);
  pack_chl_quan_param(oc, p_param->has_bias, p_param->bias_data,
                      p_param->multiplier_data, p_param->shift_data,
                      chl_quan_data);

  cvk_tl_shape_t input_shape = {in, ic, ih, iw};
  cvk_tl_shape_t filter_shape = {1, oc, kh, kw};
  cvk_tl_shape_t output_shape = {in, oc, oh, ow};
  cvk_tl_shape_t chl_quan_shape_dma = {1, oc, 1, chl_quan_size_dma};
  cvk_tl_shape_t chl_quan_shape_tiu = {1, oc, 1, 1};

  cvk_tl_t *tl_input =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, input_shape, CVK_FMT_I8,
                                      /*eu_aign=*/1);

  cvk_tl_t *tl_filter =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, filter_shape, CVK_FMT_I8,
                                      /*eu_align=*/1);

  cvk_tl_t *tl_output =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, output_shape, CVK_FMT_I8,
                                      /*eu_align=*/1);

  // Shape for TDMA load
  cvk_tl_t *tl_chl_quan_dma =
      cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, chl_quan_shape_dma, CVK_FMT_U8,
                                      /*eu_align*/ 0);

  if (!tl_input || !tl_filter || !tl_output || !tl_chl_quan_dma) {
    if (tl_input == NULL) {
      printf("      fail to alloc tl_input (%d, %d, %d, %d)\n",
            input_shape.n, input_shape.c, input_shape.h, input_shape.w);
    }
    if (tl_filter == NULL) {
      printf("      fail to alloc tl_filter (%d, %d, %d, %d)\n",
            filter_shape.n, filter_shape.c, filter_shape.h, filter_shape.w);
    }
    if (tl_output == NULL) {
      printf("      fail to alloc tl_output (%d, %d, %d, %d)\n",
            output_shape.n, output_shape.c, output_shape.h, output_shape.w);
    }
    if (tl_chl_quan_dma == NULL) {
      printf("      fail to alloc tl_chl_quan_dma (%d, %d, %d, %d)\n",
            chl_quan_shape_dma.n, chl_quan_shape_dma.c, chl_quan_shape_dma.h,
            chl_quan_shape_dma.w);
    }

    // Reverse order
    if (tl_chl_quan_dma)
      cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_chl_quan_dma);
    if (tl_output)
      cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_output);
    if (tl_filter)
      cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_filter);
    if (tl_input)
      cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_input);

    return -1;
  }


  tensor_copy_s2d_g2l(rt_handler, cvk_ctx, tl_chl_quan_dma, chl_quan_data);
  tensor_copy_s2d_g2l(rt_handler, cvk_ctx, tl_input, (uint8_t *)input_data);
  tensor_copy_s2d_g2l(rt_handler, cvk_ctx, tl_filter, (uint8_t *)kernel_data);

  {
    cvk_tl_t tl_chl_quan_tiu;
    memset(&tl_chl_quan_tiu, 0, sizeof(tl_chl_quan_tiu));
    tl_chl_quan_tiu.start_address = tl_chl_quan_dma->start_address;
    tl_chl_quan_tiu.fmt = tl_chl_quan_dma->fmt;
    tl_chl_quan_tiu.shape.n = chl_quan_shape_tiu.n;
    tl_chl_quan_tiu.shape.c = chl_quan_shape_tiu.c;
    tl_chl_quan_tiu.shape.h = chl_quan_shape_tiu.h;
    tl_chl_quan_tiu.shape.w = chl_quan_shape_tiu.w;
    tl_chl_quan_tiu.stride = cvk_ctx->ops->tl_default_stride(
        cvk_ctx, tl_chl_quan_tiu.shape, CVK_FMT_I8, /*eu_align=*/0);

    cvk_tiu_depthwise_convolution_param_t param;
    memset(&param, 0, sizeof(param));
    param.ofmap = tl_output;
    param.ifmap = tl_input;
    param.weight = tl_filter;
    param.chl_quan_param = &tl_chl_quan_tiu;
    param.ins_h = ins_h;
    param.ins_last_h = ins_last_h;
    param.ins_w = ins_w;
    param.ins_last_w = ins_last_w;
    param.stride_h = stride_h;
    param.stride_w = stride_w;
    param.dilation_h = dh;
    param.dilation_w = dw;
    param.pad_top = pad_top;
    param.pad_bottom = pad_bot;
    param.pad_left = pad_left;
    param.pad_right = pad_right;
    param.has_bias = has_bias;
    param.relu_enable = relu_enable;

#ifdef ENABLE_DEBUG_MSG
    printf("      tiu_dw_conv_qdm:\n");
    printf("        ifmap shape (%d, %d, %d, %d)\n", param.ifmap->shape.n,
           param.ifmap->shape.c, param.ifmap->shape.h, param.ifmap->shape.w);
    printf("        weight shape (%d, %d, %d, %d)\n", param.weight->shape.n,
           param.weight->shape.c, param.weight->shape.h, param.weight->shape.w);
    printf("        ofmap shape (%d, %d, %d, %d)\n", param.ofmap->shape.n,
           param.ofmap->shape.c, param.ofmap->shape.h, param.ofmap->shape.w);
#endif

    cvk_ctx->ops->tiu_depthwise_convolution(cvk_ctx, &param);
  }

  CVI_RT_Submit(cvk_ctx);

#ifdef ENABLE_DEBUG_MSG
  printf("      compare result:\n");
#endif
  int8_t *conv_output_data =
      (int8_t *)tensor_copy_l2g_d2s(rt_handler, cvk_ctx, tl_output);
  for (int i = 0; i < output_size; i++) {
    if (conv_output_data[i] != output_data[i]) {
      printf("        output_data[%d] %d(tiu) != %d(ref)\n", i,
             conv_output_data[i], output_data[i]);
      ret = -1;
      break;
    }
  }

  if (ret) {
    dump_test_param(p_param, /*dump_content=*/true);
  }

  // Reverse order
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_chl_quan_dma);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_output);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_filter);
  cvk_ctx->ops->lmem_free_tensor(cvk_ctx, tl_input);

  free(conv_output_data);

  free(input_data);
  free(kernel_data);
  free(output_data);
  free(bias_data);
  free(multiplier_data);
  free(shift_data);
  free(chl_quan_data);

#ifdef ENABLE_DEBUG_MSG
  printf("    <= run_compare_dw_conv\n");
#endif

  return ret;
}

bool check_valid_test_param(cvk_context_t *cvk_ctx,
                            dw_conv_test_param_t *p_param)
{
  int in = p_param->input_n;
  int ic = p_param->input_c;
  int ih = p_param->input_h;
  int iw = p_param->input_w;
  int oc = p_param->output_c;
  int oh = p_param->output_h;
  int ow = p_param->output_w;
  int kh = p_param->kh;
  int kw = p_param->kw;
  int stride_h = p_param->stride_h;
  int stride_w = p_param->stride_w;
  int chl_quan_size_dma =
      p_param->has_bias ? 9 : 5;  // bias(4) + multiplier(4) + shift(1)

  // Skip invalid shape
  if ((kh > ih) || (kw > iw) || (stride_h > ih) || (stride_w > iw)) {
    return false;
  }

  // muliply random-choosen value may exceeded than int32_t
  uint32_t input_size = in * ic * ih * iw;
  uint32_t kernel_size = ic * kh * kw;  // no oc
  uint32_t output_size = in * oc * oh * ow;

  uint32_t lmem_size_per_lane = cvk_ctx->info.lmem_size;
  uint32_t total_lmem_size = cvk_ctx->info.lmem_size * cvk_ctx->info.npu_num;

  uint32_t total_needed_size = input_size + kernel_size + output_size +
                          chl_quan_size_dma * cvk_ctx->info.npu_num;
  if (total_needed_size > total_lmem_size) {
    return false;
  }

  cvk_tl_shape_t input_shape = {in, ic, ih, iw};
  cvk_tl_shape_t filter_shape = {1, oc, kh, kw};
  cvk_tl_shape_t output_shape = {in, oc, oh, ow};
  cvk_tl_shape_t chl_quan_shape_dma = {1, oc, 1, chl_quan_size_dma};

  uint32_t needed_size =
      cvk_ctx->ops->lmem_tensor_to_size(cvk_ctx, input_shape, CVK_FMT_I8, /*eu_align=*/1) +
      cvk_ctx->ops->lmem_tensor_to_size(cvk_ctx, filter_shape, CVK_FMT_I8, /*eu_align=*/1) +
      cvk_ctx->ops->lmem_tensor_to_size(cvk_ctx, output_shape, CVK_FMT_I8, /*eu_align=*/1) +
      cvk_ctx->ops->lmem_tensor_to_size(cvk_ctx, chl_quan_shape_dma, CVK_FMT_I8, /*eu_align=*/0);

  // Skip invalid shape
  if (needed_size > lmem_size_per_lane) {
    return false;
  }

  return true;
}

static int random_test(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx)
{
  int ret = 0;

  if (rt_handle == NULL || cvk_ctx == NULL) {
    return -1;
  }

#ifndef ENABLE_FULL_REGRESSION
#ifndef ENABLE_TV_GEN_PATTERN
  // Input with same range size
  // n: 12b, c: 12b, h: 12b(4095-32), wb: 12b(4095-32)
  int batch_range[] = {1, 1, 2, 4095 - 32};
  int input_height_range[] = {1, 512, 1024, 4095 - 32};
  int input_width_range[] = {1, 512, 1024, 4095 - 32};
  int input_depth_range[] = {1, 16, 32, 4095 - 32};

  // Kernel with same range size
  // h: 12b, w: 12b
  // stride_h: 4b, strid_w: 4b
  int kernel_height_range[] = {1, 11, 4095};
  int kernel_width_range[] = {1, 11, 4095};
  int kernel_stride_height_range[] = {1, 5, 15};
  int kernel_stride_width_range[] = {1, 5, 15};
#else
  // TV_GEN pattern
  //  Random Test, total 2187, skipped 13095, executed 27, failed 0, ret 0

  // Input with same range size
  // n: 12b, c: 12b, h: 12b(4095-32), wb: 12b(4095-32)
  int batch_range[] = {1, 1, 3232};
  int input_height_range[] = {1, 512, 4095 - 32};
  int input_width_range[] = {1, 512, 4095 - 32};
  int input_depth_range[] = {1, 16, 4095 - 32};

  // Kernel with same range size
  // h: 12b, w: 12b
  // stride_h: 4b, strid_w: 4b
  int kernel_height_range[] = {1, 11, 4095};
  int kernel_width_range[] = {1, 11, 4095};
  int kernel_stride_height_range[] = {1, 5, 15};
  int kernel_stride_width_range[] = {1, 5, 15};
#endif // ENABLE_TV_GEN_PATTERN
#else
#if 0
  // Input with same range size
  int batch_range[] = {1};
  int input_height_range[] = {1};
  int input_width_range[] = {1};
  int input_depth_range[] = {1};

  // Kernel with same range size
  int kernel_height_range[] = {1};
  int kernel_width_range[] = {1};
  int kernel_stride_height_range[] = {1};
  int kernel_stride_width_range[] = {1};
  int output_depth_range[] = {1};
#else
  // 10/21/2019
  // Random Test, total 512000, skipped 2535629, executed 24371

  // Input with same range size
  // n: 12b, c: 12b, h: 12b(4095-32), wb: 12b(4095-32)
  int batch_range[] = {1, 2, 4, 8, 16, 32, 64, 4095 - 32};
  int input_height_range[] = {1, 3, 11, 128, 512, 1024, 2048, 4095 - 32};
  int input_width_range[] = {1, 3, 11, 128, 512, 1024, 2048, 4095 - 32};
  int input_depth_range[] = {1, 3, 11, 32, 64, 1024, 2048, 4095 - 32};

  // Kernel with same range size
  // h: 12b, w: 12b
  // stride_h: 4b, strid_w: 4b
  int kernel_height_range[] = {1, 3, 11, 511, 4095};
  int kernel_width_range[] = {1, 3, 11, 511, 4095};
  int kernel_stride_height_range[] = {1, 3, 5, 7, 15};
  int kernel_stride_width_range[] = {1, 3, 5, 7, 15};
#endif
#endif /* ENABLE_FULL_REGRESSION */

  const int input_range_size =
      sizeof(input_height_range) / sizeof(input_height_range[0]);
  const int kernel_range_size =
      sizeof(kernel_height_range) / sizeof(kernel_height_range[0]);

  int random_seed = clock();
  srand(random_seed);

  const int retry_test_count = 100;
  bool stop_at_first_error = true;

  int executed_tests = 0;
  int failed_tests = 0;

  printf("dw-conv random Test =>\n");
  for (int m = 0; m < retry_test_count; ++m) {
    for (int i = 0; i < input_range_size; ++i) {
      // random choosed from [range[i] : range[i+1]]
      int batch = choose_from_range(batch_range, input_range_size, i);

      for (int j = 0; j < input_range_size; ++j) {
        int input_height =
            choose_from_range(input_height_range, input_range_size, j);

        for (int k = 0; k < input_range_size; ++k) {
          int input_width =
              choose_from_range(input_width_range, input_range_size, k);

          for (int l = 0; l < input_range_size; ++l) {
            int input_depth =
                choose_from_range(input_depth_range, input_range_size, k);

            for (int m = 0; m < kernel_range_size; ++m) {
              int kernel_height =
                  choose_from_range(kernel_height_range, kernel_range_size, m);

              for (int n = 0; n < kernel_range_size; ++n) {
                int kernel_width =
                    choose_from_range(kernel_width_range, kernel_range_size, n);

                for (int x = 0; x < kernel_range_size; ++x) {
                  int kernel_stride_height = choose_from_range(
                      kernel_stride_height_range, kernel_range_size, x);

                  for (int y = 0; y < kernel_range_size; ++y) {
                    int kernel_stride_width = choose_from_range(
                        kernel_stride_width_range, kernel_range_size, y);
                    int has_bias = rand() % 2;
                    int dh = 1;
                    int dw = 1;
                    int ins_h = 0;
                    int ins_h_last = 0;
                    int ins_w = 0;
                    int ins_w_last = 0;
                    int pad_top = 0;
                    int pad_bot = 0;
                    int pad_left = 0;
                    int pad_right = 0;

                    int ih_ext = calc_dilute_hw(input_height, ins_h, ins_h_last,
                                                pad_top, pad_bot);
                    int iw_ext = calc_dilute_hw(input_width, ins_w, ins_w_last,
                                                pad_left, pad_right);
                    int kh_ext = calc_dilute_hw(kernel_height, dh - 1, 0, 0, 0);
                    int kw_ext = calc_dilute_hw(kernel_width, dw - 1, 0, 0, 0);

                    int oh =
                        calc_output_hw(ih_ext, kh_ext, kernel_stride_height);
                    int ow =
                        calc_output_hw(iw_ext, kw_ext, kernel_stride_width);

                    // depthwise, input depth == output depth
                    int output_depth = input_depth;

                    dw_conv_test_param_t test_param;
                    memset(&test_param, 0, sizeof(test_param));
                    test_param.input_n = batch;
                    test_param.input_c = input_depth;
                    test_param.input_h = input_height;
                    test_param.input_w = input_width;
                    test_param.kh = kernel_height;
                    test_param.kw = kernel_width;
                    test_param.dh = dh;
                    test_param.dw = dw;
                    test_param.pad_top = pad_top;
                    test_param.pad_bot = pad_bot;
                    test_param.pad_left = pad_left;
                    test_param.pad_right = pad_right;
                    test_param.ins_h = ins_h;
                    test_param.ins_h_last = ins_h_last;
                    test_param.ins_w = ins_w;
                    test_param.ins_w_last = ins_w_last;
                    test_param.stride_h = kernel_stride_height;
                    test_param.stride_w = kernel_stride_width;
                    test_param.output_c = output_depth;
                    test_param.output_h = oh;
                    test_param.output_w = ow;
                    test_param.has_bias = has_bias;
                    test_param.retry_cnt = 5;

                    bool is_valid_param =
                        check_valid_test_param(cvk_ctx, &test_param);
                    if (is_valid_param == false)
                      continue;

                    int ret2 = run_compare_dw_conv(rt_handle, cvk_ctx, &test_param);
                    failed_tests = ret2 ? failed_tests + 1 : failed_tests;
                    ret |= ret2;
                    executed_tests++;

#ifdef ENABLE_DEBUG_MSG
                    printf("  [%d] random test: input shape(%d, %d, %d, %d)",
                           executed_tests, batch, input_depth,
                           input_height, input_width);
                    printf(", kernel shape (%d, %d, %d, %d), result %d\n",
                           output_depth, input_depth, kernel_height,
                           kernel_width, ret);
#endif

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
    }

    // Stop at first error
    if (ret && stop_at_first_error) {
      break;
    }

    if (executed_tests >= MIN_EXEC_TESTS) {
      break;
    }
  }

  printf("<= dw-conv: random test, total %d, failed %d, ret %d\n",
         executed_tests, failed_tests, ret);

  return ret;
}

int main(int argc, char **argv)
{
  int ret = 0;

  if (!argc)
    return -1;
  if (!argv)
    return -1;

  CVI_RT_HANDLE rt_handle;
  cvk_context_t *cvk_ctx = NULL;

  CVI_RT_Init(&rt_handle);
  cvk_ctx = CVI_RT_RegisterKernel(rt_handle, CMDBUF_SIZE);

  if (!ret)
    ret |= simple_test(rt_handle, cvk_ctx);
  if (!ret)
    ret |= random_test(rt_handle, cvk_ctx);

  CVI_RT_UnRegisterKernel(cvk_ctx);
  CVI_RT_DeInit(rt_handle);

  if (!ret)
    printf("%s pass\n", __FILENAME__);
  else
    printf("%s fail\n", __FILENAME__);

  return ret;
}
