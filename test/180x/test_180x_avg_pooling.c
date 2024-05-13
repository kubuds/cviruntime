#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>

#include "test_cvikernel_util.h"
#include "test_native_ref.h"

#define INVALIDE_STRIDE (-1)

static void print_pooling_param(const cvk_tiu_average_pooling_param_t *p)
{
  int in = p->ifmap->shape.n;
  int ic = p->ifmap->shape.c;
  int ih = p->ifmap->shape.h;
  int iw = p->ifmap->shape.w;

  printf("  Pooling parameters:\n");
  printf("    ifmap = (%d, %d, %d, %d)\n", in, ic, ih, iw);
  printf("    opd0_sign = %d\n", p->ifmap->fmt == CVK_FMT_I8);
  printf("    weight = (%d, %d)\n", p->kh, p->kw);
  printf("    padding = (%d, %d, %d, %d)\n",
         p->pad_top, p->pad_bottom, p->pad_left, p->pad_right);
  printf("    stride = (%d, %d)\n", p->stride_h, p->stride_w);
  printf("    ins0 = (%d, %d, %d, %d)\n",
         p->ins_h, p->ins_last_h, p->ins_w, p->ins_last_w);
  printf("    avg_pooling_const = %d\n", p->avg_pooling_const);
  printf("    rshift_bits = %d\n", p->rshift_bits);
}

static int8_t *alloc_input(cvk_tiu_average_pooling_param_t *p)
{
  uint64_t size = tl_shape_size(&p->ifmap->shape, p->ifmap->fmt);
  int8_t *data = (int8_t *)malloc(size);
  if (!data)
    return NULL;

  for (uint64_t i = 0; i < size; i++)
    data[i] = rand() % 256 - 128;
  return data;
}

static int8_t *alloc_output(cvk_tiu_average_pooling_param_t *p)
{
  uint64_t size = tl_shape_size(&p->ofmap->shape, p->ofmap->fmt);
  return (int8_t *)malloc(size);
}

static int pooling_ih_ext(cvk_tiu_average_pooling_param_t *p, int ih)
{
  int ins = p->ins_h;
  int ins_last = p->ins_last_h;
  int pad = p->pad_top + p->pad_bottom;
  return (ih - 1) * (ins + 1) + ins_last + 1 + pad;
}

static int pooling_iw_ext(cvk_tiu_average_pooling_param_t *p, int iw)
{
  int ins = p->ins_w;
  int ins_last = p->ins_last_w;
  int pad = p->pad_left + p->pad_right;
  return (iw - 1) * (ins + 1) + ins_last + 1 + pad;
}

static int pooling_oh(cvk_tiu_average_pooling_param_t *p, int ih)
{
  int ih_ext = pooling_ih_ext(p, ih);
  return (ih_ext - p->kh) / p->stride_h + 1;
}

static int pooling_ow(cvk_tiu_average_pooling_param_t *p, int iw)
{
  int iw_ext = pooling_iw_ext(p, iw);
  return (iw_ext - p->kw) / p->stride_w + 1;
}

static void free_pooling_param(
    cvk_context_t *cvk_ctx,
    cvk_tiu_average_pooling_param_t *p)
{
  if (p->ifmap)
    cvk_ctx->ops->lmem_free_tensor(cvk_ctx, p->ifmap);
  if (p->ofmap)
    cvk_ctx->ops->lmem_free_tensor(cvk_ctx, p->ofmap);
}

