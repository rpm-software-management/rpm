/** \ingroup py_c  
 * \file python/rpmbc-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "rpmbc-py.h"

#include "rpmdebug-py.c"

#include "debug.h"

/*@unchecked@*/
static int _bc_debug = 0;

#define is_rpmbc(o)	((o)->ob_type == &rpmbc_Type)

/*@unchecked@*/ /*@observer@*/
static const char initialiser_name[] = "rpmbc";

/*@unchecked@*/ /*@observer@*/
static const struct {
  /* Number of digits in the conversion base that always fits in an mp_limb_t.
     For example, for base 10 on a machine where a mp_limb_t has 32 bits this
     is 9, since 10**9 is the largest number that fits into a mp_limb_t.  */
  int chars_per_limb;

  /* log(2)/log(conversion_base) */
  double chars_per_bit_exactly;

  /* base**chars_per_limb, i.e. the biggest number that fits a word, built by
     factors of base.  Exception: For 2, 4, 8, etc, big_base is log2(base),
     i.e. the number of bits used to represent each digit in the base.  */
  unsigned int big_base;

  /* A BITS_PER_MP_LIMB bit approximation to 1/big_base, represented as a
     fixed-point number.  Instead of dividing by big_base an application can
     choose to multiply by big_base_inverted.  */
  unsigned int big_base_inverted;
} mp_bases[257] = {
  /*  0 */ {0, 0.0, 0, 0},
  /*  1 */ {0, 1e37, 0, 0},
  /*  2 */ {32, 1.0000000000000000, 0x1, 0x0},
  /*  3 */ {20, 0.6309297535714574, 0xcfd41b91, 0x3b563c24},
  /*  4 */ {16, 0.5000000000000000, 0x2, 0x0},
  /*  5 */ {13, 0.4306765580733931, 0x48c27395, 0xc25c2684},
  /*  6 */ {12, 0.3868528072345416, 0x81bf1000, 0xf91bd1b6},
  /*  7 */ {11, 0.3562071871080222, 0x75db9c97, 0x1607a2cb},
  /*  8 */ {10, 0.3333333333333333, 0x3, 0x0},
  /*  9 */ {10, 0.3154648767857287, 0xcfd41b91, 0x3b563c24},
  /* 10 */ {9, 0.3010299956639812, 0x3b9aca00, 0x12e0be82},
  /* 11 */ {9, 0.2890648263178878, 0x8c8b6d2b, 0xd24cde04},
  /* 12 */ {8, 0.2789429456511298, 0x19a10000, 0x3fa39ab5},
  /* 13 */ {8, 0.2702381544273197, 0x309f1021, 0x50f8ac5f},
  /* 14 */ {8, 0.2626495350371935, 0x57f6c100, 0x74843b1e},
  /* 15 */ {8, 0.2559580248098155, 0x98c29b81, 0xad0326c2},
  /* 16 */ {8, 0.2500000000000000, 0x4, 0x0},
  /* 17 */ {7, 0.2446505421182260, 0x18754571, 0x4ef0b6bd},
  /* 18 */ {7, 0.2398124665681314, 0x247dbc80, 0xc0fc48a1},
  /* 19 */ {7, 0.2354089133666382, 0x3547667b, 0x33838942},
  /* 20 */ {7, 0.2313782131597592, 0x4c4b4000, 0xad7f29ab},
  /* 21 */ {7, 0.2276702486969530, 0x6b5a6e1d, 0x313c3d15},
  /* 22 */ {7, 0.2242438242175754, 0x94ace180, 0xb8cca9e0},
  /* 23 */ {7, 0.2210647294575037, 0xcaf18367, 0x42ed6de9},
  /* 24 */ {6, 0.2181042919855316, 0xb640000, 0x67980e0b},
  /* 25 */ {6, 0.2153382790366965, 0xe8d4a51, 0x19799812},
  /* 26 */ {6, 0.2127460535533632, 0x1269ae40, 0xbce85396},
  /* 27 */ {6, 0.2103099178571525, 0x17179149, 0x62c103a9},
  /* 28 */ {6, 0.2080145976765095, 0x1cb91000, 0x1d353d43},
  /* 29 */ {6, 0.2058468324604344, 0x23744899, 0xce1decea},
  /* 30 */ {6, 0.2037950470905062, 0x2b73a840, 0x790fc511},
  /* 31 */ {6, 0.2018490865820999, 0x34e63b41, 0x35b865a0},
  /* 32 */ {6, 0.2000000000000000, 0x5, 0x0},
  /* 33 */ {6, 0.1982398631705605, 0x4cfa3cc1, 0xa9aed1b3},
  /* 34 */ {6, 0.1965616322328226, 0x5c13d840, 0x63dfc229},
  /* 35 */ {6, 0.1949590218937863, 0x6d91b519, 0x2b0fee30},
  /* 36 */ {6, 0.1934264036172708, 0x81bf1000, 0xf91bd1b6},
  /* 37 */ {6, 0.1919587200065601, 0x98ede0c9, 0xac89c3a9},
  /* 38 */ {6, 0.1905514124267734, 0xb3773e40, 0x6d2c32fe},
  /* 39 */ {6, 0.1892003595168700, 0xd1bbc4d1, 0x387907c9},
  /* 40 */ {6, 0.1879018247091076, 0xf4240000, 0xc6f7a0b},
  /* 41 */ {5, 0.1866524112389434, 0x6e7d349, 0x28928154},
  /* 42 */ {5, 0.1854490234153689, 0x7ca30a0, 0x6e8629d},
  /* 43 */ {5, 0.1842888331487062, 0x8c32bbb, 0xd373dca0},
  /* 44 */ {5, 0.1831692509136336, 0x9d46c00, 0xa0b17895},
  /* 45 */ {5, 0.1820879004699383, 0xaffacfd, 0x746811a5},
  /* 46 */ {5, 0.1810425967800402, 0xc46bee0, 0x4da6500f},
  /* 47 */ {5, 0.1800313266566926, 0xdab86ef, 0x2ba23582},
  /* 48 */ {5, 0.1790522317510414, 0xf300000, 0xdb20a88},
  /* 49 */ {5, 0.1781035935540111, 0x10d63af1, 0xe68d5ce4},
  /* 50 */ {5, 0.1771838201355579, 0x12a05f20, 0xb7cdfd9d},
  /* 51 */ {5, 0.1762914343888821, 0x1490aae3, 0x8e583933},
  /* 52 */ {5, 0.1754250635819545, 0x16a97400, 0x697cc3ea},
  /* 53 */ {5, 0.1745834300480449, 0x18ed2825, 0x48a5ca6c},
  /* 54 */ {5, 0.1737653428714400, 0x1b5e4d60, 0x2b52db16},
  /* 55 */ {5, 0.1729696904450771, 0x1dff8297, 0x111586a6},
  /* 56 */ {5, 0.1721954337940981, 0x20d38000, 0xf31d2b36},
  /* 57 */ {5, 0.1714416005739134, 0x23dd1799, 0xc8d76d19},
  /* 58 */ {5, 0.1707072796637201, 0x271f35a0, 0xa2cb1eb4},
  /* 59 */ {5, 0.1699916162869140, 0x2a9ce10b, 0x807c3ec3},
  /* 60 */ {5, 0.1692938075987814, 0x2e593c00, 0x617ec8bf},
  /* 61 */ {5, 0.1686130986895011, 0x3257844d, 0x45746cbe},
  /* 62 */ {5, 0.1679487789570419, 0x369b13e0, 0x2c0aa273},
  /* 63 */ {5, 0.1673001788101741, 0x3b27613f, 0x14f90805},
  /* 64 */ {5, 0.1666666666666667, 0x6, 0x0},
  /* 65 */ {5, 0.1660476462159378, 0x4528a141, 0xd9cf0829},
  /* 66 */ {5, 0.1654425539190583, 0x4aa51420, 0xb6fc4841},
  /* 67 */ {5, 0.1648508567221603, 0x50794633, 0x973054cb},
  /* 68 */ {5, 0.1642720499620502, 0x56a94400, 0x7a1dbe4b},
  /* 69 */ {5, 0.1637056554452156, 0x5d393975, 0x5f7fcd7f},
  /* 70 */ {5, 0.1631512196835108, 0x642d7260, 0x47196c84},
  /* 71 */ {5, 0.1626083122716342, 0x6b8a5ae7, 0x30b43635},
  /* 72 */ {5, 0.1620765243931223, 0x73548000, 0x1c1fa5f6},
  /* 73 */ {5, 0.1615554674429964, 0x7b908fe9, 0x930634a},
  /* 74 */ {5, 0.1610447717564444, 0x84435aa0, 0xef7f4a3c},
  /* 75 */ {5, 0.1605440854340214, 0x8d71d25b, 0xcf5552d2},
  /* 76 */ {5, 0.1600530732548213, 0x97210c00, 0xb1a47c8e},
  /* 77 */ {5, 0.1595714156699382, 0xa1563f9d, 0x9634b43e},
  /* 78 */ {5, 0.1590988078692941, 0xac16c8e0, 0x7cd3817d},
  /* 79 */ {5, 0.1586349589155960, 0xb768278f, 0x65536761},
  /* 80 */ {5, 0.1581795909397823, 0xc3500000, 0x4f8b588e},
  /* 81 */ {5, 0.1577324383928644, 0xcfd41b91, 0x3b563c24},
  /* 82 */ {5, 0.1572932473495469, 0xdcfa6920, 0x28928154},
  /* 83 */ {5, 0.1568617748594410, 0xeac8fd83, 0x1721bfb0},
  /* 84 */ {5, 0.1564377883420715, 0xf9461400, 0x6e8629d},
  /* 85 */ {4, 0.1560210650222250, 0x31c84b1, 0x491cc17c},
  /* 86 */ {4, 0.1556113914024939, 0x342ab10, 0x3a11d83b},
  /* 87 */ {4, 0.1552085627701551, 0x36a2c21, 0x2be074cd},
  /* 88 */ {4, 0.1548123827357682, 0x3931000, 0x1e7a02e7},
  /* 89 */ {4, 0.1544226628011101, 0x3bd5ee1, 0x11d10edd},
  /* 90 */ {4, 0.1540392219542636, 0x3e92110, 0x5d92c68},
  /* 91 */ {4, 0.1536618862898642, 0x4165ef1, 0xf50dbfb2},
  /* 92 */ {4, 0.1532904886526781, 0x4452100, 0xdf9f1316},
  /* 93 */ {4, 0.1529248683028321, 0x4756fd1, 0xcb52a684},
  /* 94 */ {4, 0.1525648706011593, 0x4a75410, 0xb8163e97},
  /* 95 */ {4, 0.1522103467132434, 0x4dad681, 0xa5d8f269},
  /* 96 */ {4, 0.1518611533308632, 0x5100000, 0x948b0fcd},
  /* 97 */ {4, 0.1515171524096389, 0x546d981, 0x841e0215},
  /* 98 */ {4, 0.1511782109217764, 0x57f6c10, 0x74843b1e},
  /* 99 */ {4, 0.1508442006228941, 0x5b9c0d1, 0x65b11e6e},
  /* 100 */ {4, 0.1505149978319906, 0x5f5e100, 0x5798ee23},
  /* 101 */ {4, 0.1501904832236880, 0x633d5f1, 0x4a30b99b},
  /* 102 */ {4, 0.1498705416319474, 0x673a910, 0x3d6e4d94},
  /* 103 */ {4, 0.1495550618645152, 0x6b563e1, 0x314825b0},
  /* 104 */ {4, 0.1492439365274121, 0x6f91000, 0x25b55f2e},
  /* 105 */ {4, 0.1489370618588283, 0x73eb721, 0x1aadaccb},
  /* 106 */ {4, 0.1486343375718350, 0x7866310, 0x10294ba2},
  /* 107 */ {4, 0.1483356667053617, 0x7d01db1, 0x620f8f6},
  /* 108 */ {4, 0.1480409554829326, 0x81bf100, 0xf91bd1b6},
  /* 109 */ {4, 0.1477501131786861, 0x869e711, 0xe6d37b2a},
  /* 110 */ {4, 0.1474630519902391, 0x8ba0a10, 0xd55cff6e},
  /* 111 */ {4, 0.1471796869179852, 0x90c6441, 0xc4ad2db2},
  /* 112 */ {4, 0.1468999356504447, 0x9610000, 0xb4b985cf},
  /* 113 */ {4, 0.1466237184553111, 0x9b7e7c1, 0xa5782bef},
  /* 114 */ {4, 0.1463509580758620, 0xa112610, 0x96dfdd2a},
  /* 115 */ {4, 0.1460815796324244, 0xa6cc591, 0x88e7e509},
  /* 116 */ {4, 0.1458155105286054, 0xacad100, 0x7b8813d3},
  /* 117 */ {4, 0.1455526803620167, 0xb2b5331, 0x6eb8b595},
  /* 118 */ {4, 0.1452930208392429, 0xb8e5710, 0x627289db},
  /* 119 */ {4, 0.1450364656948130, 0xbf3e7a1, 0x56aebc07},
  /* 120 */ {4, 0.1447829506139581, 0xc5c1000, 0x4b66dc33},
  /* 121 */ {4, 0.1445324131589439, 0xcc6db61, 0x4094d8a3},
  /* 122 */ {4, 0.1442847926987864, 0xd345510, 0x3632f7a5},
  /* 123 */ {4, 0.1440400303421672, 0xda48871, 0x2c3bd1f0},
  /* 124 */ {4, 0.1437980688733776, 0xe178100, 0x22aa4d5f},
  /* 125 */ {4, 0.1435588526911310, 0xe8d4a51, 0x19799812},
  /* 126 */ {4, 0.1433223277500932, 0xf05f010, 0x10a523e5},
  /* 127 */ {4, 0.1430884415049874, 0xf817e01, 0x828a237},
  /* 128 */ {4, 0.1428571428571428, 0x7, 0x0},
  /* 129 */ {4, 0.1426283821033600, 0x10818201, 0xf04ec452},
  /* 130 */ {4, 0.1424021108869747, 0x11061010, 0xe136444a},
  /* 131 */ {4, 0.1421782821510107, 0x118db651, 0xd2af9589},
  /* 132 */ {4, 0.1419568500933153, 0x12188100, 0xc4b42a83},
  /* 133 */ {4, 0.1417377701235801, 0x12a67c71, 0xb73dccf5},
  /* 134 */ {4, 0.1415209988221527, 0x1337b510, 0xaa4698c5},
  /* 135 */ {4, 0.1413064939005528, 0x13cc3761, 0x9dc8f729},
  /* 136 */ {4, 0.1410942141636095, 0x14641000, 0x91bf9a30},
  /* 137 */ {4, 0.1408841194731412, 0x14ff4ba1, 0x86257887},
  /* 138 */ {4, 0.1406761707131039, 0x159df710, 0x7af5c98c},
  /* 139 */ {4, 0.1404703297561400, 0x16401f31, 0x702c01a0},
  /* 140 */ {4, 0.1402665594314587, 0x16e5d100, 0x65c3ceb1},
  /* 141 */ {4, 0.1400648234939879, 0x178f1991, 0x5bb91502},
  /* 142 */ {4, 0.1398650865947379, 0x183c0610, 0x5207ec23},
  /* 143 */ {4, 0.1396673142523192, 0x18eca3c1, 0x48ac9c19},
  /* 144 */ {4, 0.1394714728255649, 0x19a10000, 0x3fa39ab5},
  /* 145 */ {4, 0.1392775294872041, 0x1a592841, 0x36e98912},
  /* 146 */ {4, 0.1390854521985406, 0x1b152a10, 0x2e7b3140},
  /* 147 */ {4, 0.1388952096850913, 0x1bd51311, 0x2655840b},
  /* 148 */ {4, 0.1387067714131417, 0x1c98f100, 0x1e7596ea},
  /* 149 */ {4, 0.1385201075671774, 0x1d60d1b1, 0x16d8a20d},
  /* 150 */ {4, 0.1383351890281539, 0x1e2cc310, 0xf7bfe87},
  /* 151 */ {4, 0.1381519873525671, 0x1efcd321, 0x85d2492},
  /* 152 */ {4, 0.1379704747522905, 0x1fd11000, 0x179a9f4},
  /* 153 */ {4, 0.1377906240751463, 0x20a987e1, 0xf59e80eb},
  /* 154 */ {4, 0.1376124087861776, 0x21864910, 0xe8b768db},
  /* 155 */ {4, 0.1374358029495937, 0x226761f1, 0xdc39d6d5},
  /* 156 */ {4, 0.1372607812113589, 0x234ce100, 0xd021c5d1},
  /* 157 */ {4, 0.1370873187823978, 0x2436d4d1, 0xc46b5e37},
  /* 158 */ {4, 0.1369153914223921, 0x25254c10, 0xb912f39c},
  /* 159 */ {4, 0.1367449754241439, 0x26185581, 0xae150294},
  /* 160 */ {4, 0.1365760475984821, 0x27100000, 0xa36e2eb1},
  /* 161 */ {4, 0.1364085852596902, 0x280c5a81, 0x991b4094},
  /* 162 */ {4, 0.1362425662114337, 0x290d7410, 0x8f19241e},
  /* 163 */ {4, 0.1360779687331669, 0x2a135bd1, 0x8564e6b7},
  /* 164 */ {4, 0.1359147715670014, 0x2b1e2100, 0x7bfbb5b4},
  /* 165 */ {4, 0.1357529539050150, 0x2c2dd2f1, 0x72dadcc8},
  /* 166 */ {4, 0.1355924953769864, 0x2d428110, 0x69ffc498},
  /* 167 */ {4, 0.1354333760385373, 0x2e5c3ae1, 0x6167f154},
  /* 168 */ {4, 0.1352755763596663, 0x2f7b1000, 0x5911016e},
  /* 169 */ {4, 0.1351190772136599, 0x309f1021, 0x50f8ac5f},
  /* 170 */ {4, 0.1349638598663645, 0x31c84b10, 0x491cc17c},
  /* 171 */ {4, 0.1348099059658080, 0x32f6d0b1, 0x417b26d8},
  /* 172 */ {4, 0.1346571975321549, 0x342ab100, 0x3a11d83b},
  /* 173 */ {4, 0.1345057169479844, 0x3563fc11, 0x32dee622},
  /* 174 */ {4, 0.1343554469488779, 0x36a2c210, 0x2be074cd},
  /* 175 */ {4, 0.1342063706143054, 0x37e71341, 0x2514bb58},
  /* 176 */ {4, 0.1340584713587979, 0x39310000, 0x1e7a02e7},
  /* 177 */ {4, 0.1339117329233981, 0x3a8098c1, 0x180ea5d0},
  /* 178 */ {4, 0.1337661393673756, 0x3bd5ee10, 0x11d10edd},
  /* 179 */ {4, 0.1336216750601996, 0x3d311091, 0xbbfb88e},
  /* 180 */ {4, 0.1334783246737591, 0x3e921100, 0x5d92c68},
  /* 181 */ {4, 0.1333360731748201, 0x3ff90031, 0x1c024c},
  /* 182 */ {4, 0.1331949058177136, 0x4165ef10, 0xf50dbfb2},
  /* 183 */ {4, 0.1330548081372441, 0x42d8eea1, 0xea30efa3},
  /* 184 */ {4, 0.1329157659418126, 0x44521000, 0xdf9f1316},
  /* 185 */ {4, 0.1327777653067443, 0x45d16461, 0xd555c0c9},
  /* 186 */ {4, 0.1326407925678156, 0x4756fd10, 0xcb52a684},
  /* 187 */ {4, 0.1325048343149731, 0x48e2eb71, 0xc193881f},
  /* 188 */ {4, 0.1323698773862368, 0x4a754100, 0xb8163e97},
  /* 189 */ {4, 0.1322359088617821, 0x4c0e0f51, 0xaed8b724},
  /* 190 */ {4, 0.1321029160581950, 0x4dad6810, 0xa5d8f269},
  /* 191 */ {4, 0.1319708865228925, 0x4f535d01, 0x9d15039d},
  /* 192 */ {4, 0.1318398080287045, 0x51000000, 0x948b0fcd},
  /* 193 */ {4, 0.1317096685686114, 0x52b36301, 0x8c394d1d},
  /* 194 */ {4, 0.1315804563506306, 0x546d9810, 0x841e0215},
  /* 195 */ {4, 0.1314521597928493, 0x562eb151, 0x7c3784f8},
  /* 196 */ {4, 0.1313247675185968, 0x57f6c100, 0x74843b1e},
  /* 197 */ {4, 0.1311982683517524, 0x59c5d971, 0x6d02985d},
  /* 198 */ {4, 0.1310726513121843, 0x5b9c0d10, 0x65b11e6e},
  /* 199 */ {4, 0.1309479056113158, 0x5d796e61, 0x5e8e5c64},
  /* 200 */ {4, 0.1308240206478128, 0x5f5e1000, 0x5798ee23},
  /* 201 */ {4, 0.1307009860033912, 0x614a04a1, 0x50cf7bde},
  /* 202 */ {4, 0.1305787914387386, 0x633d5f10, 0x4a30b99b},
  /* 203 */ {4, 0.1304574268895465, 0x65383231, 0x43bb66bd},
  /* 204 */ {4, 0.1303368824626505, 0x673a9100, 0x3d6e4d94},
  /* 205 */ {4, 0.1302171484322746, 0x69448e91, 0x374842ee},
  /* 206 */ {4, 0.1300982152363760, 0x6b563e10, 0x314825b0},
  /* 207 */ {4, 0.1299800734730872, 0x6d6fb2c1, 0x2b6cde75},
  /* 208 */ {4, 0.1298627138972530, 0x6f910000, 0x25b55f2e},
  /* 209 */ {4, 0.1297461274170591, 0x71ba3941, 0x2020a2c5},
  /* 210 */ {4, 0.1296303050907487, 0x73eb7210, 0x1aadaccb},
  /* 211 */ {4, 0.1295152381234257, 0x7624be11, 0x155b891f},
  /* 212 */ {4, 0.1294009178639407, 0x78663100, 0x10294ba2},
  /* 213 */ {4, 0.1292873358018581, 0x7aafdeb1, 0xb160fe9},
  /* 214 */ {4, 0.1291744835645007, 0x7d01db10, 0x620f8f6},
  /* 215 */ {4, 0.1290623529140715, 0x7f5c3a21, 0x14930ef},
  /* 216 */ {4, 0.1289509357448472, 0x81bf1000, 0xf91bd1b6},
  /* 217 */ {4, 0.1288402240804449, 0x842a70e1, 0xefdcb0c7},
  /* 218 */ {4, 0.1287302100711566, 0x869e7110, 0xe6d37b2a},
  /* 219 */ {4, 0.1286208859913518, 0x891b24f1, 0xddfeb94a},
  /* 220 */ {4, 0.1285122442369443, 0x8ba0a100, 0xd55cff6e},
  /* 221 */ {4, 0.1284042773229231, 0x8e2ef9d1, 0xcceced50},
  /* 222 */ {4, 0.1282969778809442, 0x90c64410, 0xc4ad2db2},
  /* 223 */ {4, 0.1281903386569819, 0x93669481, 0xbc9c75f9},
  /* 224 */ {4, 0.1280843525090381, 0x96100000, 0xb4b985cf},
  /* 225 */ {4, 0.1279790124049077, 0x98c29b81, 0xad0326c2},
  /* 226 */ {4, 0.1278743114199984, 0x9b7e7c10, 0xa5782bef},
  /* 227 */ {4, 0.1277702427352035, 0x9e43b6d1, 0x9e1771a9},
  /* 228 */ {4, 0.1276667996348261, 0xa1126100, 0x96dfdd2a},
  /* 229 */ {4, 0.1275639755045533, 0xa3ea8ff1, 0x8fd05c41},
  /* 230 */ {4, 0.1274617638294791, 0xa6cc5910, 0x88e7e509},
  /* 231 */ {4, 0.1273601581921740, 0xa9b7d1e1, 0x8225759d},
  /* 232 */ {4, 0.1272591522708010, 0xacad1000, 0x7b8813d3},
  /* 233 */ {4, 0.1271587398372755, 0xafac2921, 0x750eccf9},
  /* 234 */ {4, 0.1270589147554692, 0xb2b53310, 0x6eb8b595},
  /* 235 */ {4, 0.1269596709794558, 0xb5c843b1, 0x6884e923},
  /* 236 */ {4, 0.1268610025517973, 0xb8e57100, 0x627289db},
  /* 237 */ {4, 0.1267629036018709, 0xbc0cd111, 0x5c80c07b},
  /* 238 */ {4, 0.1266653683442337, 0xbf3e7a10, 0x56aebc07},
  /* 239 */ {4, 0.1265683910770258, 0xc27a8241, 0x50fbb19b},
  /* 240 */ {4, 0.1264719661804097, 0xc5c10000, 0x4b66dc33},
  /* 241 */ {4, 0.1263760881150453, 0xc91209c1, 0x45ef7c7c},
  /* 242 */ {4, 0.1262807514205999, 0xcc6db610, 0x4094d8a3},
  /* 243 */ {4, 0.1261859507142915, 0xcfd41b91, 0x3b563c24},
  /* 244 */ {4, 0.1260916806894653, 0xd3455100, 0x3632f7a5},
  /* 245 */ {4, 0.1259979361142023, 0xd6c16d31, 0x312a60c3},
  /* 246 */ {4, 0.1259047118299582, 0xda488710, 0x2c3bd1f0},
  /* 247 */ {4, 0.1258120027502338, 0xdddab5a1, 0x2766aa45},
  /* 248 */ {4, 0.1257198038592741, 0xe1781000, 0x22aa4d5f},
  /* 249 */ {4, 0.1256281102107963, 0xe520ad61, 0x1e06233c},
  /* 250 */ {4, 0.1255369169267456, 0xe8d4a510, 0x19799812},
  /* 251 */ {4, 0.1254462191960791, 0xec940e71, 0x15041c33},
  /* 252 */ {4, 0.1253560122735751, 0xf05f0100, 0x10a523e5},
  /* 253 */ {4, 0.1252662914786691, 0xf4359451, 0xc5c2749},
  /* 254 */ {4, 0.1251770521943144, 0xf817e010, 0x828a237},
  /* 255 */ {4, 0.1250882898658681, 0xfc05fc01, 0x40a1423},
  /* 256 */ {4, 0.1250000000000000, 0x8, 0x0},
};

