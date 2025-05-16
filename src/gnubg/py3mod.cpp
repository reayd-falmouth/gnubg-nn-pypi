// gnubgmodule.cpp - Python 3 extension module for gnubg

#include <Python.h>

#if defined(_WIN32)
// Windows: use Win32 loader APIs
  #include <windows.h>

#elif defined(__unix__) || defined(__APPLE__)
// Unix/macOS: use dladdr() + dirname()
  #include <dlfcn.h>
  #include <libgen.h>

#endif

#include <string>
#include <iostream>
#include <fstream>
#include <cstdarg>

#include "stdutil.h"
#include "analyze.h"
#include "bm.h"
#include "player.h"
#include "equities.h"
#include "misc.h"

extern "C" {
    #include "eval.h"
    #include "inputs.h"
    #include "positionid.h"

    typedef struct NetDef NetDef;
    extern NetDef* nets[];    // pulls in the real definition from eval.c :contentReference[oaicite:0]{index=0}:contentReference[oaicite:1]{index=1}
    void initnet(void);       // this will populate nets[] :contentReference[oaicite:2]{index=2}:contentReference[oaicite:3]{index=3}
}

// Hook GNUBG's internal logging function
extern "C" void outputf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

#include "br.h"
#include "osr.h"

// bring in std::vector
#include <vector>

// shorthand 26‐element board for bearoff/resign logic
typedef short int AnalyzeBoard[26];

// The global “analyzer” used for the resign call
namespace {
    Player analyzer;
}

//-----------------------------------------------------------------------------
// Which “ply” to evaluate.  Must match your enum below.
enum {
    PLY_OSR      = -2,
    PLY_BEAROFF  = -3,
    PLY_PRUNE    = -4,
    PLY_1SBEAR   = -5,
    PLY_RACENET  = -6,
    PLY_1ANDHALF = -7,
    PLY_1SRACE   = -8,
};

unsigned int const nData = 400;

