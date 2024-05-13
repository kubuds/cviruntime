/**
 * plz refer [git](https://github.com/xiezhq-hermann/atan_lookup)
 * input range is `all real numbers` and output range is -pi/2 < x < pi/2,
 * you can refer [here](https://www.mathopenref.com/arctan.html) for more
 * details
 */
//
// xiezhq@shanghaitech.edu.cn && wanghe@shanghaitech.edu.cn
/* Reference:
   [1] Abhisek Ukil, Vishal H Shah, Bernhard Deck,
   "Fast Computation of arctangent Functions for Embedded Applications: A
   Comparative Analysis" IEEE International Symposium on Industrial Electronics,
   Pages: 1206 - 1211, DOI: 10.1109/ISIE.2011.5984330, 2011
   [2] Sreeraman Rajan, Sichun Wang, Robert Inkol, and Alain Joyal
   "Efficient Approximations for the Arctangent Function"
   IEEE SIGNAL PROCESSING MAGAZINE [108] MAY 2006
 */


#include "../1880v2_test_util.h"
#define OUT
#define IN
#include <cfloat>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>
//#define DBG

using namespace std;

#if 0
double atan_double(double x) {
  /*
  More precise look-up table is used for higher accuracy
  */
  if (x >= 0) {
    if (x <= 1) {
      int index = round(x * 100);
      return (LUT_d[index] + (x * 100 - index) * (LUT_d[index + 1] - LUT_d[index]));
    } else {
      double re_x = 1 / x;
      int index = round(re_x * 100);
      return (M_PI_2 - (LUT_d[index] + (re_x * 100 - index) * (LUT_d[index + 1] - LUT_d[index])));
      // No recursive is better here
    }
  } else {
    if (x >= -1) {
      double abs_x = -x;
      int index = round(abs_x * 100);
      return -(LUT_d[index] + (abs_x * 100 - index) * (LUT_d[index + 1] - LUT_d[index]));
    } else {
      double re_x = 1 / (-x);
      int index = round(re_x * 100);
      return (LUT_d[index] + (re_x * 100 - index) * (LUT_d[index+1] - LUT_d[index])) - M_PI_2;
    }
  }
}
#endif

/**
 * pre_data means we test fixed pattern, it should be same sa lut
 */
enum TEST_MODE {
  PRE_DATA_COMPARE_FIX = 0,  // pre-data + fix compare
  DATA_COMPARE_ACCURACY,     // generate \range_start to \range_end value that
                             // check epsilon
  DATA_COMPARE_U8,  // generate \range_start to \range_end value that check
                    // epsilon, result bf16->u8
  TEST_MODE_MAX,
};

static TEST_MODE mode;