static size_t
mp32sizeinbase(uint32 xsize, uint32 * xdata, uint32 base)
{
    uint32 nbits;
    size_t res;

    if (xsize == 0)
	return 1;

    /* XXX assumes positive integer. */
    nbits = 32 * xsize - mp32mszcnt(xsize, xdata);
    if ((base & (base-1)) == 0) {	/* exact power of 2 */
	uint32 lbits = mp_bases[base].big_base;
	res = (nbits + (lbits - 1)) / lbits;
    } else {
	res = (nbits * mp_bases[base].chars_per_bit_exactly) + 1;
    }
if (_bc_debug < 0)
fprintf(stderr, "*** mp32sizeinbase(%p[%d], %d) res %u\n", xdata, xsize, base, (unsigned)res);
    return res;
}

/*@-boundswrite@*/
static void my32ndivmod(uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, register uint32* wksp)
{
	/* result must be xsize+1 in length */
	/* wksp must be ysize+1 in length */
	/* expect ydata to be normalized */
	register uint64 temp;
	register uint32 q;
	uint32 msw = *ydata;
	uint32 qsize = xsize-ysize;

if (_bc_debug < 0) {
fprintf(stderr, "*** my32ndivmod(%p[%d], x, y, %p[%d])\n", result, xsize+1, wksp, ysize+1);
fprintf(stderr, "\t     x: %p[%d]\t", xdata, xsize), mp32println(stderr, xsize, xdata);
fprintf(stderr, "\t     y: %p[%d]\t", ydata, ysize), mp32println(stderr, ysize, ydata);
}

	mp32copy(xsize, result+1, xdata);
if (_bc_debug < 0)
fprintf(stderr, "\tres(%d): %p[%d]\t", mp32ge(ysize, result+1, ydata), result+1, xsize), mp32println(stderr, xsize, result+1);
	/*@-compdef@*/ /* LCL: result+1 undefined */
	if (mp32ge(ysize, result+1, ydata))
	{
		/* fprintf(stderr, "subtracting\n"); */
		(void) mp32sub(ysize, result+1, ydata);
		*(result++) = 1;
	}
	else
		*(result++) = 0;
	/*@=compdef@*/

if (_bc_debug < 0)
fprintf(stderr, "\tresult: %p[%d]\t", result-1, xsize+1), mp32println(stderr, xsize+1, result-1);

	/*@-usedef@*/	/* LCL: result[0] is set */
	while (qsize--)
	{
		/* fprintf(stderr, "result = "); mp32println(stderr, xsize+1, result); */
		/* get the two high words of r into temp */
		temp = result[0];
		temp <<= 32;
		temp += result[1];
		/* fprintf(stderr, "q = %016llx / %08lx\n", temp, msw); */
		temp /= msw;
		q = (uint32) temp;

		/* fprintf(stderr, "q = %08x\n", q); */

		/*@-evalorder@*/
		*wksp = mp32setmul(ysize, wksp+1, ydata, q);
		/*@=evalorder@*/

		/* fprintf(stderr, "mp32lt "); mp32print(ysize+1, result); fprintf(stderr, " < "); mp32println(stderr, ysize+1, wksp); */
		while (mp32lt(ysize+1, result, wksp))
		{
			/* fprintf(stderr, "mp32lt! "); mp32print(ysize+1, result); fprintf(stderr, " < "); mp32println(stderr, ysize+1, wksp); */
			/* fprintf(stderr, "decreasing q\n"); */
			(void) mp32subx(ysize+1, wksp, ysize, ydata);
			q--;
		}
		/* fprintf(stderr, "subtracting\n"); */
		(void) mp32sub(ysize+1, result, wksp);
		*(result++) = q;
	}
	/*@=usedef@*/
}
/*@=boundswrite@*/