static float ocrData[nData][2] = {
        { /*1 , */ 1.0000,0.0000 } ,
        { /*2 , */ 1.0000,0.0000 } ,
        { /*3 , */ 1.0000,0.0000 } ,
        { /*4 , */ 1.0556,0.2291 } ,
        { /*5 , */ 1.1389,0.3458 } ,
        { /*6 , */ 1.2500,0.4330 } ,
        { /*7 , */ 1.3642,0.4876 } ,
        { /*8 , */ 1.5401,0.5226 } ,
        { /*9 , */ 1.6983,0.5234 } ,
        { /*10 , */ 1.8404,0.5097 } ,
        { /*11 , */ 1.9462,0.5277 } ,
        { /*12 , */ 2.0718,0.5433 } ,
        { /*13 , */ 2.1892,0.5794 } ,
        { /*14 , */ 2.2908,0.6378 } ,
        { /*15 , */ 2.4052,0.6880 } ,
        { /*16 , */ 2.5256,0.7294 } ,
        { /*17 , */ 2.6745,0.7213 } ,
        { /*18 , */ 2.7905,0.7534 } ,
        { /*19 , */ 2.9042,0.7878 } ,
        { /*20 , */ 3.0184,0.8244 } ,
        { /*21 , */ 3.1588,0.8089 } ,
        { /*22 , */ 3.2708,0.8484 } ,
        { /*23 , */ 3.3829,0.8884 } ,
        { /*24 , */ 3.5021,0.9203 } ,
        { /*25 , */ 3.6485,0.8860 } ,
        { /*26 , */ 3.7668,0.9129 } ,
        { /*27 , */ 3.8828,0.9424 } ,
        { /*28 , */ 4.0056,0.9610 } ,
        { /*29 , */ 4.1278,0.9795 } ,
        { /*30 , */ 4.2492,0.9983 } ,
        { /*31 , */ 4.3696,1.0191 } ,
        { /*32 , */ 4.4950,1.0303 } ,
        { /*33 , */ 4.6199,1.0417 } ,
        { /*34 , */ 4.7419,1.0577 } ,
        { /*35 , */ 4.8628,1.0764 } ,
        { /*36 , */ 4.9854,1.0918 } ,
        { /*37 , */ 5.1091,1.1044 } ,
        { /*38 , */ 5.2300,1.1231 } ,
        { /*39 , */ 5.3517,1.1401 } ,
        { /*40 , */ 5.4742,1.1555 } ,
        { /*41 , */ 5.5988,1.1647 } ,
        { /*42 , */ 5.7207,1.1805 } ,
        { /*43 , */ 5.8423,1.1966 } ,
        { /*44 , */ 5.9646,1.2114 } ,
        { /*45 , */ 6.0881,1.2221 } ,
        { /*46 , */ 6.2100,1.2374 } ,
        { /*47 , */ 6.3318,1.2528 } ,
        { /*48 , */ 6.4544,1.2662 } ,
        { /*49 , */ 6.5777,1.2772 } ,
        { /*50 , */ 6.6999,1.2909 } ,
        { /*51 , */ 6.8220,1.3050 } ,
        { /*52 , */ 6.9446,1.3175 } ,
        { /*53 , */ 7.0672,1.3300 } ,
        { /*54 , */ 7.1895,1.3430 } ,
        { /*55 , */ 7.3118,1.3561 } ,
        { /*56 , */ 7.4344,1.3682 } ,
        { /*57 , */ 7.5570,1.3799 } ,
        { /*58 , */ 7.6793,1.3924 } ,
        { /*59 , */ 7.8017,1.4048 } ,
        { /*60 , */ 7.9242,1.4168 } ,
        { /*61 , */ 8.0467,1.4283 } ,
        { /*62 , */ 8.1691,1.4404 } ,
        { /*63 , */ 8.2915,1.4524 } ,
        { /*64 , */ 8.4139,1.4640 } ,
        { /*65 , */ 8.5365,1.4751 } ,
        { /*66 , */ 8.6589,1.4867 } ,
        { /*67 , */ 8.7813,1.4982 } ,
        { /*68 , */ 8.9038,1.5095 } ,
        { /*69 , */ 9.0263,1.5205 } ,
        { /*70 , */ 9.1487,1.5317 } ,
        { /*71 , */ 9.2711,1.5429 } ,
        { /*72 , */ 9.3936,1.5538 } ,
        { /*73 , */ 9.5161,1.5645 } ,
        { /*74 , */ 9.6385,1.5754 } ,
        { /*75 , */ 9.7609,1.5862 } ,
        { /*76 , */ 9.8834,1.5968 } ,
        { /*77 , */ 10.0058,1.6073 } ,
        { /*78 , */ 10.1283,1.6179 } ,
        { /*79 , */ 10.2507,1.6284 } ,
        { /*80 , */ 10.3732,1.6387 } ,
        { /*81 , */ 10.4956,1.6490 } ,
        { /*82 , */ 10.6181,1.6593 } ,
        { /*83 , */ 10.7405,1.6695 } ,
        { /*84 , */ 10.8630,1.6796 } ,
        { /*85 , */ 10.9854,1.6896 } ,
        { /*86 , */ 11.1079,1.6997 } ,
        { /*87 , */ 11.2303,1.7096 } ,
        { /*88 , */ 11.3528,1.7195 } ,
        { /*89 , */ 11.4752,1.7293 } ,
        { /*90 , */ 11.5977,1.7391 } ,
        { /*91 , */ 11.7201,1.7488 } ,
        { /*92 , */ 11.8426,1.7585 } ,
        { /*93 , */ 11.9650,1.7681 } ,
        { /*94 , */ 12.0875,1.7777 } ,
        { /*95 , */ 12.2099,1.7872 } ,
        { /*96 , */ 12.3324,1.7967 } ,
        { /*97 , */ 12.4548,1.8061 } ,
        { /*98 , */ 12.5773,1.8154 } ,
        { /*99 , */ 12.6997,1.8247 } ,
        { /*100 , */ 12.8222,1.8340 } ,
        { /*101 , */ 12.9446,1.8432 } ,
        { /*102 , */ 13.0671,1.8524 } ,
        { /*103 , */ 13.1895,1.8616 } ,
        { /*104 , */ 13.3120,1.8706 } ,
        { /*105 , */ 13.4344,1.8797 } ,
        { /*106 , */ 13.5569,1.8887 } ,
        { /*107 , */ 13.6793,1.8976 } ,
        { /*108 , */ 13.8018,1.9065 } ,
        { /*109 , */ 13.9242,1.9154 } ,
        { /*110 , */ 14.0466,1.9242 } ,
        { /*111 , */ 14.1691,1.9331 } ,
        { /*112 , */ 14.2915,1.9418 } ,
        { /*113 , */ 14.4140,1.9505 } ,
        { /*114 , */ 14.5364,1.9592 } ,
        { /*115 , */ 14.6589,1.9678 } ,
        { /*116 , */ 14.7813,1.9764 } ,
        { /*117 , */ 14.9038,1.9850 } ,
        { /*118 , */ 15.0262,1.9935 } ,
        { /*119 , */ 15.1487,2.0020 } ,
        { /*120 , */ 15.2711,2.0104 } ,
        { /*121 , */ 15.3936,2.0189 } ,
        { /*122 , */ 15.5160,2.0272 } ,
        { /*123 , */ 15.6385,2.0356 } ,
        { /*124 , */ 15.7609,2.0439 } ,
        { /*125 , */ 15.8834,2.0522 } ,
        { /*126 , */ 16.0058,2.0604 } ,
        { /*127 , */ 16.1283,2.0686 } ,
        { /*128 , */ 16.2507,2.0768 } ,
        { /*129 , */ 16.3732,2.0850 } ,
        { /*130 , */ 16.4956,2.0931 } ,
        { /*131 , */ 16.6181,2.1012 } ,
        { /*132 , */ 16.7405,2.1092 } ,
        { /*133 , */ 16.8630,2.1173 } ,
        { /*134 , */ 16.9854,2.1253 } ,
        { /*135 , */ 17.1079,2.1332 } ,
        { /*136 , */ 17.2303,2.1411 } ,
        { /*137 , */ 17.3528,2.1490 } ,
        { /*138 , */ 17.4752,2.1569 } ,
        { /*139 , */ 17.5977,2.1648 } ,
        { /*140 , */ 17.7201,2.1726 } ,
        { /*141 , */ 17.8426,2.1804 } ,
        { /*142 , */ 17.9650,2.1881 } ,
        { /*143 , */ 18.0875,2.1959 } ,
        { /*144 , */ 18.2099,2.2036 } ,
        { /*145 , */ 18.3324,2.2113 } ,
        { /*146 , */ 18.4548,2.2189 } ,
        { /*147 , */ 18.5773,2.2265 } ,
        { /*148 , */ 18.6997,2.2341 } ,
        { /*149 , */ 18.8222,2.2417 } ,
        { /*150 , */ 18.9446,2.2493 } ,
        { /*151 , */ 19.0671,2.2568 } ,
        { /*152 , */ 19.1895,2.2643 } ,
        { /*153 , */ 19.3120,2.2718 } ,
        { /*154 , */ 19.4344,2.2793 } ,
        { /*155 , */ 19.5569,2.2867 } ,
        { /*156 , */ 19.6793,2.2941 } ,
        { /*157 , */ 19.8018,2.3015 } ,
        { /*158 , */ 19.9242,2.3088 } ,
        { /*159 , */ 20.0466,2.3161 } ,
        { /*160 , */ 20.1691,2.3235 } ,
        { /*161 , */ 20.2915,2.3307 } ,
        { /*162 , */ 20.4140,2.3380 } ,
        { /*163 , */ 20.5364,2.3452 } ,
        { /*164 , */ 20.6589,2.3525 } ,
        { /*165 , */ 20.7813,2.3597 } ,
        { /*166 , */ 20.9038,2.3669 } ,
        { /*167 , */ 21.0262,2.3740 } ,
        { /*168 , */ 21.1487,2.3811 } ,
        { /*169 , */ 21.2711,2.3883 } ,
        { /*170 , */ 21.3936,2.3953 } ,
        { /*171 , */ 21.5160,2.4024 } ,
        { /*172 , */ 21.6385,2.4095 } ,
        { /*173 , */ 21.7609,2.4165 } ,
        { /*174 , */ 21.8834,2.4235 } ,
        { /*175 , */ 22.0058,2.4305 } ,
        { /*176 , */ 22.1283,2.4375 } ,
        { /*177 , */ 22.2507,2.4444 } ,
        { /*178 , */ 22.3732,2.4513 } ,
        { /*179 , */ 22.4956,2.4582 } ,
        { /*180 , */ 22.6181,2.4651 } ,
        { /*181 , */ 22.7405,2.4720 } ,
        { /*182 , */ 22.8630,2.4788 } ,
        { /*183 , */ 22.9854,2.4857 } ,
        { /*184 , */ 23.1079,2.4925 } ,
        { /*185 , */ 23.2303,2.4993 } ,
        { /*186 , */ 23.3528,2.5061 } ,
        { /*187 , */ 23.4752,2.5128 } ,
        { /*188 , */ 23.5977,2.5196 } ,
        { /*189 , */ 23.7201,2.5263 } ,
        { /*190 , */ 23.8426,2.5330 } ,
        { /*191 , */ 23.9650,2.5397 } ,
        { /*192 , */ 24.0875,2.5463 } ,
        { /*193 , */ 24.2099,2.5530 } ,
        { /*194 , */ 24.3324,2.5597 } ,
        { /*195 , */ 24.4548,2.5662 } ,
        { /*196 , */ 24.5773,2.5728 } ,
        { /*197 , */ 24.6997,2.5794 } ,
        { /*198 , */ 24.8222,2.5860 } ,
        { /*199 , */ 24.9446,2.5925 } ,
        { /*200 , */ 25.0671,2.5991 } ,
        { /*201 , */ 25.1895,2.6056 } ,
        { /*202 , */ 25.3120,2.6121 } ,
        { /*203 , */ 25.4344,2.6186 } ,
        { /*204 , */ 25.5569,2.6251 } ,
        { /*205 , */ 25.6793,2.6315 } ,
        { /*206 , */ 25.8018,2.6379 } ,
        { /*207 , */ 25.9242,2.6443 } ,
        { /*208 , */ 26.0466,2.6508 } ,
        { /*209 , */ 26.1691,2.6572 } ,
        { /*210 , */ 26.2915,2.6635 } ,
        { /*211 , */ 26.4140,2.6699 } ,
        { /*212 , */ 26.5364,2.6762 } ,
        { /*213 , */ 26.6589,2.6826 } ,
        { /*214 , */ 26.7813,2.6889 } ,
        { /*215 , */ 26.9038,2.6952 } ,
        { /*216 , */ 27.0262,2.7015 } ,
        { /*217 , */ 27.1487,2.7077 } ,
        { /*218 , */ 27.2711,2.7140 } ,
        { /*219 , */ 27.3936,2.7202 } ,
        { /*220 , */ 27.5160,2.7264 } ,
        { /*221 , */ 27.6385,2.7327 } ,
        { /*222 , */ 27.7609,2.7389 } ,
        { /*223 , */ 27.8834,2.7451 } ,
        { /*224 , */ 28.0058,2.7512 } ,
        { /*225 , */ 28.1283,2.7574 } ,
        { /*226 , */ 28.2507,2.7635 } ,
        { /*227 , */ 28.3732,2.7697 } ,
        { /*228 , */ 28.4956,2.7757 } ,
        { /*229 , */ 28.6181,2.7819 } ,
        { /*230 , */ 28.7405,2.7880 } ,
        { /*231 , */ 28.8630,2.7940 } ,
        { /*232 , */ 28.9854,2.8001 } ,
        { /*233 , */ 29.1079,2.8061 } ,
        { /*234 , */ 29.2303,2.8122 } ,
        { /*235 , */ 29.3528,2.8183 } ,
        { /*236 , */ 29.4752,2.8242 } ,
        { /*237 , */ 29.5977,2.8302 } ,
        { /*238 , */ 29.7201,2.8362 } ,
        { /*239 , */ 29.8426,2.8422 } ,
        { /*240 , */ 29.9650,2.8481 } ,
        { /*241 , */ 30.0875,2.8541 } ,
        { /*242 , */ 30.2099,2.8600 } ,
        { /*243 , */ 30.3324,2.8659 } ,
        { /*244 , */ 30.4548,2.8718 } ,
        { /*245 , */ 30.5773,2.8778 } ,
        { /*246 , */ 30.6997,2.8836 } ,
        { /*247 , */ 30.8222,2.8895 } ,
        { /*248 , */ 30.9446,2.8954 } ,
        { /*249 , */ 31.0671,2.9012 } ,
        { /*250 , */ 31.1895,2.9071 } ,
        { /*251 , */ 31.3120,2.9129 } ,
        { /*252 , */ 31.4344,2.9187 } ,
        { /*253 , */ 31.5569,2.9245 } ,
        { /*254 , */ 31.6793,2.9303 } ,
        { /*255 , */ 31.8017,2.9361 } ,
        { /*256 , */ 31.9242,2.9418 } ,
        { /*257 , */ 32.0466,2.9476 } ,
        { /*258 , */ 32.1691,2.9533 } ,
        { /*259 , */ 32.2915,2.9591 } ,
        { /*260 , */ 32.4140,2.9649 } ,
        { /*261 , */ 32.5364,2.9706 } ,
        { /*262 , */ 32.6589,2.9763 } ,
        { /*263 , */ 32.7813,2.9820 } ,
        { /*264 , */ 32.9038,2.9876 } ,
        { /*265 , */ 33.0262,2.9933 } ,
        { /*266 , */ 33.1487,2.9990 } ,
        { /*267 , */ 33.2711,3.0046 } ,
        { /*268 , */ 33.3936,3.0102 } ,
        { /*269 , */ 33.5160,3.0159 } ,
        { /*270 , */ 33.6385,3.0215 } ,
        { /*271 , */ 33.7609,3.0271 } ,
        { /*272 , */ 33.8834,3.0327 } ,
        { /*273 , */ 34.0058,3.0384 } ,
        { /*274 , */ 34.1283,3.0439 } ,
        { /*275 , */ 34.2507,3.0494 } ,
        { /*276 , */ 34.3732,3.0550 } ,
        { /*277 , */ 34.4956,3.0605 } ,
        { /*278 , */ 34.6181,3.0661 } ,
        { /*279 , */ 34.7405,3.0716 } ,
        { /*280 , */ 34.8630,3.0772 } ,
        { /*281 , */ 34.9854,3.0826 } ,
        { /*282 , */ 35.1079,3.0881 } ,
        { /*283 , */ 35.2303,3.0936 } ,
        { /*284 , */ 35.3528,3.0990 } ,
        { /*285 , */ 35.4752,3.1045 } ,
        { /*286 , */ 35.5977,3.1099 } ,
        { /*287 , */ 35.7201,3.1154 } ,
        { /*288 , */ 35.8426,3.1209 } ,
        { /*289 , */ 35.9650,3.1264 } ,
        { /*290 , */ 36.0875,3.1317 } ,
        { /*291 , */ 36.2099,3.1371 } ,
        { /*292 , */ 36.3324,3.1426 } ,
        { /*293 , */ 36.4548,3.1479 } ,
        { /*294 , */ 36.5773,3.1533 } ,
        { /*295 , */ 36.6997,3.1587 } ,
        { /*296 , */ 36.8222,3.1640 } ,
        { /*297 , */ 36.9446,3.1694 } ,
        { /*298 , */ 37.0671,3.1748 } ,
        { /*299 , */ 37.1895,3.1801 } ,
        { /*300 , */ 37.3120,3.1854 } ,
        { /*301 , */ 37.4344,3.1907 } ,
        { /*302 , */ 37.5568,3.1961 } ,
        { /*303 , */ 37.6793,3.2013 } ,
        { /*304 , */ 37.8017,3.2066 } ,
        { /*305 , */ 37.9242,3.2119 } ,
        { /*306 , */ 38.0466,3.2172 } ,
        { /*307 , */ 38.1691,3.2225 } ,
        { /*308 , */ 38.2915,3.2277 } ,
        { /*309 , */ 38.4140,3.2329 } ,
        { /*310 , */ 38.5364,3.2382 } ,
        { /*311 , */ 38.6589,3.2435 } ,
        { /*312 , */ 38.7813,3.2487 } ,
        { /*313 , */ 38.9038,3.2539 } ,
        { /*314 , */ 39.0262,3.2591 } ,
        { /*315 , */ 39.1487,3.2643 } ,
        { /*316 , */ 39.2711,3.2695 } ,
        { /*317 , */ 39.3936,3.2747 } ,
        { /*318 , */ 39.5160,3.2798 } ,
        { /*319 , */ 39.6385,3.2850 } ,
        { /*320 , */ 39.7609,3.2902 } ,
        { /*321 , */ 39.8834,3.2953 } ,
        { /*322 , */ 40.0058,3.3006 } ,
        { /*323 , */ 40.1283,3.3056 } ,
        { /*324 , */ 40.2507,3.3107 } ,
        { /*325 , */ 40.3732,3.3159 } ,
        { /*326 , */ 40.4956,3.3210 } ,
        { /*327 , */ 40.6181,3.3261 } ,
        { /*328 , */ 40.7405,3.3312 } ,
        { /*329 , */ 40.8630,3.3362 } ,
        { /*330 , */ 40.9854,3.3413 } ,
        { /*331 , */ 41.1079,3.3464 } ,
        { /*332 , */ 41.2303,3.3515 } ,
        { /*333 , */ 41.3528,3.3565 } ,
        { /*334 , */ 41.4752,3.3616 } ,
        { /*335 , */ 41.5977,3.3666 } ,
        { /*336 , */ 41.7201,3.3717 } ,
        { /*337 , */ 41.8426,3.3767 } ,
        { /*338 , */ 41.9650,3.3817 } ,
        { /*339 , */ 42.0875,3.3867 } ,
        { /*340 , */ 42.2099,3.3917 } ,
        { /*341 , */ 42.3324,3.3967 } ,
        { /*342 , */ 42.4548,3.4018 } ,
        { /*343 , */ 42.5773,3.4067 } ,
        { /*344 , */ 42.6997,3.4116 } ,
        { /*345 , */ 42.8222,3.4166 } ,
        { /*346 , */ 42.9446,3.4216 } ,
        { /*347 , */ 43.0671,3.4265 } ,
        { /*348 , */ 43.1895,3.4315 } ,
        { /*349 , */ 43.3119,3.4364 } ,
        { /*350 , */ 43.4344,3.4413 } ,
        { /*351 , */ 43.5568,3.4463 } ,
        { /*352 , */ 43.6793,3.4512 } ,
        { /*353 , */ 43.8017,3.4561 } ,
        { /*354 , */ 43.9242,3.4610 } ,
        { /*355 , */ 44.0466,3.4659 } ,
        { /*356 , */ 44.1691,3.4708 } ,
        { /*357 , */ 44.2915,3.4757 } ,
        { /*358 , */ 44.4140,3.4805 } ,
        { /*359 , */ 44.5364,3.4855 } ,
        { /*360 , */ 44.6589,3.4902 } ,
        { /*361 , */ 44.7813,3.4952 } ,
        { /*362 , */ 44.9038,3.5000 } ,
        { /*363 , */ 45.0262,3.5048 } ,
        { /*364 , */ 45.1487,3.5097 } ,
        { /*365 , */ 45.2711,3.5145 } ,
        { /*366 , */ 45.3936,3.5193 } ,
        { /*367 , */ 45.5160,3.5242 } ,
        { /*368 , */ 45.6385,3.5290 } ,
        { /*369 , */ 45.7609,3.5338 } ,
        { /*370 , */ 45.8834,3.5385 } ,
        { /*371 , */ 46.0058,3.5434 } ,
        { /*372 , */ 46.1283,3.5481 } ,
        { /*373 , */ 46.2507,3.5529 } ,
        { /*374 , */ 46.3732,3.5577 } ,
        { /*375 , */ 46.4956,3.5625 } ,
        { /*376 , */ 46.6181,3.5672 } ,
        { /*377 , */ 46.7405,3.5719 } ,
        { /*378 , */ 46.8630,3.5767 } ,
        { /*379 , */ 46.9854,3.5815 } ,
        { /*380 , */ 47.1079,3.5862 } ,
        { /*381 , */ 47.2303,3.5909 } ,
        { /*382 , */ 47.3528,3.5956 } ,
        { /*383 , */ 47.4752,3.6004 } ,
        { /*384 , */ 47.5977,3.6051 } ,
        { /*385 , */ 47.7201,3.6097 } ,
        { /*386 , */ 47.8426,3.6144 } ,
        { /*387 , */ 47.9650,3.6191 } ,
        { /*388 , */ 48.0875,3.6238 } ,
        { /*389 , */ 48.2099,3.6284 } ,
        { /*390 , */ 48.3324,3.6332 } ,
        { /*391 , */ 48.4548,3.6379 } ,
        { /*392 , */ 48.5773,3.6424 } ,
        { /*393 , */ 48.6997,3.6472 } ,
        { /*394 , */ 48.8222,3.6518 } ,
        { /*395 , */ 48.9446,3.6564 } ,
        { /*396 , */ 49.0671,3.6610 } ,
        { /*397 , */ 49.1895,3.6657 } ,
        { /*398 , */ 49.3119,3.6704 } ,
        { /*399 , */ 49.4344,3.6751 } ,
        { /*400 , */ 49.5568,3.6996 } ,
};