static u16 test_pattern[] = {
    0x0000, 0x38D2, 0x3952, 0x399D, 0x39D2, 0x3A03, 0x3A1D, 0x3A38, 0x3A52,
    0x3A6C, 0x3A83, 0x3A90, 0x3A9D, 0x3AAA, 0x3AB8, 0x3AC5, 0x3AD2, 0x3ADF,
    0x3AEC, 0x3AF9, 0x3B03, 0x3B0A, 0x3B10, 0x3B17, 0x3B1D, 0x3B24, 0x3B2A,
    0x3B31, 0x3B38, 0x3B3E, 0x3B45, 0x3B4B, 0x3B52, 0x3B58, 0x3B5F, 0x3B65,
    0x3B6C, 0x3B72, 0x3B79, 0x3B80, 0x3B83, 0x3B86, 0x3B8A, 0x3B8D, 0x3B90,
    0x3B93, 0x3B97, 0x3B9A, 0x3B9D, 0x3BA1, 0x3BA4, 0x3BA7, 0x3BAA, 0x3BAE,
    0x3BB1, 0x3BB4, 0x3BB8, 0x3BBB, 0x3BBE, 0x3BC1, 0x3BC5, 0x3BC8, 0x3BCB,
    0x3BCE, 0x3BD2, 0x3BD5, 0x3BD8, 0x3BDC, 0x3BDF, 0x3BE2, 0x3BE5, 0x3BE9,
    0x3BEC, 0x3BEF, 0x3BF2, 0x3BF6, 0x3BF9, 0x3BFC, 0x3C00, 0x3C01, 0x3C03,
    0x3C05, 0x3C06, 0x3C08, 0x3C0A, 0x3C0B, 0x3C0D, 0x3C0F, 0x3C10, 0x3C12,
    0x3C13, 0x3C15, 0x3C17, 0x3C18, 0x3C1A, 0x3C1C, 0x3C1D, 0x3C1F, 0x3C21,
    0x3C22, 0x3C24, 0x3C25, 0x3C27, 0x3C29, 0x3C2A, 0x3C2C, 0x3C2E, 0x3C2F,
    0x3C31, 0x3C33, 0x3C34, 0x3C36, 0x3C38, 0x3C39, 0x3C3B, 0x3C3C, 0x3C3E,
    0x3C40, 0x3C41, 0x3C43, 0x3C45, 0x3C46, 0x3C48, 0x3C4A, 0x3C4B, 0x3C4D,
    0x3C4E, 0x3C50, 0x3C52, 0x3C53, 0x3C55, 0x3C57, 0x3C58, 0x3C5A, 0x3C5C,
    0x3C5D, 0x3C5F, 0x3C60, 0x3C62, 0x3C64, 0x3C65, 0x3C67, 0x3C69, 0x3C6A,
    0x3C6C, 0x3C6E, 0x3C6F, 0x3C71, 0x3C72, 0x3C74, 0x3C76, 0x3C77, 0x3C79,
    0x3C7B, 0x3C7C, 0x3C7E, 0x3C80, 0x3C81, 0x3C81, 0x3C82, 0x3C83, 0x3C84,
    0x3C85, 0x3C86, 0x3C86, 0x3C87, 0x3C88, 0x3C89, 0x3C8A, 0x3C8A, 0x3C8B,
    0x3C8C, 0x3C8D, 0x3C8E, 0x3C8F, 0x3C8F, 0x3C90, 0x3C91, 0x3C92, 0x3C93,
    0x3C93, 0x3C94, 0x3C95, 0x3C96, 0x3C97, 0x3C98, 0x3C98, 0x3C99, 0x3C9A,
    0x3C9B, 0x3C9C, 0x3C9C, 0x3C9D, 0x3C9E, 0x3C9F, 0x3CA0, 0x3CA1, 0x3CA1,
    0x3CA2, 0x3CA3, 0x3CA4, 0x3CA5, 0x3CA5, 0x3CA6, 0x3CA7, 0x3CA8, 0x3CA9,
    0x3CAA, 0x3CAA, 0x3CAB, 0x3CAC, 0x3CAD, 0x3CAE, 0x3CAE, 0x3CAF, 0x3CB0,
    0x3CB1, 0x3CB2, 0x3CB3, 0x3CB3, 0x3CB4, 0x3CB5, 0x3CB6, 0x3CB7, 0x3CB8,
    0x3CB8, 0x3CB9, 0x3CBA, 0x3CBB, 0x3CBC, 0x3CBC, 0x3CBD, 0x3CBE, 0x3CBF,
    0x3CC0, 0x3CC1, 0x3CC1, 0x3CC2, 0x3CC3, 0x3CC4, 0x3CC5, 0x3CC5, 0x3CC6,
    0x3CC7, 0x3CC8, 0x3CC9, 0x3CCA, 0x3CCA, 0x3CCB, 0x3CCC, 0x3CCD, 0x3CCE,
    0x3CCE, 0x3CCF, 0x3CD0, 0x3CD1, 0x3CD2, 0x3CD3, 0x3CD3, 0x3CD4, 0x3CD5,
    0x3CD6, 0x3CD7, 0x3CD7, 0x3CD8, 0x3CD9, 0x3CDA, 0x3CDB, 0x3CDC, 0x3CDC,
    0x3CDD, 0x3CDE, 0x3CDF, 0x3CE0, 0x3CE0, 0x3CE1, 0x3CE2, 0x3CE3, 0x3CE4,
    0x3CE5, 0x3CE5, 0x3CE6, 0x3CE7, 0x3CE8, 0x3CE9, 0x3CE9, 0x3CEA, 0x3CEB,
    0x3CEC, 0x3CED, 0x3CEE, 0x3CEE, 0x3CEF, 0x3CF0, 0x3CF1, 0x3CF2, 0x3CF2,
    0x3CF3, 0x3CF4, 0x3CF5, 0x3CF6, 0x3CF7, 0x3CF7, 0x3CF8, 0x3CF9, 0x3CFA,
    0x3CFB, 0x3CFB, 0x3CFC, 0x3CFD, 0x3CFE, 0x3CFF, 0x3D00, 0x3D00, 0x3D01,
    0x3D01, 0x3D01, 0x3D02, 0x3D02, 0x3D03, 0x3D03, 0x3D03, 0x3D04, 0x3D04,
    0x3D05, 0x3D05, 0x3D06, 0x3D06, 0x3D06, 0x3D07, 0x3D07, 0x3D08, 0x3D08,
    0x3D08, 0x3D09, 0x3D09, 0x3D0A, 0x3D0A, 0x3D0A, 0x3D0B, 0x3D0B, 0x3D0C,
    0x3D0C, 0x3D0C, 0x3D0D, 0x3D0D, 0x3D0E, 0x3D0E, 0x3D0F, 0x3D0F, 0x3D0F,
    0x3D10, 0x3D10, 0x3D11, 0x3D11, 0x3D11, 0x3D12, 0x3D12, 0x3D13, 0x3D13,
    0x3D13, 0x3D14, 0x3D14, 0x3D15, 0x3D15, 0x3D16, 0x3D16, 0x3D16, 0x3D17,
    0x3D17, 0x3D18, 0x3D18, 0x3D18, 0x3D19, 0x3D19, 0x3D1A, 0x3D1A, 0x3D1A,
    0x3D1B, 0x3D1B, 0x3D1C, 0x3D1C, 0x3D1C, 0x3D1D, 0x3D1D, 0x3D1E, 0x3D1E,
    0x3D1F, 0x3D1F, 0x3D1F, 0x3D20, 0x3D20, 0x3D21, 0x3D21, 0x3D21, 0x3D22,
    0x3D22, 0x3D23, 0x3D23, 0x3D23, 0x3D24, 0x3D24, 0x3D25, 0x3D25, 0x3D25,
    0x3D26, 0x3D26, 0x3D27, 0x3D27, 0x3D28, 0x3D28, 0x3D28, 0x3D29, 0x3D29,
    0x3D2A, 0x3D2A, 0x3D2A, 0x3D2B, 0x3D2B, 0x3D2C, 0x3D2C, 0x3D2C, 0x3D2D,
    0x3D2D, 0x3D2E, 0x3D2E, 0x3D2E, 0x3D2F, 0x3D2F, 0x3D30, 0x3D30, 0x3D31,
    0x3D31, 0x3D31, 0x3D32, 0x3D32, 0x3D33, 0x3D33, 0x3D33, 0x3D34, 0x3D34,
    0x3D35, 0x3D35, 0x3D35, 0x3D36, 0x3D36, 0x3D37, 0x3D37, 0x3D38, 0x3D38,
    0x3D38, 0x3D39, 0x3D39, 0x3D3A, 0x3D3A, 0x3D3A, 0x3D3B, 0x3D3B, 0x3D3C,
    0x3D3C, 0x3D3C, 0x3D3D, 0x3D3D, 0x3D3E, 0x3D3E, 0x3D3E, 0x3D3F, 0x3D3F,
    0x3D40, 0x3D40, 0x3D41, 0x3D41, 0x3D41, 0x3D42, 0x3D42, 0x3D43, 0x3D43,
    0x3D43, 0x3D44, 0x3D44, 0x3D45, 0x3D45, 0x3D45, 0x3D46, 0x3D46, 0x3D47,
    0x3D47, 0x3D47, 0x3D48, 0x3D48, 0x3D49, 0x3D49, 0x3D4A, 0x3D4A, 0x3D4A,
    0x3D4B, 0x3D4B, 0x3D4C, 0x3D4C, 0x3D4C, 0x3D4D, 0x3D4D, 0x3D4E, 0x3D4E,
    0x3D4E, 0x3D4F, 0x3D4F, 0x3D50, 0x3D50, 0x3D50, 0x3D51, 0x3D51, 0x3D52,
    0x3D52, 0x3D53, 0x3D53, 0x3D53, 0x3D54, 0x3D54, 0x3D55, 0x3D55, 0x3D55,
    0x3D56, 0x3D56, 0x3D57, 0x3D57, 0x3D57, 0x3D58, 0x3D58, 0x3D59, 0x3D59,
    0x3D59, 0x3D5A, 0x3D5A, 0x3D5B, 0x3D5B, 0x3D5C, 0x3D5C, 0x3D5C, 0x3D5D,
    0x3D5D, 0x3D5E, 0x3D5E, 0x3D5E, 0x3D5F, 0x3D5F, 0x3D60, 0x3D60, 0x3D60,
    0x3D61, 0x3D61, 0x3D62, 0x3D62, 0x3D63, 0x3D63, 0x3D63, 0x3D64, 0x3D64,
    0x3D65, 0x3D65, 0x3D65, 0x3D66, 0x3D66, 0x3D67, 0x3D67, 0x3D67, 0x3D68,
    0x3D68, 0x3D69, 0x3D69, 0x3D69, 0x3D6A, 0x3D6A, 0x3D6B, 0x3D6B, 0x3D6C,
    0x3D6C, 0x3D6C, 0x3D6D, 0x3D6D, 0x3D6E, 0x3D6E, 0x3D6E, 0x3D6F, 0x3D6F,
    0x3D70, 0x3D70, 0x3D70, 0x3D71, 0x3D71, 0x3D72, 0x3D72, 0x3D72, 0x3D73,
    0x3D73, 0x3D74, 0x3D74, 0x3D75, 0x3D75, 0x3D75, 0x3D76, 0x3D76, 0x3D77,
    0x3D77, 0x3D77, 0x3D78, 0x3D78, 0x3D79, 0x3D79, 0x3D79, 0x3D7A, 0x3D7A,
    0x3D7B, 0x3D7B, 0x3D7B, 0x3D7C, 0x3D7C, 0x3D7D, 0x3D7D, 0x3D7E, 0x3D7E,
    0x3D7E, 0x3D7F, 0x3D7F, 0x3D80, 0x3D80, 0x3D80, 0x3D80, 0x3D81, 0x3D81,
    0x3D81, 0x3D81, 0x3D81, 0x3D82, 0x3D82, 0x3D82, 0x3D82, 0x3D82, 0x3D83,
    0x3D83, 0x3D83, 0x3D83, 0x3D83, 0x3D84, 0x3D84, 0x3D84, 0x3D84, 0x3D85,
    0x3D85, 0x3D85, 0x3D85, 0x3D85, 0x3D86, 0x3D86, 0x3D86, 0x3D86, 0x3D86,
    0x3D87, 0x3D87, 0x3D87, 0x3D87, 0x3D87, 0x3D88, 0x3D88, 0x3D88, 0x3D88,
    0x3D88, 0x3D89, 0x3D89, 0x3D89, 0x3D89, 0x3D89, 0x3D8A, 0x3D8A, 0x3D8A,
    0x3D8A, 0x3D8A, 0x3D8B, 0x3D8B, 0x3D8B, 0x3D8B, 0x3D8B, 0x3D8C, 0x3D8C,
    0x3D8C, 0x3D8C, 0x3D8C, 0x3D8D, 0x3D8D, 0x3D8D, 0x3D8D, 0x3D8E, 0x3D8E,
    0x3D8E, 0x3D8E, 0x3D8E, 0x3D8F, 0x3D8F, 0x3D8F, 0x3D8F, 0x3D8F, 0x3D90,
    0x3D90, 0x3D90, 0x3D90, 0x3D90, 0x3D91, 0x3D91, 0x3D91, 0x3D91, 0x3D91,
    0x3D92, 0x3D92, 0x3D92, 0x3D92, 0x3D92, 0x3D93, 0x3D93, 0x3D93, 0x3D93,
    0x3D93, 0x3D94, 0x3D94, 0x3D94, 0x3D94, 0x3D94, 0x3D95, 0x3D95, 0x3D95,
    0x3D95, 0x3D96, 0x3D96, 0x3D96, 0x3D96, 0x3D96, 0x3D97, 0x3D97, 0x3D97,
    0x3D97, 0x3D97, 0x3D98, 0x3D98, 0x3D98, 0x3D98, 0x3D98, 0x3D99, 0x3D99,
    0x3D99, 0x3D99, 0x3D99, 0x3D9A, 0x3D9A, 0x3D9A, 0x3D9A, 0x3D9A, 0x3D9B,
    0x3D9B, 0x3D9B, 0x3D9B, 0x3D9B, 0x3D9C, 0x3D9C, 0x3D9C, 0x3D9C, 0x3D9C,
    0x3D9D, 0x3D9D, 0x3D9D, 0x3D9D, 0x3D9D, 0x3D9E, 0x3D9E, 0x3D9E, 0x3D9E,
    0x3D9F, 0x3D9F, 0x3D9F, 0x3D9F, 0x3D9F, 0x3DA0, 0x3DA0, 0x3DA0, 0x3DA0,
    0x3DA0, 0x3DA1, 0x3DA1, 0x3DA1, 0x3DA1, 0x3DA1, 0x3DA2, 0x3DA2, 0x3DA2,
    0x3DA2, 0x3DA2, 0x3DA3, 0x3DA3, 0x3DA3, 0x3DA3, 0x3DA3, 0x3DA4, 0x3DA4,
    0x3DA4, 0x3DA4, 0x3DA4, 0x3DA5, 0x3DA5, 0x3DA5, 0x3DA5, 0x3DA5, 0x3DA6,
    0x3DA6, 0x3DA6, 0x3DA6, 0x3DA7, 0x3DA7, 0x3DA7, 0x3DA7, 0x3DA7, 0x3DA8,
    0x3DA8, 0x3DA8, 0x3DA8, 0x3DA8, 0x3DA9, 0x3DA9, 0x3DA9, 0x3DA9, 0x3DA9,
    0x3DAA, 0x3DAA, 0x3DAA, 0x3DAA, 0x3DAA, 0x3DAB, 0x3DAB, 0x3DAB, 0x3DAB,
    0x3DAB, 0x3DAC, 0x3DAC, 0x3DAC, 0x3DAC, 0x3DAC, 0x3DAD, 0x3DAD, 0x3DAD,
    0x3DAD, 0x3DAD, 0x3DAE, 0x3DAE, 0x3DAE, 0x3DAE, 0x3DAE, 0x3DAF, 0x3DAF,
    0x3DAF, 0x3DAF, 0x3DB0, 0x3DB0, 0x3DB0, 0x3DB0, 0x3DB0, 0x3DB1, 0x3DB1,
    0x3DB1, 0x3DB1, 0x3DB1, 0x3DB2, 0x3DB2, 0x3DB2, 0x3DB2, 0x3DB2, 0x3DB3,
    0x3DB3, 0x3DB3, 0x3DB3, 0x3DB3, 0x3DB4, 0x3DB4, 0x3DB4, 0x3DB4, 0x3DB4,
    0x3DB5, 0x3DB5, 0x3DB5, 0x3DB5, 0x3DB5, 0x3DB6, 0x3DB6, 0x3DB6, 0x3DB6,
    0x3DB6, 0x3DB7, 0x3DB7, 0x3DB7, 0x3DB7, 0x3DB8, 0x3DB8, 0x3DB8, 0x3DB8,
    0x3DB8, 0x3DB9, 0x3DB9, 0x3DB9, 0x3DB9, 0x3DB9, 0x3DBA, 0x3DBA, 0x3DBA,
    0x3DBA, 0x3DBA, 0x3DBB, 0x3DBB, 0x3DBB, 0x3DBB, 0x3DBB, 0x3DBC, 0x3DBC,
    0x3DBC, 0x3DBC, 0x3DBC, 0x3DBD, 0x3DBD, 0x3DBD, 0x3DBD, 0x3DBD, 0x3DBE,
    0x3DBE, 0x3DBE, 0x3DBE, 0x3DBE, 0x3DBF, 0x3DBF, 0x3DBF, 0x3DBF, 0x3DBF,
    0x3DC0, 0x3DC0, 0x3DC0, 0x3DC0, 0x3DC1, 0x3DC1, 0x3DC1, 0x3DC1, 0x3DC1,
    0x3DC2, 0x3DC2, 0x3DC2, 0x3DC2, 0x3DC2, 0x3DC3, 0x3DC3, 0x3DC3, 0x3DC3,
    0x3DC3, 0x3DC4, 0x3DC4, 0x3DC4, 0x3DC4, 0x3DC4, 0x3DC5, 0x3DC5, 0x3DC5,
    0x3DC5, 0x3DC5, 0x3DC6, 0x3DC6, 0x3DC6, 0x3DC6, 0x3DC6, 0x3DC7, 0x3DC7,
    0x3DC7, 0x3DC7, 0x3DC7, 0x3DC8, 0x3DC8, 0x3DC8, 0x3DC8, 0x3DC8, 0x3DC9,
    0x3DC9, 0x3DC9, 0x3DC9, 0x3DCA, 0x3DCA, 0x3DCA, 0x3DCA, 0x3DCA, 0x3DCB,
    0x3DCB, 0x3DCB, 0x3DCB, 0x3DCB, 0x3DCC, 0x3DCC, 0x3DCC, 0x3DCC, 0x3DCC,
    0x3DCD, 0x3DCE, 0x3DCF, 0x3DD0, 0x3DD1, 0x3DD2, 0x3DD3, 0x3DD4, 0x3DD5,
    0x3DD6, 0x3DD7, 0x3DD8, 0x3DD9, 0x3DDA, 0x3DDB, 0x3DDC, 0x3DDD, 0x3DDE,
    0x3DDF, 0x3DE0, 0x3DE1, 0x3DE2, 0x3DE3, 0x3DE4, 0x3DE5,
};