static char *
mp32str(char * t, uint32 nt, uint32 zsize, uint32 * zdata, uint32 zbase)
{
    uint32 size = zsize + 1;
    uint32 * wksp = alloca((size+1) * sizeof(*wksp));
    uint32 * adata = alloca(size * sizeof(*adata));
    uint32 * bdata = alloca(size * sizeof(*bdata));
    static char bchars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    uint32 result;

if (_bc_debug < 0)
fprintf(stderr, "*** mp32str(%p[%d], %p[%d], %d):\t", t, nt, zdata, zsize, zbase), mp32println(stderr, zsize, zdata);

    mp32setx(size, bdata, zsize, zdata);

    t[nt] = '\0';
    while (nt--) {
	mp32setx(size, adata, size, bdata);
if (_bc_debug < 0)
fprintf(stderr, "***       a: %p[%d]\t", adata, size), mp32println(stderr, size, adata);
	mp32nmod(bdata, size, adata, 1, &zbase, wksp);
if (_bc_debug < 0)
fprintf(stderr, "***    nmod: %p[%d]\t", bdata, size), mp32println(stderr, size, bdata);
	result = bdata[size-1];
	t[nt] = bchars[result];
	mp32ndivmod(bdata, size, adata, 1, &zbase, wksp);
if (_bc_debug < 0)
fprintf(stderr, "*** ndivmod: %p[%d]\t", bdata, size), mp32println(stderr, size, bdata);
	if (mp32z(size, bdata))
	    break;
    }
    /* XXX Fill leading zeroes (if any). */
    while (nt--)
	t[nt] = '0';
    return t;
}