float
ocr(unsigned int n, float* stdv)
{
    if( ! (0 < n && n <= nData) ) {
        return 0.0;
    }

    if( stdv ) {
        *stdv = ocrData[n-1][1];
    }

    return ocrData[n-1][0];
}

// ------------------------------------------------------------------
// Given a 10-byte GNUBG “auch” key, emit the 20-char A–Z position ID
static const char*
posString(const unsigned char* auch)
{
    static char buf[21];
    for (int i = 0; i < 10; ++i) {
        buf[2*i]   = 'A' + ((auch[i] >> 4) & 0xF);
        buf[2*i+1] = 'A' +  (auch[i]        & 0xF);
    }
    buf[20] = '\0';
    return buf;
}

//-----------------------------------------------------------------------------
// Convert a 20‐char A–Z key into the 10‐byte “auch” array:
static unsigned char*
auchFromString(const char* str)
{
    // str must be length 20
    static unsigned char auch[10];
    for (int i = 0; i < 10; ++i) {
        int hi = str[2*i]   - 'A';
        int lo = str[2*i+1] - 'A';
        auch[i] = static_cast<unsigned char>((hi<<4) | lo);
    }
    return auch;
}

//─── setBoard for resign logic ─────────────────────────────────────────────────
static void
setBoard(AnalyzeBoard out, const int board[2][25])
{
    // bar of + on [0]
    out[0] = board[1][24];
    // points 1..24
    for (int k = 0; k < 24; ++k) {
        int val = 0;
        if      (board[1][23-k] > 0) { val = board[1][23-k]; }
        else if (board[0][k] > 0)    { val = -board[0][k]; }
        out[k+1] = val;
    }
    // bar of - at index 25
    out[25] = -board[0][24];
}