static u16 golden_bf16[] = {
    0x0,    0x38d2, 0x3952, 0x399d, 0x39d2, 0x3a03, 0x3a1d, 0x3a38, 0x3a52,
    0x3a6c, 0x3a83, 0x3a90, 0x3a9d, 0x3aaa, 0x3ab8, 0x3ac5, 0x3ad2, 0x3adf,
    0x3aec, 0x3afa, 0x3b03, 0x3b0a, 0x3b10, 0x3b17, 0x3b1d, 0x3b24, 0x3b2a,
    0x3b31, 0x3b38, 0x3b3e, 0x3b45, 0x3b4c, 0x3b52, 0x3b59, 0x3b5f, 0x3b65,
    0x3b6c, 0x3b72, 0x3b7a, 0x3b80, 0x3b83, 0x3b86, 0x3b8a, 0x3b8d, 0x3b90,
    0x3b93, 0x3b97, 0x3b9a, 0x3b9d, 0x3ba1, 0x3ba4, 0x3ba7, 0x3baa, 0x3bae,
    0x3bb1, 0x3bb4, 0x3bb8, 0x3bbb, 0x3bbe, 0x3bc1, 0x3bc5, 0x3bc8, 0x3bcb,
    0x3bce, 0x3bd2, 0x3bd6, 0x3bd8, 0x3bdc, 0x3bdf, 0x3be2, 0x3be6, 0x3be9,
    0x3bec, 0x3bef, 0x3bf2, 0x3bf6, 0x3bf9, 0x3bfc, 0x3c00, 0x3c01, 0x3c03,
    0x3c05, 0x3c06, 0x3c08, 0x3c0a, 0x3c0b, 0x3c0d, 0x3c0f, 0x3c10, 0x3c12,
    0x3c13, 0x3c15, 0x3c17, 0x3c18, 0x3c1a, 0x3c1c, 0x3c1d, 0x3c1f, 0x3c21,
    0x3c22, 0x3c24, 0x3c25, 0x3c27, 0x3c29, 0x3c2a, 0x3c2c, 0x3c2e, 0x3c2f,
    0x3c31, 0x3c33, 0x3c34, 0x3c36, 0x3c38, 0x3c39, 0x3c3b, 0x3c3c, 0x3c3e,
    0x3c40, 0x3c41, 0x3c43, 0x3c45, 0x3c46, 0x3c48, 0x3c4a, 0x3c4b, 0x3c4d,
    0x3c4e, 0x3c50, 0x3c52, 0x3c53, 0x3c55, 0x3c57, 0x3c58, 0x3c5a, 0x3c5c,
    0x3c5d, 0x3c5f, 0x3c60, 0x3c62, 0x3c64, 0x3c66, 0x3c68, 0x3c69, 0x3c6a,
    0x3c6c, 0x3c6e, 0x3c70, 0x3c71, 0x3c72, 0x3c74, 0x3c76, 0x3c78, 0x3c79,
    0x3c7b, 0x3c7c, 0x3c7e, 0x3c80, 0x3c81, 0x3c81, 0x3c82, 0x3c83, 0x3c84,
    0x3c85, 0x3c86, 0x3c86, 0x3c87, 0x3c88, 0x3c89, 0x3c8a, 0x3c8a, 0x3c8b,
    0x3c8c, 0x3c8d, 0x3c8e, 0x3c8f, 0x3c8f, 0x3c90, 0x3c91, 0x3c92, 0x3c93,
    0x3c93, 0x3c94, 0x3c95, 0x3c96, 0x3c97, 0x3c98, 0x3c98, 0x3c99, 0x3c9a,
    0x3c9b, 0x3c9c, 0x3c9c, 0x3c9d, 0x3c9e, 0x3c9f, 0x3ca0, 0x3ca1, 0x3ca1,
    0x3ca2, 0x3ca3, 0x3ca4, 0x3ca5, 0x3ca5, 0x3ca6, 0x3ca7, 0x3ca8, 0x3ca9,
    0x3caa, 0x3caa, 0x3cab, 0x3cac, 0x3cad, 0x3cae, 0x3cae, 0x3caf, 0x3cb0,
    0x3cb1, 0x3cb2, 0x3cb3, 0x3cb3, 0x3cb4, 0x3cb5, 0x3cb6, 0x3cb7, 0x3cb8,
    0x3cb8, 0x3cb9, 0x3cba, 0x3cbb, 0x3cbc, 0x3cbc, 0x3cbd, 0x3cbe, 0x3cbf,
    0x3cc0, 0x3cc1, 0x3cc1, 0x3cc2, 0x3cc3, 0x3cc4, 0x3cc5, 0x3cc5, 0x3cc6,
    0x3cc7, 0x3cc8, 0x3cc9, 0x3cca, 0x3cca, 0x3ccb, 0x3ccc, 0x3ccd, 0x3cce,
    0x3cce, 0x3ccf, 0x3cd0, 0x3cd1, 0x3cd2, 0x3cd3, 0x3cd3, 0x3cd4, 0x3cd5,
    0x3cd6, 0x3cd7, 0x3cd7, 0x3cd8, 0x3cd9, 0x3cda, 0x3cdb, 0x3cdc, 0x3cdc,
    0x3cdd, 0x3cde, 0x3cdf, 0x3ce0, 0x3ce0, 0x3ce1, 0x3ce2, 0x3ce3, 0x3ce4,
    0x3ce5, 0x3ce5, 0x3ce6, 0x3ce7, 0x3ce8, 0x3ce9, 0x3ce9, 0x3cea, 0x3ceb,
    0x3cec, 0x3ced, 0x3cee, 0x3cee, 0x3cef, 0x3cf0, 0x3cf1, 0x3cf2, 0x3cf2,
    0x3cf3, 0x3cf4, 0x3cf5, 0x3cf6, 0x3cf7, 0x3cf7, 0x3cf8, 0x3cf9, 0x3cfa,
    0x3cfb, 0x3cfb, 0x3cfc, 0x3cfd, 0x3cfe, 0x3cff, 0x3d00, 0x3d00, 0x3d01,
    0x3d01, 0x3d01, 0x3d02, 0x3d02, 0x3d03, 0x3d03, 0x3d03, 0x3d04, 0x3d04,
    0x3d05, 0x3d05, 0x3d06, 0x3d06, 0x3d06, 0x3d07, 0x3d07, 0x3d08, 0x3d08,
    0x3d08, 0x3d09, 0x3d09, 0x3d0a, 0x3d0a, 0x3d0a, 0x3d0b, 0x3d0b, 0x3d0c,
    0x3d0c, 0x3d0c, 0x3d0d, 0x3d0d, 0x3d0e, 0x3d0e, 0x3d0f, 0x3d0f, 0x3d0f,
    0x3d10, 0x3d10, 0x3d11, 0x3d11, 0x3d11, 0x3d12, 0x3d12, 0x3d13, 0x3d13,
    0x3d13, 0x3d14, 0x3d14, 0x3d15, 0x3d15, 0x3d16, 0x3d16, 0x3d16, 0x3d17,
    0x3d17, 0x3d18, 0x3d18, 0x3d18, 0x3d19, 0x3d19, 0x3d1a, 0x3d1a, 0x3d1a,
    0x3d1b, 0x3d1b, 0x3d1c, 0x3d1c, 0x3d1c, 0x3d1d, 0x3d1d, 0x3d1e, 0x3d1e,
    0x3d1f, 0x3d1f, 0x3d1f, 0x3d20, 0x3d20, 0x3d21, 0x3d21, 0x3d21, 0x3d22,
    0x3d22, 0x3d23, 0x3d23, 0x3d23, 0x3d24, 0x3d24, 0x3d25, 0x3d25, 0x3d25,
    0x3d26, 0x3d26, 0x3d27, 0x3d27, 0x3d28, 0x3d28, 0x3d28, 0x3d29, 0x3d29,
    0x3d2a, 0x3d2a, 0x3d2a, 0x3d2b, 0x3d2b, 0x3d2c, 0x3d2c, 0x3d2c, 0x3d2d,
    0x3d2d, 0x3d2e, 0x3d2e, 0x3d2e, 0x3d2f, 0x3d2f, 0x3d30, 0x3d30, 0x3d31,
    0x3d31, 0x3d31, 0x3d32, 0x3d32, 0x3d33, 0x3d33, 0x3d33, 0x3d34, 0x3d34,
    0x3d35, 0x3d35, 0x3d35, 0x3d36, 0x3d36, 0x3d37, 0x3d37, 0x3d38, 0x3d38,
    0x3d38, 0x3d39, 0x3d39, 0x3d3a, 0x3d3a, 0x3d3a, 0x3d3b, 0x3d3b, 0x3d3c,
    0x3d3c, 0x3d3c, 0x3d3d, 0x3d3d, 0x3d3e, 0x3d3e, 0x3d3e, 0x3d3f, 0x3d3f,
    0x3d40, 0x3d40, 0x3d41, 0x3d41, 0x3d41, 0x3d42, 0x3d42, 0x3d43, 0x3d43,
    0x3d43, 0x3d44, 0x3d44, 0x3d45, 0x3d45, 0x3d45, 0x3d46, 0x3d46, 0x3d47,
    0x3d47, 0x3d47, 0x3d48, 0x3d48, 0x3d49, 0x3d49, 0x3d4a, 0x3d4a, 0x3d4a,
    0x3d4b, 0x3d4b, 0x3d4c, 0x3d4c, 0x3d4c, 0x3d4d, 0x3d4d, 0x3d4e, 0x3d4e,
    0x3d4e, 0x3d4f, 0x3d4f, 0x3d50, 0x3d50, 0x3d50, 0x3d51, 0x3d51, 0x3d52,
    0x3d52, 0x3d53, 0x3d53, 0x3d53, 0x3d54, 0x3d54, 0x3d55, 0x3d55, 0x3d55,
    0x3d56, 0x3d56, 0x3d57, 0x3d57, 0x3d57, 0x3d58, 0x3d58, 0x3d59, 0x3d59,
    0x3d59, 0x3d5a, 0x3d5a, 0x3d5b, 0x3d5b, 0x3d5c, 0x3d5c, 0x3d5c, 0x3d5d,
    0x3d5d, 0x3d5e, 0x3d5e, 0x3d5e, 0x3d5f, 0x3d5f, 0x3d60, 0x3d60, 0x3d60,
    0x3d60, 0x3d60, 0x3d61, 0x3d61, 0x3d62, 0x3d62, 0x3d62, 0x3d63, 0x3d63,
    0x3d64, 0x3d64, 0x3d64, 0x3d65, 0x3d65, 0x3d66, 0x3d66, 0x3d66, 0x3d67,
    0x3d67, 0x3d68, 0x3d68, 0x3d68, 0x3d69, 0x3d69, 0x3d6a, 0x3d6a, 0x3d6b,
    0x3d6b, 0x3d6b, 0x3d6c, 0x3d6c, 0x3d6d, 0x3d6d, 0x3d6d, 0x3d6e, 0x3d6e,
    0x3d6f, 0x3d6f, 0x3d6f, 0x3d70, 0x3d70, 0x3d71, 0x3d71, 0x3d71, 0x3d72,
    0x3d72, 0x3d73, 0x3d73, 0x3d74, 0x3d74, 0x3d74, 0x3d75, 0x3d75, 0x3d76,
    0x3d76, 0x3d76, 0x3d77, 0x3d77, 0x3d78, 0x3d78, 0x3d78, 0x3d79, 0x3d79,
    0x3d7a, 0x3d7a, 0x3d7a, 0x3d7b, 0x3d7b, 0x3d7c, 0x3d7c, 0x3d7d, 0x3d7d,
    0x3d7d, 0x3d7e, 0x3d7e, 0x3d7f, 0x3d7f, 0x3d7f, 0x3d7f, 0x3d81, 0x3d81,
    0x3d81, 0x3d81, 0x3d81, 0x3d82, 0x3d82, 0x3d82, 0x3d82, 0x3d82, 0x3d83,
    0x3d83, 0x3d83, 0x3d83, 0x3d83, 0x3d84, 0x3d84, 0x3d84, 0x3d84, 0x3d85,
    0x3d85, 0x3d85, 0x3d85, 0x3d85, 0x3d86, 0x3d86, 0x3d86, 0x3d86, 0x3d86,
    0x3d87, 0x3d87, 0x3d87, 0x3d87, 0x3d87, 0x3d88, 0x3d88, 0x3d88, 0x3d88,
    0x3d88, 0x3d89, 0x3d89, 0x3d89, 0x3d89, 0x3d89, 0x3d8a, 0x3d8a, 0x3d8a,
    0x3d8a, 0x3d8a, 0x3d8b, 0x3d8b, 0x3d8b, 0x3d8b, 0x3d8b, 0x3d8c, 0x3d8c,
    0x3d8c, 0x3d8c, 0x3d8c, 0x3d8d, 0x3d8d, 0x3d8d, 0x3d8d, 0x3d8e, 0x3d8e,
    0x3d8e, 0x3d8e, 0x3d8e, 0x3d8f, 0x3d8f, 0x3d8f, 0x3d8f, 0x3d8f, 0x3d90,
    0x3d90, 0x3d90, 0x3d90, 0x3d90, 0x3d91, 0x3d91, 0x3d91, 0x3d91, 0x3d91,
    0x3d92, 0x3d92, 0x3d92, 0x3d92, 0x3d92, 0x3d93, 0x3d93, 0x3d93, 0x3d93,
    0x3d93, 0x3d94, 0x3d94, 0x3d94, 0x3d94, 0x3d94, 0x3d95, 0x3d95, 0x3d95,
    0x3d95, 0x3d96, 0x3d96, 0x3d96, 0x3d96, 0x3d96, 0x3d97, 0x3d97, 0x3d97,
    0x3d97, 0x3d97, 0x3d98, 0x3d98, 0x3d98, 0x3d98, 0x3d98, 0x3d99, 0x3d99,
    0x3d99, 0x3d99, 0x3d99, 0x3d99, 0x3d99, 0x3d99, 0x3d99, 0x3d99, 0x3d9a,
    0x3d9a, 0x3d9a, 0x3d9a, 0x3d9a, 0x3d9b, 0x3d9b, 0x3d9b, 0x3d9b, 0x3d9b,
    0x3d9c, 0x3d9c, 0x3d9c, 0x3d9c, 0x3d9c, 0x3d9d, 0x3d9d, 0x3d9d, 0x3d9d,
    0x3d9e, 0x3d9e, 0x3d9e, 0x3d9e, 0x3d9e, 0x3d9f, 0x3d9f, 0x3d9f, 0x3d9f,
    0x3d9f, 0x3da0, 0x3da0, 0x3da0, 0x3da0, 0x3da0, 0x3da1, 0x3da1, 0x3da1,
    0x3da1, 0x3da1, 0x3da2, 0x3da2, 0x3da2, 0x3da2, 0x3da2, 0x3da3, 0x3da3,
    0x3da3, 0x3da3, 0x3da3, 0x3da4, 0x3da4, 0x3da4, 0x3da4, 0x3da4, 0x3da5,
    0x3da5, 0x3da5, 0x3da5, 0x3da6, 0x3da6, 0x3da6, 0x3da6, 0x3da6, 0x3da7,
    0x3da7, 0x3da7, 0x3da7, 0x3da7, 0x3da8, 0x3da8, 0x3da8, 0x3da8, 0x3da8,
    0x3da9, 0x3da9, 0x3da9, 0x3da9, 0x3da9, 0x3daa, 0x3daa, 0x3daa, 0x3daa,
    0x3daa, 0x3dab, 0x3dab, 0x3dab, 0x3dab, 0x3dab, 0x3dac, 0x3dac, 0x3dac,
    0x3dac, 0x3dac, 0x3dad, 0x3dad, 0x3dad, 0x3dad, 0x3dad, 0x3daf, 0x3daf,
    0x3daf, 0x3daf, 0x3db0, 0x3db0, 0x3db0, 0x3db0, 0x3db0, 0x3db1, 0x3db1,
    0x3db1, 0x3db1, 0x3db1, 0x3db2, 0x3db2, 0x3db2, 0x3db2, 0x3db2, 0x3db3,
    0x3db3, 0x3db3, 0x3db3, 0x3db3, 0x3db4, 0x3db4, 0x3db4, 0x3db4, 0x3db4,
    0x3db5, 0x3db5, 0x3db5, 0x3db5, 0x3db5, 0x3db6, 0x3db6, 0x3db6, 0x3db6,
    0x3db6, 0x3db7, 0x3db7, 0x3db7, 0x3db7, 0x3db8, 0x3db8, 0x3db8, 0x3db8,
    0x3db8, 0x3db9, 0x3db9, 0x3db9, 0x3db9, 0x3db9, 0x3dba, 0x3dba, 0x3dba,
    0x3dba, 0x3dba, 0x3dbb, 0x3dbb, 0x3dbb, 0x3dbb, 0x3dbb, 0x3dbc, 0x3dbc,
    0x3dbc, 0x3dbc, 0x3dbc, 0x3dbd, 0x3dbd, 0x3dbd, 0x3dbd, 0x3dbd, 0x3dbe,
    0x3dbe, 0x3dbe, 0x3dbe, 0x3dbe, 0x3dbf, 0x3dbf, 0x3dbf, 0x3dbf, 0x3dbf,
    0x3dc0, 0x3dc0, 0x3dc0, 0x3dc0, 0x3dc1, 0x3dc1, 0x3dc1, 0x3dc1, 0x3dc1,
    0x3dc1, 0x3dc1, 0x3dc1, 0x3dc1, 0x3dc1, 0x3dc2, 0x3dc2, 0x3dc2, 0x3dc2,
    0x3dc2, 0x3dc3, 0x3dc3, 0x3dc3, 0x3dc3, 0x3dc3, 0x3dc4, 0x3dc4, 0x3dc4,
    0x3dc4, 0x3dc4, 0x3dc5, 0x3dc5, 0x3dc5, 0x3dc5, 0x3dc5, 0x3dc6, 0x3dc6,
    0x3dc6, 0x3dc6, 0x3dc6, 0x3dc7, 0x3dc7, 0x3dc7, 0x3dc7, 0x3dc7, 0x3dc8,
    0x3dc8, 0x3dc8, 0x3dc8, 0x3dc9, 0x3dc9, 0x3dc9, 0x3dc9, 0x3dc9, 0x3dca,
    0x3dca, 0x3dca, 0x3dca, 0x3dca, 0x3dcb, 0x3dcb, 0x3dcb, 0x3dcb, 0x3dcb,
    0x3dcc, 0x3dcd, 0x3dce, 0x3dcf, 0x3dd0, 0x3dd1, 0x3dd2, 0x3dd3, 0x3dd4,
    0x3dd5, 0x3dd6, 0x3dd7, 0x3dd8, 0x3dd9, 0x3dda, 0x3ddb, 0x3ddc, 0x3ddd,
    0x3dde, 0x3ddf, 0x3de0, 0x3de1, 0x3de2, 0x3de3, 0x3de4,
};