static PyObject *
rpmbc_format(rpmbcObject * z, uint32 zbase, int withname)
{
    PyStringObject * so;
    uint32 i;
    uint32 nt;
    uint32 zsize;
    uint32 * zdata;
    char * t, * te;
    char prefix[5];
    char * tcp = prefix;
    uint32 zsign;

    if (z == NULL || !is_rpmbc(z)) {
	PyErr_BadInternalCall();
	return NULL;
    }

if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_format(%p,%d,%d):\t", z, zbase, withname), mp32println(stderr, z->n.size, z->n.data);

    assert(zbase >= 2 && zbase <= 36);

    if (withname)
	i = strlen(initialiser_name) + 2; /* e.g. 'rpmbc(' + ')' */
    else
	i = 0;

    zsign = mp32msbset(z->n.size, z->n.data);
    nt = mp32bitcnt(z->n.size, z->n.data);
    if (nt == 0) {
	zbase = 10;	/* '0' in every base, right */
	nt = 1;
	zsize = 1;
	zdata = &zsign;
    } else if (zsign) {
	*tcp++ = '-';
	i += 1;		/* space to hold '-' */
	zsize = (nt + 31)/32;
	zdata = alloca(zsize * sizeof(*zdata));
	mp32setx(zsize, zdata, zsize, z->n.data + (z->n.size - zsize));
	mp32neg(zsize, zdata);
    } else {
	zsize = (nt + 31)/32;
	zdata = z->n.data + (z->n.size - zsize);
    }
    
    nt = mp32sizeinbase(zsize, zdata, zbase);
    i += nt;

    if (zbase == 16) {
	*tcp++ = '0';
	*tcp++ = 'x';
	i += 2;		/* space to hold '0x' */
    } else if (zbase == 8) {
	*tcp++ = '0';
	i += 1;		/* space to hold the extra '0' */
    } else if (zbase > 10) {
	*tcp++ = '0' + zbase / 10;
	*tcp++ = '0' + zbase % 10;
	*tcp++ = '#';
	i += 3;		/* space to hold e.g. '12#' */
    } else if (zbase < 10) {
	*tcp++ = '0' + zbase;
	*tcp++ = '#';
	i += 2;		/* space to hold e.g. '6#' */
    }

    so = (PyStringObject *)PyString_FromStringAndSize((char *)0, i);
    if (so == NULL)
	return NULL;

    /* get the beginning of the string memory and start copying things */
    te = PyString_AS_STRING(so);
    if (withname) {
	te = stpcpy(te, initialiser_name);
	*te++ = '('; /*')'*/
    }

    /* copy the already prepared prefix; e.g. sign and base indicator */
    *tcp = '\0';
    t = te = stpcpy(te, prefix);

    (void) mp32str(te, nt, zsize, zdata, zbase);

    /* Nuke leading zeroes. */
    nt = 0;
    while (t[nt] == '0')
	nt++;
    if (t[nt] == '\0')	/* all zeroes special case. */
	nt--;
    if (nt > 0)
    do {
	*t = t[nt];
    } while (*t++ != '\0');

    te += strlen(te);

    if (withname)
	*te++ = /*'('*/ ')';
    *te = '\0';

    assert(te - PyString_AS_STRING(so) <= i);

    if (te - PyString_AS_STRING(so) != i)
	so->ob_size -= i - (te - PyString_AS_STRING(so));

    return (PyObject *)so;
}

/**
 *  Precomputes the sliding window table for computing powers of x.
 *
 * Sliding Window Exponentiation, Algorithm 14.85 in "Handbook of Applied Cryptography".
 *
 * First of all, the table with the powers of g can be reduced by
 * about half; the even powers don't need to be accessed or stored.
 *
 * Get up to K bits starting with a one, if we have that many still available.
 *
 * Do the number of squarings of A in the first column, then multiply by
 * the value in column two, and finally do the number of squarings in
 * column three.
 *
 * This table can be used for K=2,3,4 and can be extended.
 *  
 *
\verbatim
	   0 : - | -       | -
	   1 : 1 |  g1 @ 0 | 0
	  10 : 1 |  g1 @ 0 | 1
	  11 : 2 |  g3 @ 1 | 0
	 100 : 1 |  g1 @ 0 | 2
	 101 : 3 |  g5 @ 2 | 0
	 110 : 2 |  g3 @ 1 | 1
	 111 : 3 |  g7 @ 3 | 0
	1000 : 1 |  g1 @ 0 | 3
	1001 : 4 |  g9 @ 4 | 0
	1010 : 3 |  g5 @ 2 | 1
	1011 : 4 | g11 @ 5 | 0
	1100 : 2 |  g3 @ 1 | 2
	1101 : 4 | g13 @ 6 | 0
	1110 : 3 |  g7 @ 3 | 1
	1111 : 4 | g15 @ 7 | 0
\endverbatim
 *
 */
static void mpnslide(const mpnumber* n, uint32 xsize, const uint32* xdata,
		uint32 size, /*@out@*/ uint32* slide)
	/*@modifies slide @*/
{
    uint32 rsize = (xsize > size ? xsize : size);
    uint32 * result = alloca(2 * rsize * sizeof(*result));

    mp32sqr(result, xsize, xdata);			/* x^2 temp */
    mp32setx(size, slide, xsize+xsize, result);
if (_bc_debug < 0)
fprintf(stderr, "\t  x^2:\t"), mp32println(stderr, size, slide);
    mp32mul(result,   xsize, xdata, size, slide);	/* x^3 */
    mp32setx(size, slide+size, xsize+size, result);
if (_bc_debug < 0)
fprintf(stderr, "\t  x^3:\t"), mp32println(stderr, size, slide+size);
    mp32mul(result,  size, slide, size, slide+size);	/* x^5 */
    mp32setx(size, slide+2*size, size+size, result);
if (_bc_debug < 0)
fprintf(stderr, "\t  x^5:\t"), mp32println(stderr, size, slide+2*size);
    mp32mul(result,  size, slide, size, slide+2*size);	/* x^7 */
    mp32setx(size, slide+3*size, size+size, result);
if (_bc_debug < 0)
fprintf(stderr, "\t  x^7:\t"), mp32println(stderr, size, slide+3*size);
    mp32mul(result,  size, slide, size, slide+3*size);	/* x^9 */
    mp32setx(size, slide+4*size, size+size, result);
if (_bc_debug < 0)
fprintf(stderr, "\t  x^9:\t"), mp32println(stderr, size, slide+4*size);
    mp32mul(result,  size, slide, size, slide+4*size);	/* x^11 */
    mp32setx(size, slide+5*size, size+size, result);
if (_bc_debug < 0)
fprintf(stderr, "\t x^11:\t"), mp32println(stderr, size, slide+5*size);
    mp32mul(result,  size, slide, size, slide+5*size);	/* x^13 */
    mp32setx(size, slide+6*size, size+size, result);
if (_bc_debug < 0)
fprintf(stderr, "\t x^13:\t"), mp32println(stderr, size, slide+6*size);
    mp32mul(result,  size, slide, size, slide+6*size);	/* x^15 */
    mp32setx(size, slide+7*size, size+size, result);
if (_bc_debug < 0)
fprintf(stderr, "\t x^15:\t"), mp32println(stderr, size, slide+7*size);
    mp32setx(size, slide, xsize, xdata);		/* x^1 */
if (_bc_debug < 0)
fprintf(stderr, "\t  x^1:\t"), mp32println(stderr, size, slide);
}