//─── boardString: turn a C board into the “PositionKey” string ─────────────────
static const char*
boardString(const int board[2][25])
{
    unsigned char auch[10];
    // PositionKey comes from <eval.h>; it wants Board = int[2][25]
    PositionKey((int(*)[25])board, auch);

    // now convert 10 nybbles to 20-char A…Z string
    static char buf[21];
    for (int i = 0; i < 10; ++i) {
        buf[2*i]   = 'A' + ((auch[i] >> 4) & 0xF);
        buf[2*i+1] = 'A' + (auch[i] & 0xF);
    }
    buf[20] = '\0';
    return buf;
}

//-----------------------------------------------------------------------------
// stringToBoard(key, board):
//   key is either a 20-char A–Z string or a 14-char base64 ID;
//   fills int board[2][25] and returns true on success.
static bool
stringToBoard(const char* key, int board[2][25])
{
    size_t len = strlen(key);
    if (len == 20) {
        // interpret as PositionKey
        PositionFromKey(board, auchFromString(key));
        return true;
    }
    else if (len == 14) {
        // interpret as PositionID
        PositionFromID(board, key);
        return true;
    }
    // otherwise invalid
    return false;
}

//-----------------------------------------------------------------------------
// Converter for the “ply” argument: accepts a Python int and checks the range
static int
readPly(PyObject* obj, void* out_p)
{
    int& nPlies = *static_cast<int*>(out_p);
    if (!PyLong_Check(obj)) {
        PyErr_SetString(PyExc_TypeError, "ply must be an integer");
        return 0;
    }
    long v = PyLong_AsLong(obj);
    // valid if non-negative or one of our special negative codes
    if (v >= 0 || (v <= PLY_OSR && v >= PLY_1ANDHALF)) {
        nPlies = (int)v;
        return 1;
    }
    PyErr_SetString(PyExc_ValueError, "invalid ply");
    return 0;
}

//-----------------------------------------------------------------------------
// Convert a Python object (26-element sequence or string) into
// a 26-entry AnalyzeBoard (short[26]).  Returns 1 on success, 0 on error.
static int
anyAnalyzeBoard(PyObject* o, void* out_ptr)
{
    // out_ptr really points at an AnalyzeBoard:
    short* board = static_cast<short*>(out_ptr);

    // Case 1: a sequence of length 26
    if (PySequence_Check(o) && PySequence_Size(o) == 26) {
        PyObject* seq = PySequence_Fast(o, "expected 26-element sequence");
        if (!seq) return 0;
        long s0 = 0, s1 = 0;
        for (Py_ssize_t k = 0; k < 26; ++k) {
            PyObject* item = PySequence_Fast_GET_ITEM(seq, k);
            if (!PyLong_Check(item)) {
                PyErr_SetString(PyExc_TypeError, "board entries must be ints");
                Py_DECREF(seq);
                return 0;
            }
            long v = PyLong_AsLong(item);
            board[k] = static_cast<short>(v);
            if (v > 0)      s0 += v;
            else if (v < 0) s1 += -v;
        }
        Py_DECREF(seq);
        if (!(s0 <= 15 && s1 <= 15)) {
            PyErr_Format(PyExc_ValueError,
                         "Invalid board (x has %ld, o has %ld)", s0, s1);
            return 0;
        }
        return 1;
    }

    // Case 2: a Unicode string key/ID
    if (PyUnicode_Check(o)) {
        const char* s = PyUnicode_AsUTF8(o);
        if (!s) return 0;  // error already set

        // First decode into the 2×25 int board:
        int temp[2][25];
        if (!stringToBoard(s, temp)) {
            // stringToBoard should set ValueError if invalid
            return 0;
        }
        // Then convert to AnalyzeBoard layout:
        setBoard(board, temp);
        return 1;
    }

    PyErr_SetString(PyExc_ValueError,
                    "Expected 26-element list or position key string");
    return 0;
}

//-----------------------------------------------------------------------------
// pack_board: turn a C int[2][25] into a Python tuple of two 25-tuples
static PyObject*
pack_board(const int board[2][25])
{
    PyObject* outer = PyTuple_New(2);
    if (!outer) return NULL;

    for (int s = 0; s < 2; ++s) {
        PyObject* inner = PyTuple_New(25);
        if (!inner) {
            Py_DECREF(outer);
            return NULL;
        }
        for (int p = 0; p < 25; ++p) {
            PyTuple_SET_ITEM(inner, p,
                             PyLong_FromLong(board[s][p]));
        }
        PyTuple_SET_ITEM(outer, s, inner);
    }
    return outer;
}

//─── anyBoard converter ─────────────────────────────────────────────────────────
static int
anyBoard(PyObject* obj, void* out_board)
{
    // reinterpret the void* as int[2][25]
    int (*board)[25] = reinterpret_cast<int (*)[25]>(out_board);
    if (!PySequence_Check(obj) || PySequence_Size(obj) != 2) {
        PyErr_SetString(PyExc_ValueError, "Expected 2-element sequence of 25-element sequences");
        return 0;
    }
    for (int s = 0; s < 2; ++s) {
        PyObject* row = PySequence_GetItem(obj, s);
        if (!PySequence_Check(row) || PySequence_Size(row) != 25) {
            Py_XDECREF(row);
            PyErr_SetString(PyExc_ValueError, "Each side must be length 25");
            return 0;
        }
        for (int p = 0; p < 25; ++p) {
            PyObject* v = PySequence_GetItem(row, p);
            long x = PyLong_AsLong(v);
            board[s][p] = (int)x;
            Py_DECREF(v);
        }
        Py_DECREF(row);
    }
    return 1;
}

static PyObject* py_boardfromid(PyObject* self, PyObject* args) {
    const char* pos_id;
    if (!PyArg_ParseTuple(args, "s", &pos_id)) {
        return NULL;
    }

    int board[2][25] = {{0}};
    PositionFromID(board, pos_id);

    PyObject* outer = PyList_New(2);
    for (int s = 0; s < 2; ++s) {
        PyObject* inner = PyList_New(25);
        for (int p = 0; p < 25; ++p) {
            PyList_SetItem(inner, p, PyLong_FromLong(board[s][p]));
        }
        PyList_SetItem(outer, s, inner);
    }

    return outer;
}

// Helper: Convert Python list to C int array
bool PyList_ToBoard(PyObject* listObj, int board[2][25]) {
    if (!PyList_Check(listObj) || PyList_Size(listObj) != 2)
        return false;

    for (int s = 0; s < 2; ++s) {
        PyObject* side = PyList_GetItem(listObj, s);
        if (!PyList_Check(side) || PyList_Size(side) != 25)
            return false;

        for (int p = 0; p < 25; ++p) {
            PyObject* val = PyList_GetItem(side, p);
            board[s][p] = PyLong_AsLong(val);
        }
    }
    return true;
}

static PyObject* py_classify(PyObject* self, PyObject* args) {
    PyObject* boardObj;
    if (!PyArg_ParseTuple(args, "O", &boardObj)) return NULL;

    int board[2][25];
    if (!PyList_ToBoard(boardObj, board)) {
        PyErr_SetString(PyExc_ValueError, "Expected 2x25 board list");
        return NULL;
    }

    int cls = ClassifyPosition(board);
    return PyLong_FromLong(cls);
}

static PyObject* py_pubbestmove(PyObject* self, PyObject* args) {
    PyObject* boardObj;
    int dice0, dice1;
    if (!PyArg_ParseTuple(args, "Oii", &boardObj, &dice0, &dice1)) return NULL;

    int board[2][25];
    int move[8] = {0};

    if (!PyList_ToBoard(boardObj, board)) {
        PyErr_SetString(PyExc_ValueError, "Expected 2x25 board list");
        return NULL;
    }

    int n = FindPubevalMove(dice0, dice1, board, move);
    if (n < 0) {
        PyErr_SetString(PyExc_RuntimeError, "No valid move found");
        return NULL;
    }

    PyObject* result = PyList_New(n);
    for (int i = 0; i < n; ++i) {
        PyList_SetItem(result, i, PyLong_FromLong(move[i]));
    }
    return result;
}

static PyObject* py_pubevalscore(PyObject* self, PyObject* args) {
    PyObject* boardObj;
    if (!PyArg_ParseTuple(args, "O", &boardObj)) return NULL;

    int board[2][25];
    if (!PyList_ToBoard(boardObj, board)) {
        PyErr_SetString(PyExc_ValueError, "Expected 2x25 board list");
        return NULL;
    }

    int fRace = ClassifyPosition(board) <= CLASS_RACE;
    float score = pubEvalVal(fRace, board);

    return PyFloat_FromDouble(score);
}

static PyObject* py_id(PyObject* self, PyObject* args) {
    PyObject* boardObj;
    if (!PyArg_ParseTuple(args, "O", &boardObj)) return NULL;

    int board[2][25];
    if (!PyList_ToBoard(boardObj, board)) {
        PyErr_SetString(PyExc_ValueError, "Expected 2x25 board list");
        return NULL;
    }

    const char* id = PositionID(board);
    return PyUnicode_FromString(id);
}