// <! gen atan f(x) = atan(x)
static double _gen_atan(float i)
{
  return atan(i);
}

static void tl_lut_ref(u16 *ofmap, u16 *ifmap, tl_shape_t ifmap_shape)
{
  assert(ofmap);

  for (u32 i = 0; i < tl_shape_size(&ifmap_shape); i++) {
    float f = convert_bf16_fp32(ifmap[i]);
    double v = _gen_atan(f);
    ofmap[i] = convert_fp32_bf16(v);

    if (mode == PRE_DATA_COMPARE_FIX) {
      ofmap[i] = golden_bf16[i];
    } else if (mode == DATA_COMPARE_U8) {
      ofmap[i] = (u8)convert_bf16_s8(ofmap[i]);
    }
  }
}


static bool verify(u16 *ofmap_data, u16 *ref_data, u16 *ifmap, u64 ifmap_size,
                   float epsilon)
{
  u64 size = ifmap_size;

  for (u64 i = 0; i < size; i++) {
    bool is_close;
    u16 ref = ref_data[i];
    u16 ofmap_data_bf16;
    float ref_f;
    float ofmap_data_f;

    ref_f = convert_bf16_fp32(ref);
    ofmap_data_f = convert_bf16_fp32(ofmap_data[i]);
    ofmap_data_bf16 = ofmap_data[i];

    if (mode == PRE_DATA_COMPARE_FIX) {
      is_close = ofmap_data[i] == ref;
    } else {
      is_close = fabs(ref_f - ofmap_data_f) < epsilon;
    }

    if (!is_close) {
      float input = convert_bf16_fp32(ifmap[i]);
      fprintf(stderr,
              "comparing failed at ofmap_data[%" PRIu64 "](input:%f)\n"
              "\tgot %x, exp %x, fp32: got %f exp %f, atan(%f) = %f\n",
              i, input, ofmap_data_bf16, ref, ofmap_data_f, ref_f, input,
              _gen_atan(input));
      exit(-1);
    }
  }

  return true;
}