/*@observer@*/ /*@unchecked@*/
static byte mpnslide_presq[16] = 
{ 0, 1, 1, 2, 1, 3, 2, 3, 1, 4, 3, 4, 2, 4, 3, 4 };

/*@observer@*/ /*@unchecked@*/
static byte mpnslide_mulg[16] =
{ 0, 0, 0, 1, 0, 2, 1, 3, 0, 4, 2, 5, 1, 6, 3, 7 };

/*@observer@*/ /*@unchecked@*/
static byte mpnslide_postsq[16] =
{ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };

/**
 * Exponentiation with precomputed sliding window table.
 */
/*@-boundsread@*/
static void mpnpowsld_w(mpnumber* n, uint32 size, const uint32* slide,
		uint32 psize, const uint32* pdata)
{
    uint32 rsize = (n->size > size ? n->size : size);
    uint32 * rdata = alloca(2 * rsize * sizeof(*rdata));
    uint32 lbits = 0;
    uint32 kbits = 0;
    uint32 s;
    uint32 temp;
    uint32 count;

if (_bc_debug < 0)
fprintf(stderr, "npowsld: p\t"), mp32println(stderr, psize, pdata);
    /* 2. A = 1, i = t. */
    mp32zero(n->size, n->data);
    n->data[n->size-1] = 1;

    /* Find first bit set in exponent. */
    temp = *pdata;
    count = 8 * sizeof(temp);
    while (count != 0) {
	if (temp & 0x80000000)
	    break;
	temp <<= 1;
	count--;
    }

    while (psize) {
	while (count != 0) {

	    /* Shift next bit of exponent into sliding window. */
	    kbits <<= 1;
	    if (temp & 0x80000000)
		kbits++;

	    /* On 1st non-zero in window, try to collect K bits. */
	    if (kbits != 0) {
		if (lbits != 0)
		    lbits++;
		else if (temp & 0x80000000)
		    lbits = 1;
		else
		    {};

		/* If window is full, then compute and clear window. */
		if (lbits == 4) {
if (_bc_debug < 0)
fprintf(stderr, "*** #1 lbits %d kbits %d\n", lbits, kbits);
		    for (s = mpnslide_presq[kbits]; s > 0; s--) {
			mp32sqr(rdata, n->size, n->data);
			mp32setx(n->size, n->data, 2*n->size, rdata);
if (_bc_debug < 0)
fprintf(stderr, "\t pre1:\t"), mp32println(stderr, n->size, n->data);
		    }

		    mp32mul(rdata, n->size, n->data,
				size, slide+mpnslide_mulg[kbits]*size);
		    mp32setx(n->size, n->data, n->size+size, rdata);
if (_bc_debug < 0)
fprintf(stderr, "\t mul1:\t"), mp32println(stderr, n->size, n->data);

		    for (s = mpnslide_postsq[kbits]; s > 0; s--) {
			mp32sqr(rdata, n->size, n->data);
			mp32setx(n->size, n->data, 2*n->size, rdata);
if (_bc_debug < 0)
fprintf(stderr, "\tpost1:\t"), mp32println(stderr, n->size, n->data);
		    }

		    lbits = kbits = 0;
		}
	    } else {
		mp32sqr(rdata, n->size, n->data);
		mp32setx(n->size, n->data, 2*n->size, rdata);
if (_bc_debug < 0)
fprintf(stderr, "\t  sqr:\t"), mp32println(stderr, n->size, n->data);
	    }

	    temp <<= 1;
	    count--;
	}
	if (--psize) {
	    count = 8 * sizeof(temp);
	    temp = *(pdata++);
	}
    }

    if (kbits != 0) {
if (_bc_debug < 0)
fprintf(stderr, "*** #1 lbits %d kbits %d\n", lbits, kbits);
	for (s = mpnslide_presq[kbits]; s > 0; s--) {
	    mp32sqr(rdata, n->size, n->data);
	    mp32setx(n->size, n->data, 2*n->size, rdata);
if (_bc_debug < 0)
fprintf(stderr, "\t pre2:\t"), mp32println(stderr, n->size, n->data);
	}

	mp32mul(rdata, n->size, n->data,
			size, slide+mpnslide_mulg[kbits]*size);
	mp32setx(n->size, n->data, n->size+size, rdata);
if (_bc_debug < 0)
fprintf(stderr, "\t mul2:\t"), mp32println(stderr, n->size, n->data);

	for (s = mpnslide_postsq[kbits]; s > 0; s--) {
	    mp32sqr(rdata, n->size, n->data);
	    mp32setx(n->size, n->data, 2*n->size, rdata);
if (_bc_debug < 0)
fprintf(stderr, "\tpost2:\t"), mp32println(stderr, n->size, n->data);
	}
    }
}
/*@=boundsread@*/

/**
 * mpnpow_w
 *
 * Uses sliding window exponentiation; needs extra storage:
 *	if K=3, needs 4*size, if K=4, needs 8*size
 */
/*@-boundsread@*/
static void mpnpow_w(mpnumber* n, uint32 xsize, const uint32* xdata,
		uint32 psize, const uint32* pdata)
{
    uint32 xbits = mp32bitcnt(xsize, xdata);
    uint32 pbits = mp32bitcnt(psize, pdata);
    uint32 nbits;
    uint32 *slide;
    uint32 nsize;
    uint32 size;

    /* Special case: 0**P and X**(-P) */
    if (xbits == 0 || mp32msbset(psize, pdata)) {
	mpnsetw(n, 0);
	return;
    }
    /* Special case: X**0 and 1**P */
    if (pbits == 0 || mp32isone(xsize, xdata)) {
	mpnsetw(n, 1);
	return;
    }

    /* Normalize (to uint32 boundary) exponent. */
    pdata += psize - ((pbits+31)/32);
    psize -= (pbits/32);

    /* Calculate size of result. */
    if (xbits == 0) xbits = 1;
    nbits = (*pdata) * xbits;
    nsize = (nbits + 31)/32;

    /* XXX Add 1 word to carry sign bit */
    if (!mp32msbset(xsize, xdata) && (nbits & (32 -1)) == 0)
	nsize++;

    size = ((15 * xbits)+31)/32;

if (_bc_debug < 0)
fprintf(stderr, "*** pbits %d xbits %d nsize %d size %d\n", pbits, xbits, nsize, size);
    mpnsize(n, nsize);

    /* 1. Precompute odd powers of x (up to 2**K). */
    slide = (uint32*) alloca( (8*size) * sizeof(uint32));

    mpnslide(n, xsize, xdata, size, slide);

    /*@-internalglobs -mods@*/ /* noisy */
    mpnpowsld_w(n, size, slide, psize, pdata);
    /*@=internalglobs =mods@*/

}
/*@=boundsread@*/

/* ---------- */

static void
rpmbc_dealloc(rpmbcObject * s)
	/*@modifies s @*/
{
if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_dealloc(%p)\n", s);

    mpnfree(&s->n);
    PyObject_Del(s);
}