static PyObject* py_rolldice(PyObject* self, PyObject* args) {
    int dice[2];
    RollDice(dice);
    return Py_BuildValue("ii", dice[0], dice[1]);
}

static PyObject*
py_bestmove(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    // args/kwargs locals
    int board_arr[2][25];
    int dice1, dice2;
    int nPlies = 0;
    char side = 0;
    int moveBoard = 0;
    int resignInfo = 0;
    int listMoves = 0;
    int reduced = 0;

    static const char *kwlist[] = {
            "pos","dice1","dice2",
            "n","s","b","r","list","reduced", NULL
    };

    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs,
            "O&ii|iciiii",
            const_cast<char**>(kwlist),
            anyBoard, board_arr,
            &dice1, &dice2,
            &nPlies, &side,
            &moveBoard, &resignInfo,
            &listMoves, &reduced
    )) {
        return NULL;
    }

    // Validate inputs
    if (nPlies < 0) {
        PyErr_SetString(PyExc_ValueError, "negative ply");
        return NULL;
    }
    if (dice1 < 0 || dice1 > 6 || dice2 < 0 || dice2 > 6) {
        PyErr_SetString(PyExc_ValueError, "invalid dice");
        return NULL;
    }

    bool xOnPlay;
    switch (side) {
        case 'X': case 'x': xOnPlay = true;  break;
        case 'O': case 'o': xOnPlay = false; break;
        case 0:    xOnPlay = false; break;  // default
        default:
            PyErr_SetString(PyExc_ValueError, "invalid side");
            return NULL;
    }

    std::vector<MoveRecord>* recs = nullptr;
    if (listMoves) recs = new std::vector<MoveRecord>;

    int move_buf[8];
    int n = findBestMove(
            move_buf, dice1, dice2,
            board_arr, xOnPlay, nPlies,
            recs, reduced
    );

    // pack the primary move‐tuple
    PyObject* moves = PyTuple_New(n/2);
    for (int j = 0; j < n/2; ++j) {
        int k = 2*j;
        int from = move_buf[k]>=0 ? move_buf[k]+1 : 0;
        int to   = move_buf[k+1]>=0 ? move_buf[k+1]+1 : 0;
        PyObject* mv2 = PyTuple_New(2);
        PyTuple_SET_ITEM(mv2, 0, PyLong_FromLong(from));
        PyTuple_SET_ITEM(mv2, 1, PyLong_FromLong(to));
        PyTuple_SET_ITEM(moves, j, mv2);
    }

    // if no extra info requested, return just that
    if (!moveBoard && !resignInfo && !listMoves) {
        delete recs;
        return moves;
    }

    // otherwise build the result tuple
    int extra = moveBoard + resignInfo + listMoves;
    PyObject* result = PyTuple_New(1 + extra);
    PyTuple_SET_ITEM(result, 0, moves);

    int idx = 1;

    // 1) new board?
    if (moveBoard) {
        SwapSides(board_arr);
        PyObject* bstr = PyUnicode_FromString(boardString(board_arr));
        PyTuple_SET_ITEM(result, idx++, bstr);
        SwapSides(board_arr);
    }

    // 2) resign info?
    if (resignInfo) {
        int r = 0;
        if (isRace(board_arr)) {
            AnalyzeBoard b;  // your short‐hand 26‐element array
            setBoard(b, board_arr);
            r = analyzer.offerResign(nPlies, 2, b, true);
        }
        PyTuple_SET_ITEM(result, idx++,
                         PyLong_FromLong(r));
    }

    // 3) full move list?
    if (listMoves) {
        PyObject* listT = PyTuple_New(recs->size());
        for (size_t i = 0; i < recs->size(); ++i) {
            const auto& ri = (*recs)[i];
            PyObject* entry = PyTuple_New(4);

            // position key
            PyTuple_SET_ITEM(entry, 0, PyUnicode_FromString(posString(ri.pos)));

            // the individual move steps
            const int *mv = ri.move;
            int cnt = 0;
            while (cnt<4 && mv[2*cnt]>=0) ++cnt;
            PyObject* mvsteps = PyTuple_New(2*cnt);
            for (int j=0; j<cnt; ++j) {
                int f = mv[2*j]>=0 ? mv[2*j]+1 : 0;
                int t = mv[2*j+1]>=0 ? mv[2*j+1]+1 : 0;
                PyTuple_SET_ITEM(mvsteps,2*j,   PyLong_FromLong(f));
                PyTuple_SET_ITEM(mvsteps,2*j+1, PyLong_FromLong(t));
            }
            PyTuple_SET_ITEM(entry,1, mvsteps);

            // probabilities array
            PyObject* probs =
                    PyTuple_New(5);
            for (int k=0; k<5; ++k) {
                PyTuple_SET_ITEM(
                        probs, k,
                        PyFloat_FromDouble(ri.probs[k])
                );
            }
            PyTuple_SET_ITEM(entry,2, probs);

            // match‐score
            PyTuple_SET_ITEM(
                    entry,3,
                    PyFloat_FromDouble(ri.matchScore)
            );

            PyTuple_SET_ITEM(listT,i, entry);
        }
        PyTuple_SET_ITEM(result, idx++, listT);
        delete recs;
    }

    return result;
}

// ------------------------------------------------------------------
// keyofboard(pos: sequence) -> str
static PyObject*
py_keyofboard(PyObject* /*self*/, PyObject* args)
{
    int board_arr[2][25];
    // anyBoard will fill our 2×25 array from a Python sequence
    if (!PyArg_ParseTuple(args, "O&", anyBoard, board_arr)) {
        return NULL;
    }
    // boardString returns the 20-char A–Z key
    const char* key = boardString(board_arr);
    return PyUnicode_FromString(key);
}

//-----------------------------------------------------------------------------
// Python wrapper: boardfromkey(key: str) -> ((25,), (25,))
static PyObject*
py_boardfromkey(PyObject* /*self*/, PyObject* args)
{
    const char* key;
    if (!PyArg_ParseTuple(args, "s", &key))
        return NULL;

    int board[2][25];
    if (!stringToBoard(key, board)) {
        PyErr_SetString(PyExc_ValueError, "invalid board key");
        return NULL;
    }
    return pack_board(board);
}

//-----------------------------------------------------------------------------
// Converter: read either an int or a 6-tuple of bearoff points, producing
// a single bearoff-ID in *pi.
static int
readBearoffId(PyObject* obj, void* pi)
{
    int* out = static_cast<int*>(pi);

    // Case 1: a simple integer
    if (PyLong_Check(obj)) {
        long v = PyLong_AsLong(obj);
        if (!(1 <= v && v < 54264)) {
            PyErr_SetString(
                    PyExc_ValueError,
                    "bearoff id outside of range [1,54264)"
            );
            return 0;
        }
        *out = (int)v;
        return 1;
    }

    // Case 2: a length-6 sequence of pip-counts
    if (PySequence_Check(obj) && PySequence_Size(obj) == 6) {
        PyObject* seq = PySequence_Fast(obj, "expected sequence of length 6");
        if (!seq) return 0;

        int p[6];
        for (int k = 0; k < 6; ++k) {
            PyObject* item = PySequence_Fast_GET_ITEM(seq, k);
            long x = PyLong_AsLong(item);
            if (PyErr_Occurred()) {
                Py_DECREF(seq);
                return 0;
            }
            p[k] = (int)x;
        }
        Py_DECREF(seq);

        *out = PositionBearoff(p);
        return 1;
    }

    PyErr_SetString(PyExc_TypeError,
                    "invalid type for bearoff id (int or len-6 sequence required)");
    return 0;
}

static PyObject*
py_bearoffid2pos(PyObject* /*self*/, PyObject* args)
{
    int id;
    // O& with readBearoffId picks up either an integer or a length-6 sequence
    if (!PyArg_ParseTuple(args, "O&", readBearoffId, &id)) {
        return NULL;  // parse error
    }

    int p[6];
    // Fills p[0]..p[5] from the GNUBG bearoff DB
    PositionFromBearoff(p, id);

    // Build a 6-tuple of ints
    return Py_BuildValue(
            "(iiiiii)",
            p[0], p[1], p[2],
            p[3], p[4], p[5]
    );
}

//-----------------------------------------------------------------------------
// Python–3 binding for bearoff probabilities:
//   bearoffprobs(id_or_6tuple) -> tuple of floats
static PyObject*
py_bearoffprobs(PyObject* /*self*/, PyObject* args)
{
    int id;
    // readBearoffId handles either an integer or a 6-element tuple
    if (!PyArg_ParseTuple(args, "O&", readBearoffId, &id)) {
        return NULL;
    }

    // B is GNUBG’s bearoff‐prob struct:
    B b;
    getBearoff(id, &b);

    // number of leading zeros:
    Py_ssize_t zeros = (Py_ssize_t)(b.start - 1);
    Py_ssize_t total = zeros + (Py_ssize_t)b.len;
    PyObject* tup = PyTuple_New(total);
    if (!tup) return NULL;

    // fill leading zeros
    for (Py_ssize_t k = 0; k < zeros; ++k) {
        PyTuple_SET_ITEM(tup, k, PyFloat_FromDouble(0.0));
    }
    // fill the actual probabilities
    for (unsigned int k = 0; k < b.len; ++k) {
        PyTuple_SET_ITEM(
                tup,
                zeros + (Py_ssize_t)k,
                PyFloat_FromDouble(b.p[k])
        );
    }
    return tup;
}