static cvk_tiu_average_pooling_param_t random_pooling_param(cvk_context_t *cvk_ctx, int stride_w, int stride_h)
{
  int retry_cnt = 100;
  srand(clock());
  cvk_tiu_average_pooling_param_t p;

  for (int i = 0; i < retry_cnt; i++) {
    int in = rand() % 5 + 1;
    int ic = rand() % (3 * cvk_ctx->info.npu_num) + 1;
    int ih = rand() % 30 + 3 + (INVALIDE_STRIDE == stride_h ? 0 : stride_h);
    int iw = rand() % 30 + 6 + (INVALIDE_STRIDE == stride_w ? 0 : stride_w);
    int opd0_sign = rand() % 2;

    memset(&p, 0, sizeof(p));
    p.kh = rand() % 7 + 1;
    p.kw = rand() % 7 + 1;
    p.stride_h = INVALIDE_STRIDE == stride_h ? rand() % (p.kh) + 1 : stride_h;
    p.stride_w = INVALIDE_STRIDE == stride_w ? rand() % (p.kh) + 1 : stride_w;
    p.ins_h = rand() % p.kh;
    p.ins_w = rand() % p.kw;
    p.ins_last_h = rand() % p.kh;
    p.ins_last_w = rand() % p.kw;
    p.pad_top = rand() % p.kh;
    p.pad_bottom = rand() % p.kh;
    p.pad_left = rand() % p.kw;
    p.pad_right= rand() % p.kw;
    p.avg_pooling_const = rand() % 256;
    p.rshift_bits = rand() % 32;

    cvk_tl_shape_t ifmap_shape;
    ifmap_shape.n = in;
    ifmap_shape.c = ic;
    ifmap_shape.h = ih;
    ifmap_shape.w = iw;

    int on = in;
    int oc = ic;
    int oh = pooling_oh(&p, ih);
    int ow = pooling_ow(&p, iw);
    cvk_tl_shape_t ofmap_shape;
    ofmap_shape.n = on;
    ofmap_shape.c = oc;
    ofmap_shape.h = oh;
    ofmap_shape.w = ow;

    cvk_fmt_t fmt = opd0_sign? CVK_FMT_I8: CVK_FMT_U8;
    p.ofmap = cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, ofmap_shape, CVK_FMT_I8, 1);
    p.ifmap = cvk_ctx->ops->lmem_alloc_tensor(cvk_ctx, ifmap_shape, fmt, 1);

    if ((p.kh > pooling_ih_ext(&p, ih))
        || (p.kw > pooling_iw_ext(&p, iw))
        || (p.pad_top >= (1 << 4))
        || (p.pad_bottom >= (1 << 4))
        || (p.pad_left >= (1 << 4))
        || (p.pad_right >= (1 << 4))
        || !p.ofmap
        || !p.ifmap) {
      printf("retry init_pooling_param\n");
      free_pooling_param(cvk_ctx, &p);
    } else
      break;
  }

  return p;
}

static int compare_results(
    cvk_tiu_average_pooling_param_t *p,
    int8_t input[],
    int8_t output[])
{
  int in = p->ifmap->shape.n;
  int ic = p->ifmap->shape.c;
  int ih = p->ifmap->shape.h;
  int iw = p->ifmap->shape.w;
  int opd0_sign = (p->ifmap->fmt == CVK_FMT_I8);

  int8_t *output_ref = alloc_output(p);
  int ret = native_pooling_ave_int8(
      input, &p->avg_pooling_const, NULL, output_ref,
      in, ic, ih, iw, p->kh, p->kw,
      p->pad_top, p->pad_bottom, p->pad_left, p->pad_right,
      p->stride_h, p->stride_w,
      p->ins_h, p->ins_w, p->ins_last_h, p->ins_last_w,
      opd0_sign, opd0_sign, p->rshift_bits, 1);
  if (ret)
    return ret;

  ret = array_cmp_int8(
      "Comparing results ...\n", output_ref, output,
      tl_shape_size(&p->ofmap->shape, p->ofmap->fmt));

  if (ret != 0) {
    printf("Comparison FAILED!!!\n");
    print_pooling_param(p);
  }

  free(output_ref);

  return ret;
}

static int _test_pooling(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx, int stride_w, int stride_h)
{
  int ret;
  cvk_tiu_average_pooling_param_t p = random_pooling_param(cvk_ctx, stride_w, stride_h);
  int8_t *input = alloc_input(&p);
  if (!input)
    return -1;

  tensor_copy_s2d_g2l(rt_handle, cvk_ctx, p.ifmap, (uint8_t *)input);

  cvk_ctx->ops->tiu_average_pooling(cvk_ctx, &p);
  CVI_RT_Submit(cvk_ctx);

  int8_t *output = (int8_t *)tensor_copy_l2g_d2s(rt_handle, cvk_ctx, p.ofmap);
  if (!output)
    return -1;

  ret = compare_results(&p, input, output);

  free_pooling_param(cvk_ctx, &p);
  free(output);
  free(input);

  return ret;
}

static int test_pooling(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx) {
  return _test_pooling(rt_handle, cvk_ctx, INVALIDE_STRIDE, INVALIDE_STRIDE);
}

static int test_avg_pooling(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx)
{
  int ret = 0;
  for (uint64_t i = 0; i < 16; i++)
    ret |= test_pooling(rt_handle, cvk_ctx);

  // test stride extend (0, 31]
  int stride_list[] = {15, 16, 31};
  int stride_list_len = sizeof(stride_list) / sizeof(stride_list[0]);

  for (int stride_w_idx = 0; stride_w_idx < stride_list_len; stride_w_idx++) {
    for (int stride_h_idx = 0; stride_h_idx < stride_list_len; stride_h_idx++) {
      int stride_w = stride_list[stride_w_idx];
      int stride_h = stride_list[stride_h_idx];

      ret |= _test_pooling(rt_handle, cvk_ctx, stride_w, stride_h);
    }
  }
  
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

  ret = test_avg_pooling(rt_handle, cvk_ctx);

  CVI_RT_UnRegisterKernel(cvk_ctx);
  CVI_RT_DeInit(rt_handle);

  if (!ret)
    printf("%s pass\n", __FILENAME__);
  else
    printf("%s fail\n", __FILENAME__);

  return ret;
}