#ifdef	DYING
static int
rpmbc_print(rpmbcObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/
{
if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_print(%p)\n", s);
    mp32print(fp, s->n.size, s->n.data);
    return 0;
}
#else
#define	rpmbc_print	0
#endif

static int
rpmbc_compare(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    int ret;

    if (mp32eqx(a->n.size, a->n.data, b->n.size, b->n.data))
	ret = 0;
    else if (mp32gtx(a->n.size, a->n.data, b->n.size, b->n.data))
	ret = 1;
    else
	ret = -1;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_compare(%p[%s],%p[%s]) ret %d\n", a, lbl(a), b, lbl(b), ret);
    return ret;
}

static PyObject *
rpmbc_repr(rpmbcObject * a)
	/*@*/
{
    PyObject * so = rpmbc_format(a, 10, 1);
if (_bc_debug && so != NULL)
fprintf(stderr, "*** rpmbc_repr(%p): \"%s\"\n", a, PyString_AS_STRING(so));
    return so;
}

/** \ingroup py_c  
 */
static PyObject *
rpmbc_str(rpmbcObject * a)
	/*@*/
{
    PyObject * so = rpmbc_format(a, 10, 0);
if (_bc_debug && so != NULL)
fprintf(stderr, "*** rpmbc_str(%p): \"%s\"\n", a, PyString_AS_STRING(so));
    return so;
}

/** \ingroup py_c  
 */
static int rpmbc_init(rpmbcObject * z, PyObject *args, PyObject *kwds)
	/*@modifies z @*/
{
    PyObject * o = NULL;
    uint32 words = 0;
    long l = 0;

    if (!PyArg_ParseTuple(args, "|O:Cvt", &o)) return -1;

    if (o == NULL) {
	if (z->n.data == NULL)
	    mpnsetw(&z->n, 0);
    } else if (PyInt_Check(o)) {
	l = PyInt_AsLong(o);
	words = sizeof(l)/sizeof(words);
    } else if (PyLong_Check(o)) {
	l = PyLong_AsLong(o);
	words = sizeof(l)/sizeof(words);
    } else if (PyFloat_Check(o)) {
	double d = PyFloat_AsDouble(o);
	/* XXX TODO: check for overflow/underflow. */
	l = (long) (d + 0.5);
	words = sizeof(l)/sizeof(words);
    } else if (PyString_Check(o)) {
	const unsigned char * hex = PyString_AsString(o);
	/* XXX TODO: check for hex. */
	mpnsethex(&z->n, hex);
    } else if (is_rpmbc(o)) {
	rpmbcObject *a = (rpmbcObject *)o;
	mpnsize(&z->n, a->n.size);
	if (a->n.size > 0)
	    mp32setx(z->n.size, z->n.data, a->n.size, a->n.data);
    } else {
	PyErr_SetString(PyExc_TypeError, "non-numeric coercion failed (rpmbc_init)");
	return -1;
    }

    if (words > 0) {
	mpnsize(&z->n, words);
	switch (words) {
	case 2:
/*@-shiftimplementation @*/
	    z->n.data[0] = (l >> 32) & 0xffffffff;
/*@=shiftimplementation @*/
	    z->n.data[1] = (l      ) & 0xffffffff;
	    break;
	case 1:
	    z->n.data[0] = (l      ) & 0xffffffff;
	    break;
	}
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_init(%p[%s],%p[%s],%p[%s]):\t", z, lbl(z), args, lbl(args), kwds, lbl(kwds)), mp32println(stderr, z->n.size, z->n.data);

    return 0;
}

/** \ingroup py_c  
 */
static void rpmbc_free(/*@only@*/ rpmbcObject * s)
	/*@modifies s @*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_free(%p[%s])\n", s, lbl(s));
    mpnfree(&s->n);
    PyObject_Del(s);
}

/** \ingroup py_c  
 */
static PyObject * rpmbc_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * ns = PyType_GenericAlloc(subtype, nitems);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_alloc(%p[%s},%d) ret %p[%s]\n", subtype, lbl(subtype), nitems, ns, lbl(ns));
    return (PyObject *) ns;
}

static PyObject *
rpmbc_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@*/
{
    PyObject * ns = (PyObject *) PyObject_New(rpmbcObject, &rpmbc_Type);

    if (ns != NULL)
	mpnzero(&((rpmbcObject *)ns)->n);

if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_new(%p[%s],%p[%s],%p[%s]) ret %p[%s]\n", subtype, lbl(subtype), args, lbl(args), kwds, lbl(kwds), ns, lbl(ns));
    return ns;
}

static rpmbcObject *
rpmbc_New(void)
{
    rpmbcObject * ns = PyObject_New(rpmbcObject, &rpmbc_Type);

    mpnzero(&ns->n);
    return ns;
}

static rpmbcObject *
rpmbc_i2bc(PyObject * o)
{
    if (is_rpmbc(o)) {
	Py_INCREF(o);
	return (rpmbcObject *)o;
    }
    if (PyInt_Check(o) || PyLong_Check(o)) {
	rpmbcObject * ns = PyObject_New(rpmbcObject, &rpmbc_Type);
	PyObject * args = PyTuple_New(1);

	mpnzero(&((rpmbcObject *)ns)->n);
	(void) PyTuple_SetItem(args, 0, o);
	rpmbc_init(ns, args, NULL);
	Py_DECREF(args);
	return ns;
    }

    PyErr_SetString(PyExc_TypeError, "number coercion (to rpmbcObject) failed");
    return NULL;
}

/* ---------- */

/** \ingroup py_c  
 */    
static PyObject * 
rpmbc_Debug(/*@unused@*/ rpmbcObject * s, PyObject * args)
        /*@globals _Py_NoneStruct @*/
        /*@modifies _Py_NoneStruct @*/ 
{
    if (!PyArg_ParseTuple(args, "i:Debug", &_bc_debug)) return NULL;
 
if (_bc_debug < 0) 
fprintf(stderr, "*** rpmbc_Debug(%p)\n", s);
 
    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup py_c  
 */    
static PyObject * 
rpmbc_Gcd(/*@unused@*/ rpmbcObject * s, PyObject * args)
        /*@*/ 
{
    PyObject * op1;
    PyObject * op2;
    rpmbcObject * a = NULL;
    rpmbcObject * b = NULL;
    rpmbcObject * z = NULL;
    uint32 * wksp = NULL;

    if (!PyArg_ParseTuple(args, "OO:Gcd", &op1, &op2)) return NULL;

    if ((a = rpmbc_i2bc(op1)) != NULL
     && (b = rpmbc_i2bc(op2)) != NULL
     && (z = rpmbc_New()) != NULL)
    {
	wksp = alloca((a->n.size) * sizeof(*wksp));
	mpnsize(&z->n, a->n.size);
	mp32gcd_w(a->n.size, a->n.data, b->n.data, z->n.data, wksp);
    }

if (_bc_debug) {
fprintf(stderr, "*** rpmbc_Gcd(%p):\t", s), mp32println(stderr, z->n.size, z->n.data);
fprintf(stderr, "    a(%p):\t", a), mp32println(stderr, a->n.size, a->n.data);
fprintf(stderr, "    b(%p):\t", b), mp32println(stderr, b->n.size, b->n.data);
}
 
    Py_DECREF(a);
    Py_DECREF(b);

    return (PyObject *)z;
}

/** \ingroup py_c  
 */    
static PyObject * 
rpmbc_Sqrt(/*@unused@*/ rpmbcObject * s, PyObject * args)
        /*@*/ 
{
    PyObject * op1;
    rpmbcObject * a = NULL;
    rpmbcObject * z = NULL;

if (_bc_debug < 0) 
fprintf(stderr, "*** rpmbc_Sqrt(%p)\n", s);
 
    if (!PyArg_ParseTuple(args, "O:Sqrt", &op1)) return NULL;

    if ((a = rpmbc_i2bc(op1)) != NULL
     && (z = rpmbc_New()) != NULL) {
	mpnsize(&z->n, a->n.size);
	mp32sqr(z->n.data, a->n.size, a->n.data);
    }

    Py_DECREF(a);

    return (PyObject *)z;
}

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmbc_methods[] = {
 {"Debug",	(PyCFunction)rpmbc_Debug,	METH_VARARGS,
	NULL},
 {"gcd",	(PyCFunction)rpmbc_Gcd,		METH_VARARGS,
	NULL},
 {"sqrt",	(PyCFunction)rpmbc_Sqrt,	METH_VARARGS,
	NULL},
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

static PyObject *
rpmbc_getattr(PyObject * s, char * name)
	/*@*/
{
    return Py_FindMethod(rpmbc_methods, s, name);
}

/* ---------- */

static PyObject *
rpmbc_add(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 carry;

    if ((z = rpmbc_New()) != NULL) {
	mpninit(&z->n, a->n.size, a->n.data);
	carry = mp32addx(z->n.size, z->n.data, b->n.size, b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_add(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 carry;

    if ((z = rpmbc_New()) != NULL) {
	mpninit(&z->n, a->n.size, a->n.data);
	carry = mp32subx(z->n.size, z->n.data, b->n.size, b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_subtract(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	uint32 zsize = a->n.size + b->n.size;
	uint32 *zdata = alloca(zsize * sizeof(*zdata));
	uint32 znorm;

	mp32mul(zdata, a->n.size, a->n.data, b->n.size, b->n.data);
	znorm = zsize - (mp32bitcnt(zsize, zdata) + 31)/32;
	zsize -= znorm;
	zdata += znorm;
	mpnset(&z->n, zsize, zdata);
if (_bc_debug)
fprintf(stderr, "*** rpmbc_multiply(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);
    }

    return (PyObject *)z;
}

static PyObject *
rpmbc_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if (mp32z(b->n.size, b->n.data)) {
	PyErr_SetString(PyExc_ZeroDivisionError, "rpmbc_divide by zero");
	return NULL;
    }

    if ((z = rpmbc_New()) != NULL) {
	uint32 asize = a->n.size;
	uint32 *adata = a->n.data;
	uint32 anorm = asize - (mp32bitcnt(asize, adata) + 31)/32;
	uint32 bsize = b->n.size;
	uint32 *bdata = b->n.data;
	uint32 bnorm = bsize - (mp32bitcnt(bsize, bdata) + 31)/32;
	uint32 zsize;
	uint32 *zdata;
	uint32 znorm;
	uint32 *wksp;

if (_bc_debug < 0)
fprintf(stderr, "*** a %p[%d]\t", adata, asize), mp32println(stderr, asize, adata);
if (_bc_debug < 0)
fprintf(stderr, "*** b %p[%d]\t", adata, asize), mp32println(stderr, bsize, bdata);
	if (anorm < asize) {
	    asize -= anorm;
	    adata += anorm;
	}
	zsize = asize + 1;
	zdata = alloca(zsize * sizeof(*zdata));
	if (bnorm < bsize) {
	    bsize -= bnorm;
	    bdata += bnorm;
	}
	wksp = alloca((bsize+1) * sizeof(*wksp));

if (_bc_debug < 0)
fprintf(stderr, "*** a %p[%d]\t", adata, asize), mp32println(stderr, asize, adata);
if (_bc_debug < 0)
fprintf(stderr, "*** b %p[%d]\t", adata, asize), mp32println(stderr, bsize, bdata);
	my32ndivmod(zdata, asize, adata, bsize, bdata, wksp);
if (_bc_debug < 0)
fprintf(stderr, "*** z %p[%d]\t", zdata, zsize), mp32println(stderr, zsize, zdata);
	zsize -= bsize;
	znorm = mp32size(zsize, zdata);
	if (znorm < zsize) {
	    zsize -= znorm;
	    zdata += znorm;
	}
if (_bc_debug < 0)
fprintf(stderr, "*** z %p[%d]\t", zdata, zsize), mp32println(stderr, zsize, zdata);
	mpnset(&z->n, zsize, zdata);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_divide(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);
    }

    return (PyObject *)z;
}

static PyObject *
rpmbc_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	uint32 bsize = b->n.size;
	uint32 *bdata = b->n.data;
	uint32 bnorm = mp32size(bsize, bdata);
	uint32 zsize = a->n.size;
	uint32 *zdata = alloca(zsize * sizeof(*zdata));
	uint32 *wksp;

	if (bnorm < bsize) {
	    bsize -= bnorm;
	    bdata += bnorm;
	}
	wksp = alloca((bsize+1) * sizeof(*wksp));

	mp32nmod(zdata, a->n.size, a->n.data, bsize, bdata, wksp);
	mpnset(&z->n, zsize, zdata);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_remainder(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);
    }

    return (PyObject *)z;
}

static PyObject *
rpmbc_divmod(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    PyObject * z = NULL;
    rpmbcObject * q = NULL;
    rpmbcObject * r = NULL;
    uint32 asize = a->n.size;
    uint32 *adata = a->n.data;
    uint32 anorm = mp32size(asize, adata);
    uint32 bsize = b->n.size;
    uint32 *bdata = b->n.data;
    uint32 bnorm = mp32size(bsize, bdata);
    uint32 zsize;
    uint32 *zdata;
    uint32 znorm;
    uint32 *wksp;

    if (mp32z(bsize, bdata)) {
	PyErr_SetString(PyExc_ZeroDivisionError, "rpmbc_divmod by zero");
	return NULL;
    }

    if ((z = PyTuple_New(2)) == NULL
     || (q = rpmbc_New()) == NULL
     || (r = rpmbc_New()) == NULL) {
	Py_XDECREF(z);
	Py_XDECREF(q);
	Py_XDECREF(r);
	return NULL;
    }

    if (anorm < asize) {
	asize -= anorm;
	adata += anorm;
    }
    zsize = asize + 1;
    zdata = alloca(zsize * sizeof(*zdata));
    if (bnorm < bsize) {
	bsize -= bnorm;
	bdata += bnorm;
    }
    wksp = alloca((bsize+1) * sizeof(*wksp));

    mp32ndivmod(zdata, asize, adata, bsize, bdata, wksp);

if (_bc_debug < 0)
fprintf(stderr, "*** a %p[%d] b %p[%d] z %p[%d]\t", adata, asize, bdata, bsize, zdata, zsize), mp32println(stderr, zsize, zdata);

    zsize -= bsize;
    mpnset(&r->n, bsize, zdata+zsize);

    znorm = mp32size(zsize, zdata);
    if (znorm < zsize) {
	zsize -= znorm;
	zdata += znorm;
    }
    mpnset(&q->n, zsize, zdata);

if (_bc_debug) {
fprintf(stderr, "*** rpmbc_divmod(%p,%p)\n", a, b);
fprintf(stderr, "    q(%p):\t", q), mp32println(stderr, q->n.size, q->n.data);
fprintf(stderr, "    r(%p):\t", r), mp32println(stderr, r->n.size, r->n.data);
}

    (void) PyTuple_SetItem(z, 0, (PyObject *)q);
    (void) PyTuple_SetItem(z, 1, (PyObject *)r);
    return (PyObject *)z;
}

static PyObject *
rpmbc_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {

	mpnpow_w(&z->n, a->n.size, a->n.data, b->n.size, b->n.data);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_power(%p,%p,%p):\t", a, b, c), mp32println(stderr, z->n.size, z->n.data);

    }

    return (PyObject *)z;
}

static PyObject *
rpmbc_negative(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	mpninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_negative(%p):\t", a), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_positive(rpmbcObject * a)
	/*@*/
{
    Py_INCREF(a);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_positive(%p):\t", a), mp32println(stderr, a->n.size, a->n.data);

    return (PyObject *)a;
}

static PyObject *
rpmbc_absolute(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

    if (mp32msbset(a->n.size, a->n.data) == 0) {
	Py_INCREF(a);
	return (PyObject *)a;
    }

    if ((z = rpmbc_New()) != NULL) {
	mpninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_absolute(%p):\t", a), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static int
rpmbc_nonzero(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_nonzero(%p)\n", a);
    return mp32nz(a->n.size, a->n.data);
}
		
static PyObject *
rpmbc_invert(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	mpninit(&z->n, a->n.size, a->n.data);
	mp32not(z->n.size, z->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_invert(%p):\t", a), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    /* XXX check shift count in range. */
    if ((z = rpmbc_New()) != NULL) {
	uint32 bnorm = b->n.size - (mp32bitcnt(b->n.size, b->n.data) + 31)/32;
	uint32 bsize = b->n.size - bnorm;
	uint32 * bdata = b->n.data + bnorm;
	uint32 count = 0;

	if (bsize == 1)
	    count = bdata[0];
	mpninit(&z->n, a->n.size, a->n.data);
	mp32lshift(z->n.size, z->n.data, count);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_lshift(%p[%s],%p[%s]):\t", a, lbl(a), b, lbl(b)), mp32println(stderr, z->n.size, z->n.data);

    }

    return (PyObject *)z;
}

static PyObject *
rpmbc_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    /* XXX check shift count in range. */
    if ((z = rpmbc_New()) != NULL) {
	uint32 bnorm = b->n.size - (mp32bitcnt(b->n.size, b->n.data) + 31)/32;
	uint32 bsize = b->n.size - bnorm;
	uint32 * bdata = b->n.data + bnorm;
	uint32 count = 0;

	if (bsize == 1)
	    count = bdata[0];
	mpninit(&z->n, a->n.size, a->n.data);
	mp32rshift(z->n.size, z->n.data, count);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_rshift(%p[%s],%p[%s]):\t", a, lbl(a), b, lbl(b)), mp32println(stderr, z->n.size, z->n.data);

    }

    return (PyObject *)z;
}

static PyObject *
rpmbc_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	rpmbcObject * c, * d;

	if (a->n.size <= b->n.size) {
	    c = a;
	    d = b;
	} else {
	    c = b;
	    d = a;
	}
	mpninit(&z->n, c->n.size, c->n.data);
	mp32and(z->n.size, z->n.data, d->n.data + (d->n.size - c->n.size));
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_and(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_xor(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	rpmbcObject * c, * d;

	if (a->n.size <= b->n.size) {
	    c = a;
	    d = b;
	} else {
	    c = b;
	    d = a;
	}
	mpninit(&z->n, c->n.size, c->n.data);
	mp32xor(z->n.size, z->n.data, d->n.data + (d->n.size - c->n.size));
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_xor(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	rpmbcObject * c, * d;

	if (a->n.size <= b->n.size) {
	    c = a;
	    d = b;
	} else {
	    c = b;
	    d = a;
	}
	mpninit(&z->n, c->n.size, c->n.data);
	mp32or(z->n.size, z->n.data, d->n.data + (d->n.size - c->n.size));
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_or(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static int
rpmbc_coerce(PyObject ** pv, PyObject ** pw)
	/*@modifies *pv, *pw @*/
{
    uint32 words = 0;
    long l = 0;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_coerce(%p[%s],%p[%s])\n", pv, lbl(*pv), pw, lbl(*pw));

    if (is_rpmbc(*pw)) {
	Py_INCREF(*pw);
    } else if (PyInt_Check(*pw)) {
	l = PyInt_AsLong(*pw);
	words = sizeof(l)/sizeof(words);
    } else if (PyLong_Check(*pw)) {
	l = PyLong_AsLong(*pw);
	words = sizeof(l)/sizeof(words);
    } else if (PyFloat_Check(*pw)) {
	double d = PyFloat_AsDouble(*pw);
	/* XXX TODO: check for overflow/underflow. */
	l = (long) (d + 0.5);
	words = sizeof(l)/sizeof(words);
    } else {
	PyErr_SetString(PyExc_TypeError, "non-numeric coercion failed (rpmbc_coerce)");
	return 1;
    }

    if (words > 0) {
	rpmbcObject * z = rpmbc_New();
	mpnsize(&z->n, words);
	switch (words) {
	case 2:
/*@-shiftimplementation @*/
	    z->n.data[0] = (l >> 32) & 0xffffffff;
/*@=shiftimplementation @*/
	    z->n.data[1] = (l      ) & 0xffffffff;
	    break;
	case 1:
	    z->n.data[0] = (l      ) & 0xffffffff;
	    break;
	}
	(*pw) = (PyObject *) z;
    }

    Py_INCREF(*pv);
    return 0;
}

static PyObject *
rpmbc_int(rpmbcObject * a)
	/*@*/
{
    uint32 anorm = a->n.size - (mp32bitcnt(a->n.size, a->n.data) + 31)/32;
    uint32 asize = a->n.size - anorm;
    uint32 * adata = a->n.data + anorm;

    if (asize > 1) {
	PyErr_SetString(PyExc_ValueError, "rpmbc_int: arg too long to convert");
	return NULL;
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_int(%p):\t%08x\n", a, (int)(asize ? adata[0] : 0));

    return Py_BuildValue("i", (asize ? adata[0] : 0));
}

static PyObject *
rpmbc_long(rpmbcObject * a)
	/*@*/
{
    uint32 anorm = a->n.size - (mp32bitcnt(a->n.size, a->n.data) + 31)/32;
    uint32 asize = a->n.size - anorm;
    uint32 * adata = a->n.data + anorm;

    if (asize > 1) {
	PyErr_SetString(PyExc_ValueError, "rpmbc_long() arg too long to convert");
	return NULL;
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_long(%p):\t%08lx\n", a, (long)(asize ? adata[0] : 0));

    return Py_BuildValue("l", (asize ? adata[0] : 0));
}

static PyObject *
rpmbc_float(rpmbcObject * a)
	/*@*/
{
    PyObject * so = rpmbc_format(a, 10, 0);
    char * s, * se;
    double d;

    if (so == NULL)
	return NULL;
    s = PyString_AS_STRING(so);
    se = NULL;
    d = strtod(s, &se);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_float(%p): s %p \"%s\" se %p d %g\n", a, s, s, se, d);
    Py_DECREF(so);

    return Py_BuildValue("d", d);
}

static PyObject *
rpmbc_oct(rpmbcObject * a)
	/*@*/
{
    PyObject * so = rpmbc_format(a, 8, 1);
if (_bc_debug && so != NULL)
fprintf(stderr, "*** rpmbc_oct(%p): \"%s\"\n", a, PyString_AS_STRING(so));
    return so;
}

static PyObject *
rpmbc_hex(rpmbcObject * a)
	/*@*/
{
    PyObject * so = rpmbc_format(a, 16, 1);
if (_bc_debug && so != NULL)
fprintf(stderr, "*** rpmbc_hex(%p): \"%s\"\n", a, PyString_AS_STRING(so));
    return so;
}

static PyObject *
rpmbc_inplace_add(rpmbcObject * a, rpmbcObject * b)
	/*@modifies a @*/
{
    uint32 carry;

    carry = mp32addx(a->n.size, a->n.data, b->n.size, b->n.data);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_add(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@modifies a @*/
{
    uint32 carry;

    carry = mp32subx(a->n.size, a->n.data, b->n.size, b->n.data);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_subtract(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 zsize = a->n.size + b->n.size;
    uint32 *zdata = alloca(zsize * sizeof(*zdata));
    uint32 znorm;

    mp32mul(zdata, a->n.size, a->n.data, b->n.size, b->n.data);
    znorm = zsize - (mp32bitcnt(zsize, zdata) + 31)/32;
    zsize -= znorm;
    zdata += znorm;

    mpnset(&a->n, zsize, zdata);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_multiply(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 asize = a->n.size;
    uint32 *adata = a->n.data;
    uint32 anorm = mp32size(asize, adata);
    uint32 bsize = b->n.size;
    uint32 *bdata = b->n.data;
    uint32 bnorm = mp32size(bsize, bdata);
    uint32 zsize;
    uint32 *zdata;
    uint32 znorm;
    uint32 *wksp;

    if (anorm < asize) {
	asize -= anorm;
	adata += anorm;
    }
    zsize = asize + 1;
    zdata = alloca(zsize * sizeof(*zdata));
    if (bnorm < bsize) {
	bsize -= bnorm;
	bdata += bnorm;
    }
    wksp = alloca((bsize+1) * sizeof(*wksp));

    mp32ndivmod(zdata, asize, adata, bsize, bdata, wksp);
if (_bc_debug < 0)
fprintf(stderr, "*** a %p[%d] b %p[%d] z %p[%d]\t", adata, asize, bdata, bsize, zdata, zsize), mp32println(stderr, zsize, zdata);
    zsize -= bsize;
    znorm = mp32size(zsize, zdata);
    if (znorm < zsize) {
	zsize -= znorm;
	zdata += znorm;
    }
    mpnset(&a->n, zsize, zdata);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_divide(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 bsize = b->n.size;
    uint32 * bdata = b->n.data;
    uint32 bnorm = bsize - (mp32bitcnt(bsize, bdata) + 31)/32;
    uint32 zsize = a->n.size;
    uint32 * zdata = alloca(zsize * sizeof(*zdata));
    uint32 * wksp;

    if (bnorm < bsize) {
	bsize -= bnorm;
	bdata += bnorm;
    }
    wksp = alloca((bsize+1) * sizeof(*wksp));

    mp32nmod(zdata, a->n.size, a->n.data, bsize, bdata, wksp);
    mpnset(&a->n, zsize, zdata);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_remainder(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_power(%p,%p,%p):\t", a, b, c), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@modifies a @*/
{
    uint32 bnorm = b->n.size - (mp32bitcnt(b->n.size, b->n.data) + 31)/32;
    uint32 bsize = b->n.size - bnorm;
    uint32 * bdata = b->n.data + bnorm;
    uint32 count = 0;

    /* XXX check shift count in range. */
    if (bsize == 1)
	count = bdata[0];
    mp32lshift(a->n.size, a->n.data, bdata[0]);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_lshift(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@modifies a @*/
{
    uint32 bnorm = b->n.size - (mp32bitcnt(b->n.size, b->n.data) + 31)/32;
    uint32 bsize = b->n.size - bnorm;
    uint32 * bdata = b->n.data + bnorm;
    uint32 count = 0;

    /* XXX check shift count in range. */
    if (bsize == 1)
	count = bdata[0];
    mp32rshift(a->n.size, a->n.data, count);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_rshift(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    if (a->n.size <= b->n.size)
	mp32and(a->n.size, a->n.data, b->n.data + (b->n.size - a->n.size));
    else {
	memset(a->n.data, 0, (a->n.size - b->n.size) * sizeof(*a->n.data));
	mp32and(a->n.size, a->n.data + (a->n.size - b->n.size), b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_and(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_xor(rpmbcObject * a, rpmbcObject * b)
{
    if (a->n.size <= b->n.size)
	mp32xor(a->n.size, a->n.data, b->n.data + (b->n.size - a->n.size));
    else {
	memset(a->n.data, 0, (a->n.size - b->n.size) * sizeof(*a->n.data));
	mp32xor(a->n.size, a->n.data + (a->n.size - b->n.size), b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_xor(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    if (a->n.size <= b->n.size)
	mp32or(a->n.size, a->n.data, b->n.data + (b->n.size - a->n.size));
    else {
	memset(a->n.data, 0, (a->n.size - b->n.size) * sizeof(*a->n.data));
	mp32or(a->n.size, a->n.data + (a->n.size - b->n.size), b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_or(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_floor_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_true_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_floor_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_true_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyNumberMethods rpmbc_as_number = {
	(binaryfunc) rpmbc_add,			/* nb_add */
	(binaryfunc) rpmbc_subtract,		/* nb_subtract */
	(binaryfunc) rpmbc_multiply,		/* nb_multiply */
	(binaryfunc) rpmbc_divide,		/* nb_divide */
	(binaryfunc) rpmbc_remainder,		/* nb_remainder */
	(binaryfunc) rpmbc_divmod,		/* nb_divmod */
	(ternaryfunc) rpmbc_power,		/* nb_power */
	(unaryfunc) rpmbc_negative,		/* nb_negative */
	(unaryfunc) rpmbc_positive,		/* nb_positive */
	(unaryfunc) rpmbc_absolute,		/* nb_absolute */
	(inquiry) rpmbc_nonzero,		/* nb_nonzero */
	(unaryfunc) rpmbc_invert,		/* nb_invert */
	(binaryfunc) rpmbc_lshift,		/* nb_lshift */
	(binaryfunc) rpmbc_rshift,		/* nb_rshift */
	(binaryfunc) rpmbc_and,			/* nb_and */
	(binaryfunc) rpmbc_xor,			/* nb_xor */
	(binaryfunc) rpmbc_or,			/* nb_or */
	(coercion) rpmbc_coerce,		/* nb_coerce */

	(unaryfunc) rpmbc_int,			/* nb_int */
	(unaryfunc) rpmbc_long,			/* nb_long */
	(unaryfunc) rpmbc_float,		/* nb_float */
	(unaryfunc) rpmbc_oct,			/* nb_oct */
	(unaryfunc) rpmbc_hex,			/* nb_hex */

	/* Added in release 2.0 */
	(binaryfunc) rpmbc_inplace_add,		/* nb_inplace_add */
	(binaryfunc) rpmbc_inplace_subtract,	/* nb_inplace_subtract */
	(binaryfunc) rpmbc_inplace_multiply,	/* nb_inplace_multiply */
	(binaryfunc) rpmbc_inplace_divide,	/* nb_inplace_divide */
	(binaryfunc) rpmbc_inplace_remainder,	/* nb_inplace_remainder */
	(ternaryfunc) rpmbc_inplace_power,	/* nb_inplace_power */
	(binaryfunc) rpmbc_inplace_lshift,	/* nb_inplace_lshift */
	(binaryfunc) rpmbc_inplace_rshift,	/* nb_inplace_rshift */
	(binaryfunc) rpmbc_inplace_and,		/* nb_inplace_and */
	(binaryfunc) rpmbc_inplace_xor,		/* nb_inplace_xor */
	(binaryfunc) rpmbc_inplace_or,		/* nb_inplace_or */

	/* Added in release 2.2 */
	/* The following require the Py_TPFLAGS_HAVE_CLASS flag */
	(binaryfunc) rpmbc_floor_divide,	/* nb_floor_divide */
	(binaryfunc) rpmbc_true_divide,		/* nb_true_divide */
	(binaryfunc) rpmbc_inplace_floor_divide,/* nb_inplace_floor_divide */
	(binaryfunc) rpmbc_inplace_true_divide	/* nb_inplace_true_divide */

};

/* ---------- */

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmbc_doc[] =
"";

/*@-fullinitblock@*/
PyTypeObject rpmbc_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.bc",			/* tp_name */
	sizeof(rpmbcObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmbc_dealloc,	/* tp_dealloc */
	(printfunc) rpmbc_print,	/* tp_print */
	(getattrfunc) rpmbc_getattr,	/* tp_getattr */
	(setattrfunc) 0,		/* tp_setattr */
	(cmpfunc) rpmbc_compare,	/* tp_compare */
	(reprfunc) rpmbc_repr,		/* tp_repr */
	&rpmbc_as_number,		/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc) 0,			/* tp_hash */
	(ternaryfunc) 0,		/* tp_call */
	(reprfunc) rpmbc_str,		/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmbc_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) 0,		/* tp_iter */
	(iternextfunc) 0,		/* tp_iternext */
	rpmbc_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmbc_init,		/* tp_init */
	(allocfunc) rpmbc_alloc,	/* tp_alloc */
	(newfunc) rpmbc_new,		/* tp_new */
	(destructor) rpmbc_free,	/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

/* ---------- */