//-----------------------------------------------------------------------------
// moves(board, die1, die2, verbose=0) -> tuple of move-strings or (string,tuple) pairs
static PyObject*
py_moves(PyObject* /*self*/, PyObject* args)
{
    int board_arr[2][25];
    int die1, die2;
    int verbose = 0;

    // parse: O& (anyBoard), ii, optionally one int
    if (!PyArg_ParseTuple(args, "O&ii|i",
                          anyBoard, board_arr,
                          &die1, &die2,
                          &verbose)) {
        return NULL;
    }

    // generate the moves
    movelist ml;
    GenerateMoves(&ml, board_arr, die1, die2);

    // build the result tuple
    PyObject* out = PyTuple_New(ml.cMoves);
    if (!out) return NULL;

    for (unsigned int k = 0; k < (unsigned int)ml.cMoves; ++k) {
        const move& mv = ml.amMoves[k];
        const char* key = posString(mv.auch);

        if (!verbose) {
            // just the 20-char key
            PyTuple_SET_ITEM(
                    out, k,
                    PyUnicode_FromString(key)
            );
        }
        else {
            // build the moves‐tuple for this entry
            unsigned int n = 0;
            while (n < 8 && mv.anMove[n] >= 0) n += 2;

            PyObject* mvSteps = PyTuple_New(n/2);
            for (unsigned int j = 0; j < n/2; ++j) {
                int from = mv.anMove[2*j] >= 0
                           ? mv.anMove[2*j] + 1
                           : 0;
                int to   = mv.anMove[2*j+1] >= 0
                           ? mv.anMove[2*j+1] + 1
                           : 0;
                PyObject* pair = PyTuple_New(2);
                PyTuple_SET_ITEM(pair, 0, PyLong_FromLong(from));
                PyTuple_SET_ITEM(pair, 1, PyLong_FromLong(to));
                PyTuple_SET_ITEM(mvSteps, j, pair);
            }

            // package (key, mvSteps)
            PyObject* entry = PyTuple_New(2);
            PyTuple_SET_ITEM(
                    entry, 0,
                    PyUnicode_FromString(key)
            );
            PyTuple_SET_ITEM(entry, 1, mvSteps);

            PyTuple_SET_ITEM(out, k, entry);
        }
    }

    return out;
}

//-----------------------------------------------------------------------------
// probs(board, nPlies, nr=1296) -> 5‐tuple of floats
static PyObject*
py_probs(PyObject* /*self*/, PyObject* args)
{
    int board_arr[2][25];
    int nPlies;
    unsigned int nr = 1296;

    // note: "O&O&|I"  →  two converter slots, then optional unsigned‐int
    if (!PyArg_ParseTuple(
            args,
            "O&O&|I",
            anyBoard, board_arr,
            readPly,  &nPlies,
            &nr
    )) {
        return NULL;
    }

    float p[5];
    switch(nPlies) {
        case PLY_OSR:
            if (!isRace(board_arr)) {
                PyErr_SetString(PyExc_RuntimeError, "Not a race position");
                return NULL;
            }
            raceProbs(board_arr, p, nr);
            break;
        case PLY_BEAROFF:
            EvaluatePositionToBO(board_arr, p, false);
            break;
        case PLY_PRUNE:
            evalPrune(board_arr, p);
            break;
        case PLY_1SBEAR:
            EvalBearoffOneSided(board_arr, p);
            break;
        case PLY_RACENET:
            NetEvalRace(board_arr, p);
            break;
        case PLY_1ANDHALF: {
            float p1[5];
            EvaluatePosition(board_arr, p, 0,0,0,0,0,0);
            EvaluatePosition(board_arr, p1,1,0,0,0,0,0);
            for (int k=0; k<5; ++k) p[k] = 0.5f*(p[k]+p1[k]);
            break;
        }
        case PLY_1SRACE:
#if defined(OS_BEAROFF_DB)
            if (!EvalOSrace(board_arr, p)) {
            PyErr_SetString(PyExc_RuntimeError,"can't eval OS race DB");
            return NULL;
        }
        break;
#else
            PyErr_SetString(PyExc_RuntimeError,"OS race DB not available");
            return NULL;
#endif
        default:
            EvaluatePosition(board_arr, p, nPlies,0,0,0,0,0);
            break;
    }

    PyObject* out = PyTuple_New(5);
    if (!out) return NULL;
    for (int k = 0; k < 5; ++k) {
        PyTuple_SET_ITEM(out, k, PyFloat_FromDouble(p[k]));
    }
    return out;
}

//-----------------------------------------------------------------------------
// rollout(pos, ngames=1296, n=0, level=Analyze::AUTO, nt=500, std=0)
//    -> (p0,p1,p2,p3,p4)  or  ((p0..p4),(ars0..ars4))
static PyObject*
py_rollout(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    // our 26-element board rep (signed short)
    AnalyzeBoard board;

    // defaults
    int cGames    = 1296;
    int nPlies    = 0;
    Analyze::RolloutEndsAt level = Analyze::AUTO;
    int nTruncate = 500;
    int wantSts   = 0;

    static const char *kwlist[] = {
            "pos", "ngames", "n", "level", "nt", "std", NULL
    };

    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs,
            "O&|iiiii",      // one converter + 5 optional ints
            const_cast<char**>(kwlist),
            anyAnalyzeBoard, // fills board[]
            &cGames,
            &nPlies,
            &level,
            &nTruncate,
            &wantSts
    )) {
        return NULL;
    }

    if (cGames <= 0) {
        PyErr_Format(PyExc_ValueError,
                     "Invalid number of games (%d)", cGames);
        return NULL;
    }

    float p[NUM_OUTPUTS];
    float ars[NUM_OUTPUTS];

    // run the rollout
    analyzer.rollout(
            board,
            /*money=*/false,
            p, ars,
            nPlies,
            nTruncate,
            cGames,
            level
    );

    // if std==0, return just the p[5]
    if (!wantSts) {
        return Py_BuildValue(
                "ddddd",
                (double)p[0], (double)p[1], (double)p[2],
                (double)p[3], (double)p[4]
        );
    }

    // otherwise return ((p0..p4),(ars0..ars4))
    return Py_BuildValue(
            "((ddddd)(ddddd))",
            (double)p[0],   (double)p[1],
            (double)p[2],   (double)p[3],
            (double)p[4],

            (double)ars[0], (double)ars[1],
            (double)ars[2], (double)ars[3],
            (double)ars[4]
    );
}

//-----------------------------------------------------------------------------
// cubefullRollout(pos, ngames=576, side='X', ply=0) -> 13-element tuple of floats
static PyObject*
py_cubefullRollout(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    // Define default values for the arguments
    int nGames = 576;
    int nPlies = 0;
    char side = 'X';

    // Board representation
    AnalyzeBoard board;

    // Define the list of keywords for the argument parsing
    static const char* kwlist[] = {"pos", "ngames", "side", "ply", NULL};

    // Parse the input arguments
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&|ci", (char**)kwlist,
                                     anyAnalyzeBoard, board,
                                     &nGames, &side, &nPlies)) {
        return NULL;
    }

    // Check the side input and set the flag for which player is on play
    bool xOnPlay = false;
    switch (side) {
        case 'X': case 'x':
            xOnPlay = true; break;
        case 'O': case 'o':
            xOnPlay = false; break;
        default:
            PyErr_SetString(PyExc_ValueError, "Invalid side. Expected 'X' or 'O'.");
            return NULL;
    }

    // Set the board to the Analyze structure
    Analyze::GNUbgBoard anBoard;
    Analyze::set(board, anBoard, xOnPlay);

    // Perform the rollout
    const float* result = analyzer.rolloutCubefull(anBoard, nPlies, nGames, xOnPlay);

    // Prepare the result tuple with 13 floats
    PyObject* tuple = PyTuple_New(13);
    if (!tuple) return NULL;

    // Fill the tuple with the values from the result array
    for (int i = 0; i < 13; ++i) {
        PyTuple_SET_ITEM(tuple, i, PyFloat_FromDouble(result[i]));
    }

    return tuple;
}