static void gen_input(u16 *input_data, u64 ifmap_size, TEST_MODE mode,
                      int range_start, int range_end)
{

  if (mode == PRE_DATA_COMPARE_FIX) {
    memcpy(input_data, &test_pattern, sizeof(test_pattern));
  } else {
    std::random_device rd;
    std::mt19937 e2(rd());
    std::uniform_real_distribution<> dist(range_start, range_end);
    int table_hw = 256;
    for (u64 i = 0; i < ifmap_size; i++) {
      // input range is -8 ~ +8
      float input = ((int)i % (range_end - 2)) * (((int)i % 2) ? 1 : -1) +
                    0.03 + (i % table_hw) * 0.002;
      // float input = ((int)i % 10) * (((int)i % 2) ? 1 : -1) + 0.03 + (i %
      // table_hw) * 0.002;  float input = dist(e2);  input = ((int)i %
      // (range_end-2)) * (((int)i % 2) ? 1 : 1) + 0.03 + (i % table_hw) *
      // 0.002; if (input < 1 && input > 0) {
      //  input = 111.9;
      //}
      input_data[i] = convert_fp32_bf16(input);
    }
    input_data[0] = convert_fp32_bf16(0);
    input_data[1] = convert_fp32_bf16(1);
    input_data[2] = convert_fp32_bf16(-1);
  }

#ifdef DBG
  for (u64 i = 0; i < ifmap_size; i++) {
    printf("source if[%" PRIu64 "] bf16 %f 0x%x, log2f is %f\n", i,
           convert_bf16_fp32(input_data[i]), input_data[i],
           floor(log2((convert_bf16_fp32(input_data[i])))));
  }
#endif /* ifdef DBG */
}

