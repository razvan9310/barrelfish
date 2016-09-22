#include "test.h"
 one_line_type yn_vec[] = {
{64,0,123,__LINE__, 0x3ff00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
{64,0,123,__LINE__, 0x00000000, 0x00000000, 0x3ff00000, 0x00000000, 0x00000000, 0x00000000},
{64,0,123,__LINE__, 0x00000000, 0x00000000, 0x40000000, 0x00000000, 0x00000000, 0x00000000},
{64,0,123,__LINE__, 0x00000000, 0x00000000, 0x40080000, 0x00000000, 0x00000000, 0x00000000},
{64,0,123,__LINE__, 0x00000000, 0x00000000, 0x40100000, 0x00000000, 0x00000000, 0x00000000},
{64,0,123,__LINE__, 0x3feffae1, 0x7c1aebb8, 0x00000000, 0x00000000, 0x3fa99999, 0x9999999a},
{64,0,123,__LINE__, 0x3f99978d, 0x5dda2f5d, 0x3ff00000, 0x00000000, 0x3fa99999, 0x9999999a},
{64,0,123,__LINE__, 0x3f3479c9, 0xae7be13e, 0x40000000, 0x00000000, 0x3fa99999, 0x9999999a},
{64,0,123,__LINE__, 0x3ec5d788, 0x15461382, 0x40080000, 0x00000000, 0x3fa99999, 0x9999999a},
{64,0,123,__LINE__, 0x3e51795d, 0x7442f11f, 0x40100000, 0x00000000, 0x3fa99999, 0x9999999a},
{64,0,123,__LINE__, 0x3fefeb88, 0x65590ab3, 0x00000000, 0x00000000, 0x3fb99999, 0x9999999a},
{64,0,123,__LINE__, 0x3fa99169, 0x52566dfd, 0x3ff00000, 0x00000000, 0x3fb99999, 0x9999999a},
{64,0,123,__LINE__, 0x3f547683, 0x25fd91e1, 0x40000000, 0x00000000, 0x3fb99999, 0x9999999a},
{64,0,123,__LINE__, 0x3ef5d4e9, 0x3442db3c, 0x40080000, 0x00000000, 0x3fb99999, 0x9999999a},
{64,0,123,__LINE__, 0x3e9177b0, 0x11ba6ea0, 0x40100000, 0x00000000, 0x3fb99999, 0x9999999a},
{64,0,123,__LINE__, 0x3fefd1fc, 0x19331411, 0x00000000, 0x00000000, 0x3fc33333, 0x33333334},
{64,0,123,__LINE__, 0x3fb32563, 0x927c0add, 0x3ff00000, 0x00000000, 0x3fc33333, 0x33333334},
{64,0,123,__LINE__, 0x3f66ff30, 0x46535fa1, 0x40000000, 0x00000000, 0x3fc33333, 0x33333334},
{64,0,123,__LINE__, 0x3f1267f5, 0xd0689e2d, 0x40080000, 0x00000000, 0x3fc33333, 0x33333334},
{64,0,123,__LINE__, 0x3eb617f1, 0x7b30b575, 0x40100000, 0x00000000, 0x3fc33333, 0x33333334},
{64,0,123,__LINE__, 0x3fefae48, 0xd9bfc0d4, 0x00000000, 0x00000000, 0x3fc99999, 0x9999999a},
{64,0,123,__LINE__, 0x3fb978e2, 0xf61c3bd6, 0x3ff00000, 0x00000000, 0x3fc99999, 0x9999999a},
{64,0,123,__LINE__, 0x3f74696c, 0xf1c4fb46, 0x40000000, 0x00000000, 0x3fc99999, 0x9999999a},
{64,0,123,__LINE__, 0x3f25ca70, 0x33fc81fd, 0x40080000, 0x00000000, 0x3fc99999, 0x9999999a},
{64,0,123,__LINE__, 0x3ed170fb, 0xdefa5db8, 0x40100000, 0x00000000, 0x3fc99999, 0x9999999a},
{64,0,123,__LINE__, 0x3fef807f, 0xc72aa864, 0x00000000, 0x00000000, 0x3fd00000, 0x00000000},
{64,0,123,__LINE__, 0x3fbfc02a, 0x9c749ee9, 0x3ff00000, 0x00000000, 0x3fd00000, 0x00000000},
{64,0,123,__LINE__, 0x3f7fd56a, 0xa4fb4242, 0x40000000, 0x00000000, 0x3fd00000, 0x00000000},
{64,0,123,__LINE__, 0x3f354008, 0x86a35a2f, 0x40080000, 0x00000000, 0x3fd00000, 0x00000000},
{64,0,123,__LINE__, 0x3ee54449, 0xf38a05f2, 0x40100000, 0x00000000, 0x3fd00000, 0x00000000},
{64,0,123,__LINE__, 0x3fef48b6, 0xd692fb9e, 0x00000000, 0x00000000, 0x3fd33333, 0x33333333},
{64,0,123,__LINE__, 0x3fc2fc1c, 0x68a32001, 0x3ff00000, 0x00000000, 0x3fd33333, 0x33333333},
{64,0,123,__LINE__, 0x3f86de20, 0x9f39196e, 0x40000000, 0x00000000, 0x3fd33333, 0x33333333},
{64,0,123,__LINE__, 0x3f42541c, 0x0c752fd9, 0x40080000, 0x00000000, 0x3fd33333, 0x33333333},
{64,0,123,__LINE__, 0x3ef604e0, 0xb2c4c00a, 0x40100000, 0x00000000, 0x3fd33333, 0x33333333},
{64,0,123,__LINE__, 0x3fef0708, 0xc6573ae5, 0x00000000, 0x00000000, 0x3fd66666, 0x66666666},
{64,0,123,__LINE__, 0x3fc60f09, 0xfed3cc81, 0x3ff00000, 0x00000000, 0x3fd66666, 0x66666666},
{64,0,123,__LINE__, 0x3f8f0a84, 0xda6806f7, 0x40000000, 0x00000000, 0x3fd66666, 0x66666666},
{64,0,123,__LINE__, 0x3f4d0bc1, 0xbfa630ab, 0x40080000, 0x00000000, 0x3fd66666, 0x66666666},
{64,0,123,__LINE__, 0x3f045d05, 0x37bf774c, 0x40100000, 0x00000000, 0x3fd66666, 0x66666666},
{64,0,123,__LINE__, 0x3feebb95, 0x0fd4747f, 0x00000000, 0x00000000, 0x3fd99999, 0x99999999},
{64,0,123,__LINE__, 0x3fc91766, 0x1ebb8177, 0x3ff00000, 0x00000000, 0x3fd99999, 0x99999999},
{64,0,123,__LINE__, 0x3f943552, 0xd2bdaaf3, 0x40000000, 0x00000000, 0x3fd99999, 0x99999999},
{64,0,123,__LINE__, 0x3f55a0b4, 0x58ca1be8, 0x40080000, 0x00000000, 0x3fd99999, 0x99999999},
{64,0,123,__LINE__, 0x3f115640, 0x7fcf3789, 0x40100000, 0x00000000, 0x3fd99999, 0x99999999},
{64,0,123,__LINE__, 0x3fee667f, 0xd6a10561, 0x00000000, 0x00000000, 0x3fdccccc, 0xcccccccc},
{64,0,123,__LINE__, 0x3fcc13be, 0x77afcc52, 0x3ff00000, 0x00000000, 0x3fdccccc, 0xcccccccc},
{64,0,123,__LINE__, 0x3f997c40, 0x76110a23, 0x40000000, 0x00000000, 0x3fdccccc, 0xcccccccc},
{64,0,123,__LINE__, 0x3f5eb65b, 0x151f786c, 0x40080000, 0x00000000, 0x3fdccccc, 0xcccccccc},
{64,0,123,__LINE__, 0x3f1bb61b, 0x892f8cff, 0x40100000, 0x00000000, 0x3fdccccc, 0xcccccccc},
{64,0,123,__LINE__, 0x3fee07f1, 0xd54c3f35, 0x00000000, 0x00000000, 0x3fdfffff, 0xffffffff},
{64,0,123,__LINE__, 0x3fcf02a7, 0x1f4870d6, 0x3ff00000, 0x00000000, 0x3fdfffff, 0xffffffff},
{64,0,123,__LINE__, 0x3f9f56a9, 0x3f863441, 0x40000000, 0x00000000, 0x3fdfffff, 0xffffffff},
{64,0,123,__LINE__, 0x3f650088, 0x0f70db57, 0x40080000, 0x00000000, 0x3fdfffff, 0xffffffff},
{64,0,123,__LINE__, 0x3f25116b, 0xd18a61a9, 0x40100000, 0x00000000, 0x3fdfffff, 0xffffffff},
{64,0,123,__LINE__, 0x3feda018, 0x47adb931, 0x00000000, 0x00000000, 0x3fe19999, 0x99999999},
{64,0,123,__LINE__, 0x3fd0f15d, 0xa9534f53, 0x3ff00000, 0x00000000, 0x3fe19999, 0x99999999},
{64,0,123,__LINE__, 0x3fa2e066, 0x08ca87ed, 0x40000000, 0x00000000, 0x3fe19999, 0x99999999},
{64,0,123,__LINE__, 0x3f6bdca3, 0xb2679435, 0x40080000, 0x00000000, 0x3fe19999, 0x99999999},
{64,0,123,__LINE__, 0x3f2ec3c2, 0x4d8baa6f, 0x40100000, 0x00000000, 0x3fe19999, 0x99999999},
{64,0,123,__LINE__, 0x3fed2f24, 0xd2d06e4e, 0x00000000, 0x00000000, 0x3fe33333, 0x33333333},
{64,0,123,__LINE__, 0x3fd2594f, 0x19ddc92f, 0x3ff00000, 0x00000000, 0x3fe33333, 0x33333333},
{64,0,123,__LINE__, 0x3fa65b45, 0x84be102a, 0x40000000, 0x00000000, 0x3fe33333, 0x33333333},
{64,0,123,__LINE__, 0x3f72055f, 0xdad11213, 0x40080000, 0x00000000, 0x3fe33333, 0x33333333},
{64,0,123,__LINE__, 0x3f35b926, 0x63a336fb, 0x40100000, 0x00000000, 0x3fe33333, 0x33333333},
{64,0,123,__LINE__, 0x3fecb54d, 0x6a872136, 0x00000000, 0x00000000, 0x3fe4cccc, 0xcccccccd},
{64,0,123,__LINE__, 0x3fd3b87d, 0xc1127dbb, 0x3ff00000, 0x00000000, 0x3fe4cccc, 0xcccccccd},
{64,0,123,__LINE__, 0x3faa19b6, 0xfbcb3e9a, 0x40000000, 0x00000000, 0x3fe4cccc, 0xcccccccd},
{64,0,123,__LINE__, 0x3f76d285, 0x13b1873b, 0x40080000, 0x00000000, 0x3fe4cccc, 0xcccccccd},
{64,0,123,__LINE__, 0x3f3dd3be, 0xb2075517, 0x40100000, 0x00000000, 0x3fe4cccc, 0xcccccccd},
{64,0,123,__LINE__, 0x3fec32cc, 0x34b8cc59, 0x00000000, 0x00000000, 0x3fe66666, 0x66666667},
{64,0,123,__LINE__, 0x3fd50e44, 0x279c0546, 0x3ff00000, 0x00000000, 0x3fe66666, 0x66666667},
{64,0,123,__LINE__, 0x3fae1952, 0x86f3b2fe, 0x40000000, 0x00000000, 0x3fe66666, 0x66666667},
{64,0,123,__LINE__, 0x3f7c6245, 0x0da7c943, 0x40080000, 0x00000000, 0x3fe66666, 0x66666667},
{64,0,123,__LINE__, 0x3f43fddd, 0x592928af, 0x40100000, 0x00000000, 0x3fe66666, 0x66666667},
{64,0,123,__LINE__, 0x3feba7df, 0x6a752a19, 0x00000000, 0x00000000, 0x3fe80000, 0x00000001},
{64,0,123,__LINE__, 0x3fd65a01, 0xd66b68bd, 0x3ff00000, 0x00000000, 0x3fe80000, 0x00000001},
{64,0,123,__LINE__, 0x3fb12bc2, 0xf0d061c1, 0x40000000, 0x00000000, 0x3fe80000, 0x00000001},
{64,0,123,__LINE__, 0x3f816042, 0xaaa332db, 0x40080000, 0x00000000, 0x3fe80000, 0x00000001},
{64,0,123,__LINE__, 0x3f4a3fdc, 0xe9688cf6, 0x40100000, 0x00000000, 0x3fe80000, 0x00000001},
{64,0,123,__LINE__, 0x3feb14c9, 0x36e29d84, 0x00000000, 0x00000000, 0x3fe99999, 0x9999999b},
{64,0,123,__LINE__, 0x3fd79b1b, 0xab574ece, 0x3ff00000, 0x00000000, 0x3fe99999, 0x9999999b},
{64,0,123,__LINE__, 0x3fb368ca, 0xfa5427de, 0x40000000, 0x00000000, 0x3fe99999, 0x9999999b},
{64,0,123,__LINE__, 0x3f84fc41, 0xb23c60e3, 0x40080000, 0x00000000, 0x3fe99999, 0x9999999b},
{64,0,123,__LINE__, 0x3f50eca7, 0x311cbd4c, 0x40100000, 0x00000000, 0x3fe99999, 0x9999999b},
{64,0,123,__LINE__, 0x3fea79cf, 0x9417f64a, 0x00000000, 0x00000000, 0x3feb3333, 0x33333335},
{64,0,123,__LINE__, 0x3fd8d0fc, 0x2ac09609, 0x3ff00000, 0x00000000, 0x3feb3333, 0x33333335},
{64,0,123,__LINE__, 0x3fb5c250, 0x3ceb775b, 0x40000000, 0x00000000, 0x3feb3333, 0x33333335},
{64,0,123,__LINE__, 0x3f890a65, 0x7f429003, 0x40080000, 0x00000000, 0x3feb3333, 0x33333335},
{64,0,123,__LINE__, 0x3f557acc, 0xd794bfe8, 0x40100000, 0x00000000, 0x3feb3333, 0x33333335},
{64,0,123,__LINE__, 0x3fe9d73c, 0x25f5b277, 0x00000000, 0x00000000, 0x3feccccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3fd9fb13, 0xce0f024e, 0x3ff00000, 0x00000000, 0x3feccccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3fb836ce, 0xdb8280b1, 0x40000000, 0x00000000, 0x3feccccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3f8d8f96, 0x8206eb0e, 0x40080000, 0x00000000, 0x3feccccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3f5ae0f9, 0x8b7b7574, 0x40100000, 0x00000000, 0x3feccccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3fe92d5c, 0x13137d0b, 0x00000000, 0x00000000, 0x3fee6666, 0x66666669},
{64,0,123,__LINE__, 0x3fdb18d9, 0x4eda7476, 0x3ff00000, 0x00000000, 0x3fee6666, 0x66666669},
{64,0,123,__LINE__, 0x3fbac4b1, 0x27d714b1, 0x40000000, 0x00000000, 0x3fee6666, 0x66666669},
{64,0,123,__LINE__, 0x3f91482d, 0xb156b6bd, 0x40080000, 0x00000000, 0x3fee6666, 0x66666669},
{64,0,123,__LINE__, 0x3f609b4b, 0x7ea68879, 0x40100000, 0x00000000, 0x3fee6666, 0x66666669},
{64,0,123,__LINE__, 0x3fe87c7f, 0xdbd7b8ee, 0x00000000, 0x00000000, 0x3ff00000, 0x00000001},
{64,0,123,__LINE__, 0x3fdc29c9, 0xee970c6e, 0x3ff00000, 0x00000000, 0x3ff00000, 0x00000001},
{64,0,123,__LINE__, 0x3fbd6a50, 0x95fa9be9, 0x40000000, 0x00000000, 0x3ff00000, 0x00000001},
{64,0,123,__LINE__, 0x3f94086a, 0x7638f7a6, 0x40080000, 0x00000000, 0x3ff00000, 0x00000001},
{64,0,123,__LINE__, 0x3f6449e3, 0x6b5af1b3, 0x40100000, 0x00000000, 0x3ff00000, 0x00000001},
{64,0,123,__LINE__, 0x3fe7c4fb, 0x2fcfebef, 0x00000000, 0x00000000, 0x3ff0cccc, 0xccccccce},
{64,0,123,__LINE__, 0x3fdd2d69, 0xba9c976c, 0x3ff00000, 0x00000000, 0x3ff0cccc, 0xccccccce},
{64,0,123,__LINE__, 0x3fc012fb, 0x5cfc78b3, 0x40000000, 0x00000000, 0x3ff0cccc, 0xccccccce},
{64,0,123,__LINE__, 0x3f970a5d, 0x1eef9226, 0x40080000, 0x00000000, 0x3ff0cccc, 0xccccccce},
{64,0,123,__LINE__, 0x3f6888a5, 0x228510b7, 0x40100000, 0x00000000, 0x3ff0cccc, 0xccccccce},
{64,0,123,__LINE__, 0x3fe70724, 0xc161d44c, 0x00000000, 0x00000000, 0x3ff19999, 0x9999999b},
{64,0,123,__LINE__, 0x3fde2343, 0xcc63c37e, 0x3ff00000, 0x00000000, 0x3ff19999, 0x9999999b},
{64,0,123,__LINE__, 0x3fc17aef, 0x27865e62, 0x40000000, 0x00000000, 0x3ff19999, 0x9999999b},
{64,0,123,__LINE__, 0x3f9a4faa, 0xa04e8191, 0x40080000, 0x00000000, 0x3ff19999, 0x9999999b},
{64,0,123,__LINE__, 0x3f6d6434, 0x5a55c316, 0x40100000, 0x00000000, 0x3ff19999, 0x9999999b},
{64,0,123,__LINE__, 0x3fe64356, 0x17eddc81, 0x00000000, 0x00000000, 0x3ff26666, 0x66666668},
{64,0,123,__LINE__, 0x3fdf0aea, 0x85d5bf18, 0x3ff00000, 0x00000000, 0x3ff26666, 0x66666668},
{64,0,123,__LINE__, 0x3fc2ec1a, 0x23e21b78, 0x40000000, 0x00000000, 0x3ff26666, 0x66666668},
{64,0,123,__LINE__, 0x3f9dd9bf, 0xb5a70ade, 0x40080000, 0x00000000, 0x3ff26666, 0x66666668},
{64,0,123,__LINE__, 0x3f7174b5, 0x74230428, 0x40100000, 0x00000000, 0x3ff26666, 0x66666668},
{64,0,123,__LINE__, 0x3fe579eb, 0x607c7c41, 0x00000000, 0x00000000, 0x3ff33333, 0x33333335},
{64,0,123,__LINE__, 0x3fdfe3f7, 0xc98d2caf, 0x3ff00000, 0x00000000, 0x3ff33333, 0x33333335},
{64,0,123,__LINE__, 0x3fc4658c, 0x7339f932, 0x40000000, 0x00000000, 0x3ff33333, 0x33333335},
{64,0,123,__LINE__, 0x3fa0d4e7, 0xb3f0ea6f, 0x40080000, 0x00000000, 0x3ff33333, 0x33333335},
{64,0,123,__LINE__, 0x3f7492a5, 0xb6657ad7, 0x40100000, 0x00000000, 0x3ff33333, 0x33333335},
{64,0,123,__LINE__, 0x3fe4ab43, 0x3d10e1be, 0x00000000, 0x00000000, 0x3ff40000, 0x00000002},
{64,0,123,__LINE__, 0x3fe05706, 0x9774d334, 0x3ff00000, 0x00000000, 0x3ff40000, 0x00000002},
{64,0,123,__LINE__, 0x3fc5e650, 0x6ea82715, 0x40000000, 0x00000000, 0x3ff40000, 0x00000002},
{64,0,123,__LINE__, 0x3fa2e068, 0xdde7f41d, 0x40080000, 0x00000000, 0x3ff40000, 0x00000002},
{64,0,123,__LINE__, 0x3f781279, 0xda92ee2f, 0x40100000, 0x00000000, 0x3ff40000, 0x00000002},
{64,0,123,__LINE__, 0x3fe3d7be, 0x92bbfbb9, 0x00000000, 0x00000000, 0x3ff4cccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3fe0b46a, 0x18ecb9d4, 0x3ff00000, 0x00000000, 0x3ff4cccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3fc76d6b, 0x3ac0d75b, 0x40000000, 0x00000000, 0x3ff4cccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3fa50fc1, 0x44526e78, 0x40080000, 0x00000000, 0x3ff4cccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3f7bfac7, 0x6edc65a7, 0x40100000, 0x00000000, 0x3ff4cccc, 0xcccccccf},
{64,0,123,__LINE__, 0x3fe2ffc0, 0x568baaa2, 0x00000000, 0x00000000, 0x3ff59999, 0x9999999c},
{64,0,123,__LINE__, 0x3fe109ff, 0x2f1fc3ec, 0x3ff00000, 0x00000000, 0x3ff59999, 0x9999999c},
{64,0,123,__LINE__, 0x3fc8f9dd, 0x5e41b884, 0x40000000, 0x00000000, 0x3ff59999, 0x9999999c},
{64,0,123,__LINE__, 0x3fa76330, 0x3bb95571, 0x40080000, 0x00000000, 0x3ff59999, 0x9999999c},
{64,0,123,__LINE__, 0x3f802911, 0xcfdfbbf5, 0x40100000, 0x00000000, 0x3ff59999, 0x9999999c},
{64,0,123,__LINE__, 0x3fe223ad, 0x59727c9e, 0x00000000, 0x00000000, 0x3ff66666, 0x66666669},
{64,0,123,__LINE__, 0x3fe157a2, 0xbb6f3312, 0x3ff00000, 0x00000000, 0x3ff66666, 0x66666669},
{64,0,123,__LINE__, 0x3fca8aa3, 0x5b8ce82e, 0x40000000, 0x00000000, 0x3ff66666, 0x66666669},
{64,0,123,__LINE__, 0x3fa9dad6, 0x163299ee, 0x40080000, 0x00000000, 0x3ff66666, 0x66666669},
{64,0,123,__LINE__, 0x3f828f8e, 0x0cde14e4, 0x40100000, 0x00000000, 0x3ff66666, 0x66666669},
{64,0,123,__LINE__, 0x3fe143ec, 0x1344e613, 0x00000000, 0x00000000, 0x3ff73333, 0x33333336},
{64,0,123,__LINE__, 0x3fe19d35, 0xcbd98b75, 0x3ff00000, 0x00000000, 0x3ff73333, 0x33333336},
{64,0,123,__LINE__, 0x3fcc1eb6, 0x4c93d2fc, 0x40000000, 0x00000000, 0x3ff73333, 0x33333336},
{64,0,123,__LINE__, 0x3fac76b3, 0xcd3060b1, 0x40080000, 0x00000000, 0x3ff73333, 0x33333336},
{64,0,123,__LINE__, 0x3f853417, 0xedc03cbe, 0x40100000, 0x00000000, 0x3ff73333, 0x33333336},
{64,0,123,__LINE__, 0x3fe060e4, 0x6ce96517, 0x00000000, 0x00000000, 0x3ff80000, 0x00000003},
{64,0,123,__LINE__, 0x3fe1da9d, 0xa9d6fc82, 0x3ff00000, 0x00000000, 0x3ff80000, 0x00000003},
{64,0,123,__LINE__, 0x3fcdb50c, 0x80d5039f, 0x40000000, 0x00000000, 0x3ff80000, 0x00000003},
{64,0,123,__LINE__, 0x3faf36aa, 0xc0c5b3bb, 0x40080000, 0x00000000, 0x3ff80000, 0x00000003},
{64,0,123,__LINE__, 0x3f8819e3, 0xff0b0187, 0x40100000, 0x00000000, 0x3ff80000, 0x00000003},
{64,0,123,__LINE__, 0x3fdef5ff, 0x13b2d492, 0x00000000, 0x00000000, 0x3ff8cccc, 0xccccccd0},
{64,0,123,__LINE__, 0x3fe20fc3, 0xe6dcf000, 0x3ff00000, 0x00000000, 0x3ff8cccc, 0xccccccd0},
{64,0,123,__LINE__, 0x3fcf4c9a, 0x1d0ea964, 0x40000000, 0x00000000, 0x3ff8cccc, 0xccccccd0},
{64,0,123,__LINE__, 0x3fb10d3e, 0x464b660f, 0x40080000, 0x00000000, 0x3ff8cccc, 0xccccccd0},
{64,0,123,__LINE__, 0x3f8b4418, 0x3c555ec0, 0x40100000, 0x00000000, 0x3ff8cccc, 0xccccccd0},
{64,0,123,__LINE__, 0x3fdd254f, 0x22227934, 0x00000000, 0x00000000, 0x3ff99999, 0x9999999d},
{64,0,123,__LINE__, 0x3fe23c96, 0x66824fe1, 0x3ff00000, 0x00000000, 0x3ff99999, 0x9999999d},
{64,0,123,__LINE__, 0x3fd07228, 0xde234e77, 0x40000000, 0x00000000, 0x3ff99999, 0x9999999d},
{64,0,123,__LINE__, 0x3fb290e5, 0x794e918a, 0x40080000, 0x00000000, 0x3ff99999, 0x9999999d},
{64,0,123,__LINE__, 0x3f8eb5c8, 0x72cb3f07, 0x40100000, 0x00000000, 0x3ff99999, 0x9999999d},
{64,0,123,__LINE__, 0x3fdb508e, 0xeb1aae8c, 0x00000000, 0x00000000, 0x3ffa6666, 0x6666666a},
{64,0,123,__LINE__, 0x3fe26107, 0x663f6e91, 0x3ff00000, 0x00000000, 0x3ffa6666, 0x6666666a},
{64,0,123,__LINE__, 0x3fd13d92, 0x88e3f0dd, 0x40000000, 0x00000000, 0x3ffa6666, 0x6666666a},
{64,0,123,__LINE__, 0x3fb4260b, 0xedecb2c7, 0x40080000, 0x00000000, 0x3ffa6666, 0x6666666a},
{64,0,123,__LINE__, 0x3f9138f9, 0x5390ebd3, 0x40100000, 0x00000000, 0x3ffa6666, 0x6666666a},
{64,0,123,__LINE__, 0x3fd97895, 0x7ce7f2cf, 0x00000000, 0x00000000, 0x3ffb3333, 0x33333337},
{64,0,123,__LINE__, 0x3fe27d0d, 0x82c5db53, 0x3ff00000, 0x00000000, 0x3ffb3333, 0x33333337},
{64,0,123,__LINE__, 0x3fd20802, 0xc5da89b2, 0x40000000, 0x00000000, 0x3ffb3333, 0x33333337},
{64,0,123,__LINE__, 0x3fb5cc62, 0xb77f9eb0, 0x40080000, 0x00000000, 0x3ffb3333, 0x33333337},
{64,0,123,__LINE__, 0x3f933dbd, 0xc0e89d76, 0x40100000, 0x00000000, 0x3ffb3333, 0x33333337},
{64,0,123,__LINE__, 0x3fd79e3a, 0x9e138ae6, 0x00000000, 0x00000000, 0x3ffc0000, 0x00000004},
{64,0,123,__LINE__, 0x3fe290a3, 0xbaedcc46, 0x3ff00000, 0x00000000, 0x3ffc0000, 0x00000004},
{64,0,123,__LINE__, 0x3fd2d0f2, 0x7ae76c8a, 0x40000000, 0x00000000, 0x3ffc0000, 0x00000004},
{64,0,123,__LINE__, 0x3fb7838b, 0x1e8c5990, 0x40080000, 0x00000000, 0x3ffc0000, 0x00000004},
{64,0,123,__LINE__, 0x3f956a95, 0x623295f7, 0x40100000, 0x00000000, 0x3ffc0000, 0x00000004},
{64,0,123,__LINE__, 0x3fd5c256, 0x5d20c7de, 0x00000000, 0x00000000, 0x3ffccccc, 0xccccccd1},
{64,0,123,__LINE__, 0x3fe29bc9, 0x703828ac, 0x3ff00000, 0x00000000, 0x3ffccccc, 0xccccccd1},
{64,0,123,__LINE__, 0x3fd397db, 0x0e06aef0, 0x40000000, 0x00000000, 0x3ffccccc, 0xccccccd1},
{64,0,123,__LINE__, 0x3fb94b16, 0xc2085bbd, 0x40080000, 0x00000000, 0x3ffccccc, 0xccccccd1},
{64,0,123,__LINE__, 0x3f97c0d3, 0xe559d7ff, 0x40100000, 0x00000000, 0x3ffccccc, 0xccccccd1},
{64,0,123,__LINE__, 0x3fd3e5c0, 0xa05c4025, 0x00000000, 0x00000000, 0x3ffd9999, 0x9999999e},
{64,0,123,__LINE__, 0x3fe29e82, 0x64e59f5a, 0x3ff00000, 0x00000000, 0x3ffd9999, 0x9999999e},
{64,0,123,__LINE__, 0x3fd45c36, 0xb655f5cb, 0x40000000, 0x00000000, 0x3ffd9999, 0x9999999e},
{64,0,123,__LINE__, 0x3fbb2287, 0xc3831b75, 0x40080000, 0x00000000, 0x3ffd9999, 0x9999999e},
{64,0,123,__LINE__, 0x3f9a41bb, 0x0d6129f8, 0x40100000, 0x00000000, 0x3ffd9999, 0x9999999e},
{64,0,123,__LINE__, 0x3fd20950, 0xb5facde5, 0x00000000, 0x00000000, 0x3ffe6666, 0x6666666b},
{64,0,123,__LINE__, 0x3fe298d6, 0xb7a495db, 0x3ff00000, 0x00000000, 0x3ffe6666, 0x6666666b},
{64,0,123,__LINE__, 0x3fd51d80, 0xcca30f3c, 0x40000000, 0x00000000, 0x3ffe6666, 0x6666666b},
{64,0,123,__LINE__, 0x3fbd0950, 0xfe1d8095, 0x40080000, 0x00000000, 0x3ffe6666, 0x6666666b},
{64,0,123,__LINE__, 0x3f9cee79, 0x030dd010, 0x40100000, 0x00000000, 0x3ffe6666, 0x6666666b},
{64,0,123,__LINE__, 0x3fd02ddc, 0xe4c5e3e8, 0x00000000, 0x00000000, 0x3fff3333, 0x33333338},
{64,0,123,__LINE__, 0x3fe28ad2, 0xdcd91caf, 0x3ff00000, 0x00000000, 0x3fff3333, 0x33333338},
{64,0,123,__LINE__, 0x3fd5db36, 0x1b535d7a, 0x40000000, 0x00000000, 0x3fff3333, 0x33333338},
{64,0,123,__LINE__, 0x3fbefed6, 0x4831e4df, 0x40080000, 0x00000000, 0x3fff3333, 0x33333338},
{64,0,123,__LINE__, 0x3f9fc826, 0xafa66438, 0x40100000, 0x00000000, 0x3fff3333, 0x33333338},
0,};
test_yn(m)   {run_vector_1(m,yn_vec,(char *)(yn),"yn","did");   }	