static PyObject*
py_set_equities(PyObject* /*self*/, PyObject* const args)
{
    const char* which;

    // Case 1: One argument (string)
    if (PyTuple_Size(args) == 1) {
        if (!PyArg_ParseTuple(args, "s", &which)) {
            return NULL;
        }

        if (strcasecmp("gnur", which) == 0) {
            Equities::set(Equities::gnur);
        } else if (strcasecmp("jacobs", which) == 0) {
            Equities::set(Equities::Jacobs);
        } else if (strcasecmp("woolsey", which) == 0) {
            Equities::set(Equities::WoolseyHeinrich);
        } else if (strcasecmp("snowie", which) == 0) {
            Equities::set(Equities::Snowie);
        } else if (strcasecmp("mec26", which) == 0) {
            Equities::set(Equities::mec26);
        } else if (strcasecmp("mec57", which) == 0) {
            Equities::set(Equities::mec57);
        } else {
            PyErr_SetString(PyExc_RuntimeError, "Not a valid equities table name");
            return NULL;
        }
    }
        // Case 2: Two arguments (doubles for weights and growth rates)
    else if (PyTuple_Size(args) == 2) {
        double w, gr;
        if (!PyArg_ParseTuple(args, "dd", &w, &gr)) {
            return NULL;
        }

        if (!(w > 0.0 && w < 1.0 && gr > 0.0 && gr < 1.0)) {
            PyErr_SetString(PyExc_ValueError, "Weights and growth rates must be in [0, 1] range");
            return NULL;
        }

        Equities::set(w, gr);
    } else {
        // Wrong number of arguments
        PyErr_SetString(PyExc_TypeError, "Expected either one string or two floats (weight, growth_rate)");
        return NULL;
    }

    // Return None (success)
    Py_INCREF(Py_None);
    return Py_None;
}

// Seed function
static PyObject* set_seed(PyObject* self, PyObject* args) {
    unsigned long seed;
    if (!PyArg_ParseTuple(args, "l", &seed)) {
        return NULL;
    }

    // Set the seed in the random number generator
    Analyze::srandom(seed);

    Py_INCREF(Py_None);
    return Py_None;
}

// Shortcuts function
static PyObject* set_shortcuts(PyObject* self, PyObject* args) {
    int use;
    if (!PyArg_ParseTuple(args, "i", &use)) {
        return NULL;
    }

    // Enable or disable evaluation shortcuts
    setShortCuts(use);

    Py_INCREF(Py_None);
    return Py_None;
}

// Enable/Disable OS database function
static PyObject* set_osdb(PyObject* self, PyObject* args) {
    int use;
    if (!PyArg_ParseTuple(args, "i", &use)) {
        return NULL;
    }

    // Enable or disable the OS database
    if (use) {
        enableOSdb();
    } else {
        disableOSdb();
    }

    Py_INCREF(Py_None);
    return Py_None;
}

// Set move filters function
static PyObject* set_ps(PyObject* self, PyObject* args) {
    int nPlies, nMoves, nAdditional;
    double threshold;
    if (!PyArg_ParseTuple(args, "iiid", &nPlies, &nMoves, &nAdditional, &threshold)) {
        return NULL;
    }

    // Set the move filters
    setPlyBounds(nPlies, nMoves, nAdditional, threshold);

    Py_INCREF(Py_None);
    return Py_None;
}

// Set equities table function
//static PyObject* set_equities(PyObject* self, PyObject* args) {
//    const char* which;
//    if (PyTuple_Size(args) == 1) {
//        if (!PyArg_ParseTuple(args, "s", &which)) {
//            return NULL;
//        }
//
//        if (strcasecmp("gnur", which) == 0) {
//            Equities::set(Equities::gnur);
//        } else if (strcasecmp("jacobs", which) == 0) {
//            Equities::set(Equities::Jacobs);
//        } else if (strcasecmp("woolsey", which) == 0) {
//            Equities::set(Equities::WoolseyHeinrich);
//        } else if (strcasecmp("snowie", which) == 0) {
//            Equities::set(Equities::Snowie);
//        } else if (strcasecmp("mec26", which) == 0) {
//            Equities::set(Equities::mec26);
//        } else if (strcasecmp("mec57", which) == 0) {
//            Equities::set(Equities::mec57);
//        } else {
//            PyErr_SetString(PyExc_RuntimeError, "Not a valid equities table name");
//            return NULL;
//        }
//    } else if (PyTuple_Size(args) == 2) {
//        double w, gr;
//        if (!PyArg_ParseTuple(args, "dd", &w, &gr)) {
//            return NULL;
//        }
//
//        if (!(w > 0.0 && w < 1.0 && gr > 0.0 && gr < 1.0)) {
//            PyErr_SetString(PyExc_ValueError, "Weights and growth rates must be in [0, 1] range");
//            return NULL;
//        }
//
//        // Set the custom equities table
//        Equities::set(w, gr);
//    } else {
//        PyErr_SetString(PyExc_TypeError, "Expected either one string or two floats (weight, growth_rate)");
//        return NULL;
//    }
//
//    Py_INCREF(Py_None);
//    return Py_None;
//}

// Set match score function
static PyObject* set_score(PyObject* self, PyObject* args) {
    int usAway, opAway, crawford = 0;
    if (!PyArg_ParseTuple(args, "ii|i", &usAway, &opAway, &crawford)) {
        return NULL;
    }

    if (usAway < 0 || opAway < 0 || (crawford < 0 || crawford > 1)) {
        return NULL;
    }

    if (crawford == 1 && !((opAway == 1 && usAway > 1) || (opAway > 1 && usAway == 1))) {
        PyErr_SetString(PyExc_RuntimeError, "Away not compatible with crawford");
        return NULL;
    }

    // Set the match score in the analyzer
    analyzer.setScore(opAway, usAway);
    analyzer.crawford(crawford);

    Py_INCREF(Py_None);
    return Py_None;
}