static void testbench(CVI_RT_HANDLE *ctx, bmk_ctx_t *bmk)
{
  // TODO: check more shape / align
  bmk1880v2_chip_info_t chip_info = bmk1880v2_chip_info();

  u32 input_n = 1;
  u32 input_c = chip_info.npu_num;
  u32 input_h = 16;
  u32 input_w = 16;
  float epsilon = 0.01;
  int range_start = -8;
  int range_end = 8;
  fmt_t fmt = FMT_BF16;

  if (mode == PRE_DATA_COMPARE_FIX) {
    input_h = 4;
    input_w = 8;
  }

  tl_shape_t ifmap_shape = {input_n, input_c, input_h, input_w};
  tl_shape_t ofmap_shape = ifmap_shape;
  u64 ifmap_size = tl_shape_size(&ifmap_shape);
  u64 ofmap_size = tl_shape_size(&ofmap_shape);

  int data_type_size = bytesize_of_fmt(fmt);
  u64 ifmap_bytesize = ifmap_size * data_type_size;
  u64 ofmap_bytesize = ofmap_size * data_type_size;

  // get lut table shape and size
  tl_shape_t table_shape;
  u64 table_bytesize = bf16_lut_tbl_bytesize(bmk, &table_shape, fmt);

  tl_t *tl_ifmap = alloc_tl(bmk, ifmap_shape, fmt, /*align*/1);
  tl_t *tl_ofmap_bf16 = alloc_tl(bmk, ofmap_shape, fmt, /*align*/1);
  tl_t *out = tl_ofmap_bf16;

  // atan buf
  tl_t *tl_y0_buf = alloc_tl(bmk, table_shape, fmt, /*align*/1);
  tl_t *tl_slope_buf = alloc_tl(bmk, table_shape, fmt, /*align*/1);
  tl_t *tl_invert_buf = alloc_tl(bmk, table_shape, fmt, /*align*/1);
  tl_t *tl_pos_neg_buf = alloc_tl(bmk, table_shape, fmt, /*align*/1);

  // reciprocal buf
  tl_t *tl_reciprocal_table_answer =
      alloc_tl(bmk, table_shape, fmt, /*align*/1);
  tl_t *tl_reciprocal_table_answer_mantissa =
      alloc_tl(bmk, table_shape, fmt, /*align*/1);

  // temp buf
  tl_t *tl_buf = alloc_tl(bmk, ofmap_shape, fmt, /*align*/1);
  tl_t *tl_buf2 = alloc_tl(bmk, tl_ofmap_bf16->shape, fmt, /*align*/1);
  tl_t *tl_buf4 = alloc_tl(bmk, tl_ofmap_bf16->shape, fmt, /*align*/1);

  u16 *input_data = (u16 *)xmalloc(ifmap_bytesize);
  u16 *ref_data = (u16 *)xmalloc(ofmap_bytesize);

  // for reciprocal
  u16 *table_reciprocal_data = (u16 *)xmalloc(table_bytesize);
  u16 *table_reciprocal_data_mantissa = (u16 *)xmalloc(table_bytesize);

  // for atan
  u16 *table_data_atan_y0 = (u16 *)xmalloc(table_bytesize);
  u16 *table_data_atan_slope = (u16 *)xmalloc(table_bytesize);
  u16 *table_data_atan_invert = (u16 *)xmalloc(table_bytesize);
  u16 *table_data_atan_pos_neg = (u16 *)xmalloc(table_bytesize);

  gen_input(input_data, ifmap_size, mode, range_start, range_end);
  tl_lut_ref(ref_data, input_data, ifmap_shape);

  bf16_reciprocal_tbl(table_reciprocal_data, table_reciprocal_data_mantissa,
                      &table_shape);
  bf16_atan_tbl(table_data_atan_y0, table_data_atan_slope,
                table_data_atan_invert, table_data_atan_pos_neg, &table_shape);

  put_bf16_tensor_g2l(ctx, bmk, tl_ifmap, (u16 *)input_data, fmt);
  put_bf16_tensor_g2l(ctx, bmk, tl_reciprocal_table_answer,
                      (u16 *)table_reciprocal_data, fmt);
  put_bf16_tensor_g2l(ctx, bmk, tl_reciprocal_table_answer_mantissa,
                      (u16 *)table_reciprocal_data_mantissa, fmt);

  // prepare atan
  put_bf16_tensor_g2l(ctx, bmk, tl_y0_buf, (u16 *)table_data_atan_y0, fmt);
  put_bf16_tensor_g2l(ctx, bmk, tl_slope_buf, (u16 *)table_data_atan_slope,
                      fmt);
  put_bf16_tensor_g2l(ctx, bmk, tl_invert_buf, (u16 *)table_data_atan_invert,
                      fmt);
  put_bf16_tensor_g2l(ctx, bmk, tl_pos_neg_buf, (u16 *)table_data_atan_pos_neg,
                      fmt);

  bf16_atan_emit(bmk, tl_ifmap, tl_buf, tl_buf2, tl_buf4, tl_y0_buf,
                 tl_slope_buf, tl_invert_buf, tl_pos_neg_buf,
                 tl_reciprocal_table_answer,
                 tl_reciprocal_table_answer_mantissa, tl_ofmap_bf16, fmt);

  test_submit(ctx);

  u16 *ofmap_data = (u16 *)get_bf16_tensor_l2g(ctx, bmk, out, out->fmt);
  verify(ofmap_data, ref_data, input_data, ifmap_size, epsilon);

  free_tl(bmk, tl_buf4);
  free_tl(bmk, tl_buf2);
  free_tl(bmk, tl_buf);
  free_tl(bmk, tl_reciprocal_table_answer_mantissa);
  free_tl(bmk, tl_reciprocal_table_answer);
  free_tl(bmk, tl_pos_neg_buf);
  free_tl(bmk, tl_invert_buf);
  free_tl(bmk, tl_slope_buf);
  free_tl(bmk, tl_y0_buf);
  free_tl(bmk, tl_ofmap_bf16);
  free_tl(bmk, tl_ifmap);

  free(table_data_atan_y0);
  free(table_data_atan_slope);
  free(table_data_atan_invert);
  free(table_data_atan_pos_neg);
  free(table_reciprocal_data);
  free(table_reciprocal_data_mantissa);
  free(input_data);
  free(ref_data);
  free(ofmap_data);
}

int main()
{
  CVI_RT_HANDLE ctx;
  bmk_ctx_t *bmk;
  int round_mode;

  round_mode = set_store_feround();

  test_init(&ctx, &bmk);

  // for (int i = PRE_DATA_COMPARE_FIX; i < TEST_MODE_MAX; i++)
  // for (int i = PRE_DATA_COMPARE_FIX; i < DATA_COMPARE_ACCURACY; i++)
  for (int i = PRE_DATA_COMPARE_FIX; i < DATA_COMPARE_U8; i++)
  // for (int i = DATA_COMPARE_ACCURACY; i < DATA_COMPARE_U8; i++)
  {
    mode = static_cast<TEST_MODE>(i);
    printf("test mode %d...\n", mode);
    testbench(&ctx, bmk);
  }
  printf("pass\n");

  test_exit(&ctx);
  restore_feround(round_mode);
  return 0;
}