// Set match cube function
static PyObject* set_cube(PyObject* self, PyObject* args) {
    int cube;
    char x = 0;
    if (!PyArg_ParseTuple(args, "i|c", &cube, &x)) {
        return NULL;
    }

    if (cube <= 0) {
        return NULL;
    }

    bool xOwn = false;
    if (cube > 1) {
        if (x == 0) {
            return NULL;
        }

        if (tolower(x) == 'x') {
            xOwn = true;
        } else if (tolower(x) == 'o') {
            xOwn = false;
        } else {
            return NULL;
        }
    }

    // Set the cube in the analyzer
    analyzer.setCube(cube, xOwn);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* equities_value(PyObject* /*self*/, PyObject* const args)
{
    int xAway, oAway;

    if (!PyArg_ParseTuple(args, "ii", &xAway, &oAway)) {
        return NULL;
    }

    if (!(0 <= xAway && xAway <= 25 &&  0 <= oAway && oAway <= 25)) {
        PyErr_SetString(PyExc_ValueError, "Score not in 0-25 range");
        return NULL;
    }

    // Call the Equities class static method to get the value
    float v = Equities::value(xAway, oAway);

    return PyFloat_FromDouble(v);
}

static PyObject*
py_onecrace(PyObject*, PyObject* const args)
{
    int n;

    // Parse the argument (an integer)
    if (!PyArg_ParseTuple(args, "i", &n)) {
        return NULL;
    }

    float stdv;
    float v = ocr(n, &stdv);  // Call the ocr function

    if (v == 0.0) {
        return NULL;  // If OCR returns 0.0, we return None
    }

    // Return the result as a tuple of two floats (v and stdv)
    return Py_BuildValue("dd", v, stdv);
}


static PyMethodDef GnubgMethods[] = {
        {"classify", py_classify, METH_VARARGS, "Classify a board position."},
        {"pubbestmove", py_pubbestmove, METH_VARARGS, "Get best move using public evaluation."},
        {"boardfromid", py_boardfromid, METH_VARARGS, "Convert a GNUBG Position ID to a board list"},
        {"boardfromkey",
                (PyCFunction)py_boardfromkey,
                METH_VARARGS,
                "boardfromkey(key: str) -> (two 25-tuples)" },
        { "keyofboard",
                py_keyofboard,
                METH_VARARGS,
                "keyofboard(board) -> position key string" },
        {"id", py_id, METH_VARARGS, "Convert board to Position ID (base64)"},
        {"pubevalscore", py_pubevalscore, METH_VARARGS, "Public evaluation score for the board"},
        {"roll", py_rolldice, METH_VARARGS, "Roll two dice (uses GNUBG RNG)"},
        { "bestmove",
                (PyCFunction)py_bestmove,
                METH_VARARGS|METH_KEYWORDS,
                "bestmove(pos, dice1, dice2, n=0, s=0, b=0, r=0, list=0, reduced=0)\n"
                "Returns either a tuple of moves, or (moves, board?, resign?, list?)."
        },
        { "bearoffid2pos",
                py_bearoffid2pos,
                METH_VARARGS,
                "bearoffid2pos(id) -> tuple of 6 ints giving the bearoff position" },
        { "bearoffprobs",
                py_bearoffprobs,
                METH_VARARGS,
                "bearoffprobs(id_or_6tuple) → tuple of bearoff probabilities" },
        { "moves",
                py_moves,
                METH_VARARGS,
                "moves(board, die1, die2, verbose=0) → tuple of move-keys or (key,steps) pairs" },
        { "probs",
                py_probs,
                METH_VARARGS,
                "probs(board, nPlies, nr=1296)\n\n"
                "Evaluate a position:\n"
                " - board: 2×25 list\n"
                " - nPlies: one of the PLY_* constants\n"
                " - nr: roll count for OSR\n\n"
                "Returns a 5‐tuple of floats (win, gamm, backgammon, etc.)"
        },
        { "rollout",
                (PyCFunction)py_rollout,
                METH_VARARGS | METH_KEYWORDS,
                "rollout(pos, ngames=1296, n=0, level, nt=500, std=0)\n"
                "Perform a rollout:\n"
                "  pos    : 26-element list or key\n"
                "  ngames : number of simulated games\n"
                "  n      : number of plies per rollout\n"
                "  level  : Analyze::AUTO|RACE|BEAROFF|OVER\n"
                "  nt     : truncation length\n"
                "  std    : if nonzero, also return stderr array\n\n"
                "Returns a 5-tuple of floats, or if std=1 a pair of 5-tuples." },
        { "crollout",
                (PyCFunction)py_cubefullRollout,
                METH_VARARGS | METH_KEYWORDS,
                "cubefullRollout(pos, ngames=576, side='X', ply=0) -> 13-element tuple\n"
                "Simulate a set of cube rolls and return the results as 13 floats." },
//        {"equities", (PyCFunction)py_set_equities, METH_VARARGS, "Set the equities table or set custom weight and growth rate."},
        {"onecrace", py_onecrace, METH_VARARGS, "OCR function to compute value and standard deviation for a given number."},
        {NULL, NULL, 0, NULL}
};

// Method table for the 'equities' submodule
static PyMethodDef gnubg_equities_methods[] = {
        {"value", equities_value, METH_VARARGS, "Get the equities value for a given position."},
        {NULL, NULL, 0, NULL}  // Sentinel to mark the end of the method table
};

// Create a PyModuleDef for the 'equities' submodule
static PyModuleDef gnubg_equities_module = {
        PyModuleDef_HEAD_INIT,  // Standard header for all Python modules
        "equities",             // Name of the submodule
        "Equities-related methods for GNUBG", // Module docstring
        -1,                     // Size of the module, -1 means no state
        gnubg_equities_methods  // Method table
};

// Define the method table for the 'set' submodule
static PyMethodDef gnubg_set_methods[] = {
        {"seed", set_seed, METH_VARARGS, "Set internal random generator seed."},
        {"shortcuts", set_shortcuts, METH_VARARGS, "Set evaluation shortcuts."},
        {"osdb", set_osdb, METH_VARARGS, "Enable/Disable OS database"},
        {"ps", set_ps, METH_VARARGS, "Set move filters"},
        {"equities", py_set_equities, METH_VARARGS, "Set equities table to use"},
        {"score", set_score, METH_VARARGS, "Set match score"},
        {"cube", set_cube, METH_VARARGS, "Set match cube"},
        {NULL, NULL, 0, NULL}  // Sentinel to mark the end of the method table
};

// Define the 'set' submodule
static PyModuleDef gnubg_set_module = {
        PyModuleDef_HEAD_INIT,
        "set",            // Module name
        "Set various parameters for the GNUBG engine", // Module docstring
        -1,               // Size of the module state, -1 means no state
        gnubg_set_methods // Method table
};


static struct PyModuleDef gnubgmodule = {
        PyModuleDef_HEAD_INIT,
        "gnubg",
        "Python bindings for GNUBG neural net evaluation",
        -1,
        GnubgMethods
};

//---------------------------------------------------------------------------
// Cross‐platform helper to locate the “data/” alongside the shared module.
static std::string find_data_dir(PyObject* module) {
#if defined(_WIN32)
    HMODULE h = NULL;
    // Get handle for this module
    if (GetModuleHandleExA(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          (LPCSTR)&PyInit_gnubg, &h)) {
        char buf[MAX_PATH];
        if (GetModuleFileNameA(h, buf, MAX_PATH)) {
            std::string path(buf);
            auto pos = path.find_last_of("\\/");
            return path.substr(0, pos) + "\\data";
        }
    }
    return ".";

#elif defined(__unix__) || defined(__APPLE__)
    Dl_info dl;
    if (dladdr((void*)&find_data_dir, &dl) && dl.dli_fname) {
        char* dup = strdup(dl.dli_fname);
        std::string base = dirname(dup);
        free(dup);
        return base + "/data";
    }
    return ".";

#else
    // Fallback: Python’s __file__
    PyObject* fobj = PyModule_GetFilenameObject(module);
    if (!fobj) {
        PyErr_Clear();
        return ".";
    }
    const char* fname = PyUnicode_AsUTF8(fobj);
    Py_DECREF(fobj);
    if (!fname) return ".";
    char* dup = strdup(fname);
    std::string base = dirname(dup);
    free(dup);
    return base + "/data";
#endif
}

//---------------------------------------------------------------------------
// Module initialization
PyMODINIT_FUNC
PyInit_gnubg(void)
{
    // Initialize the module
    PyObject* m = PyModule_Create(&gnubgmodule);
    if (m == NULL) {
        return NULL;
    }

    // Create the 'equities' submodule
    PyObject* equities_module = PyModule_Create(&gnubg_equities_module);
    if (equities_module == NULL) {
        return NULL;
    }

    // Add the 'equities' submodule to the main module
    PyModule_AddObject(m, "equities", equities_module);

    // Create the 'set' submodule
    PyObject* set_module = PyModule_Create(&gnubg_set_module);
    if (set_module == NULL) {
        return NULL;
    }

    // Add the 'set' submodule to the main module
    PyModule_AddObject(m, "set", set_module);

    // Get the path to the GNUBG data directory
//    Dl_info dl_info;
//    dladdr((void*)PyInit_gnubg, &dl_info);
//    // Determine where we shipped our data
//    std::string base    = dirname(strdup(dl_info.dli_fname));
//    std::string datadir = base + "/data";

    // Locate our shipped data directory
    std::string datadir = find_data_dir(m);
    #ifndef _WIN32
        // on Unix we used forward‐slash
    #else
        // on Win32 convert to forward‐slash for Analyze::init()
        for (auto &c : datadir) if (c == '\\') c = '/';
    #endif

    // If the user hasn't already pointed GNUBGHOME somewhere, default it
    if (!std::getenv("GNUBGHOME")) {
        // third argument “1” means “always overwrite”, but since getenv was null
        // this just sets it for our process before Analyze::init()
        setenv("GNUBGHOME", datadir.c_str(), 1);
        //        std::cout << "Defaulting GNUBGHOME to: " << datadir << std::endl;
    }
    //    else {
    //        std::cout << "GNUBGHOME set to: " << std::getenv("GNUBGHOME") << std::endl;
    //    }

    // Debug: Print the base path and data directory
    //    std::cout << "Base path: " << base << std::endl;
    //    std::cout << "Data directory: " << datadir << std::endl;

    // Define paths to required data files
    std::string weights = datadir + "/gnubg.weights";
    std::string os_bd   = datadir + "/gnubg_os.db";
    std::string ts0_bd  = datadir + "/gnubg_ts0.bd";
    std::string os0_bd  = datadir + "/gnubg_os0.bd";

    // Debug: Print the paths to the data files
    //    std::cout << "Looking for weights at: " << weights << std::endl;
    //    std::cout << "Looking for os_bd at: " << os_bd << std::endl;
    //    std::cout << "Looking for ts0_bd at: " << ts0_bd << std::endl;
    //    std::cout << "Looking for os0_bd at: " << os0_bd << std::endl;

    // Initialize GNUBG (loads all six nets into the global `nets[]`)
    if (!Analyze::init(weights.c_str())) {
        PyErr_SetString(PyExc_ImportError,
                        "Analyze::init() failed to load GNUBG neural nets");
        return NULL;
    }

    // Optional SSE optimizations
    useSSE(1);

    // Expose constants for CLASS_* categories
    PyModule_AddIntConstant(m, "c_over", CLASS_OVER);
    PyModule_AddIntConstant(m, "c_bearoff", CLASS_BEAROFF1);
    PyModule_AddIntConstant(m, "c_race", CLASS_RACE);
    PyModule_AddIntConstant(m, "c_crashed", CLASS_CRASHED);
#if defined(CONTAINMENT_CODE)
    PyModule_AddIntConstant(m, "c_backcontain", CLASS_BACKCONTAIN);
#endif
    PyModule_AddIntConstant(m, "c_contact", CLASS_CONTACT);

    // Expose constants for PLY_* categories
    PyModule_AddIntConstant(m, "p_osr", PLY_OSR);
    PyModule_AddIntConstant(m, "p_bearoff", PLY_BEAROFF);
    PyModule_AddIntConstant(m, "p_prune", PLY_PRUNE);
    PyModule_AddIntConstant(m, "p_1sbear", PLY_1SBEAR);
    PyModule_AddIntConstant(m, "p_race", PLY_RACENET);
    PyModule_AddIntConstant(m, "p_1srace", PLY_1SRACE);
    PyModule_AddIntConstant(m, "p_0plus1", PLY_1ANDHALF);

    // Expose constants for Analyze::RolloutEndsAt categories
    PyModule_AddIntConstant(m, "ro_race", Analyze::RACE);
    PyModule_AddIntConstant(m, "ro_bearoff", Analyze::BEAROFF);
    PyModule_AddIntConstant(m, "ro_over", Analyze::OVER);
    PyModule_AddIntConstant(m, "ro_auto", Analyze::AUTO);

    // Finally return the Python extension module
    return m;
}
