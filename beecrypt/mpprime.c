/*
 * Copyright (c) 2003 Bob Deblier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!\file mpprime.c
 * \brief Multi-precision primes.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#include "system.h"
#include "mpprime.h"
#include "mp.h"
#include "mpbarrett.h"
#include "debug.h"

/**
 * A word of explanation here on what these tables accomplish:
 *
 * For fast checking whether a candidate prime can be divided by small primes, we use this table,
 * which contains the products of all small primes starting at 3, up to a word size equal to the size
 * of the candidate tested.
 *
 * Instead of trying each small prime in successive divisions, we compute one gcd with a product of small
 * primes from this table.
 * If the gcd result is not 1, the candidate is divisable by at least one of the small primes(*). If the gcd
 * result is 1, then we can subject the candidate to a probabilistic test.
 *
 * (*) Note: the candidate prime could also be one of the small primes, in which is it IS prime,
 * but too small to be of cryptographic interest. Hence, use only for candidate primes that are large enough.
 */

#if (MP_WBYTES == 8)

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_01[] =	/* primes 3 to 53 */
{ 0xe221f97c30e94e1dU };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_02[] =	/* primes 3 to 101 */
{ 0x5797d47c51681549U, 0xd734e4fc4c3eaf7fU };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_03[] =	/* primes 3 to 149 */
{ 0x1e6d8e2a0ffceafbU, 0xbcbfc14a4c3bc1e1U,
  0x009c6a22a0a7adf5U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_04[] =	/* primes 3 to 193 */
{ 0xdbf05b6f5654b3c0U, 0xf524355143958688U,
  0x9f155887819aed2aU, 0xc05b93352be98677U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_05[] =	/* primes 3 to 239 */
{ 0x3faa5dadb695ce58U, 0x4a579328eab20f1fU,
  0xef00fe27ffc36456U, 0x0a65723e27d8884aU,
  0xd59da0a992f77529U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_06[] =	/* primes 3 to 281 */
{ 0x501201cc51a492a5U, 0x44d3900ad4f8b32aU,
  0x203c858406a4457cU, 0xab0b4f805ab18ac6U,
  0xeb9572ac6e9394faU, 0x522bffb6f44af2f3U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_07[] =	/* primes 3 to 331 */
{ 0x0120eb4d70279230U, 0x9ed122fce0488be4U,
  0x1d0c99f5d8c039adU, 0x058c90b4780500feU,
  0xf39c05cc09817a27U, 0xc3e1776a246b6af2U,
  0x946a10d66eafaedfU };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_08[] =	/* primes 3 to 379 */
{ 0x106aa9fb7646fa6eU, 0xb0813c28c5d5f09fU,
  0x077ec3ba238bfb99U, 0xc1b631a203e81187U,
  0x233db117cbc38405U, 0x6ef04659a4a11de4U,
  0x9f7ecb29bada8f98U, 0x0decece92e30c48fU };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_09[] =	/* primes 3 to 421 */
{ 0x0185dbeb2b8b11d3U, 0x7633e9dc1eec5415U,
  0x65c6ce8431d227eeU, 0x28f0328a60c90118U,
  0xae031cc5a781c824U, 0xd1f16d25f4f0cccfU,
  0xf35e974579072ec8U, 0xcaf1ac8eefd5566fU,
  0xa15fb94fe34f5d37U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_10[] =	/* primes 3 to 463 */
{ 0x833a505cf9922beeU, 0xc80265a6d50e1cceU,
  0xa22f6fec2eb84450U, 0xcec64a3c0e10d472U,
  0xdd653b9b51d81d0eU, 0x3a3142ea49b91e3aU,
  0x5e21023267bda426U, 0x738730cfb8e6e2aeU,
  0xc08c9d4bd2420066U, 0xdccf95ef49a560b7U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_11[] =	/* primes 3 to 509 */
{ 0x309d024bd5380319U, 0x2ca334690bafb43aU,
  0x0abd5840fbeb24d1U, 0xf49b633047902baeU,
  0x581ca4cba778fdb1U, 0x6dc0a6afef960687U,
  0x16855d9593746604U, 0x201f1919b725fcb7U,
  0x8ffd0db8e8fa61a1U, 0x6e1c0970beb81adcU,
  0xf49c82dff960d36fU };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_12[] =	/* primes 3 to 569 */
{ 0x25eac89f8d4da338U, 0x337b49850d2d1489U,
  0x2663177b4010af3dU, 0xd23eeb0b228f3832U,
  0xffcee2e5cbd1acc9U, 0x8f47f251873380aeU,
  0x10f0ffdd8e602ffaU, 0x210f41f669a1570aU,
  0x93c158c1a9a8227fU, 0xf81a90c5630e9c44U,
  0x845c755c7df35a7dU, 0x430c679a11575655U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_13[] =	/* primes 3 to 607 */
{ 0x3383219d26454f06U, 0xe2789b7f9c3b940eU,
  0x03be2105798e3ff7U, 0x945bd325997bc262U,
  0x025598f88577748eU, 0xc7155ff88a1ff4c9U,
  0x2ce95bd8b015101fU, 0x19b73b1481627f9aU,
  0x6f83da3a03259fbdU, 0x41f92a6e85ac6efaU,
  0xde195be86e66ba89U, 0xb0ab042d3276976cU,
  0x3dbeb3d7413ea96dU };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_14[] =	/* primes 3 to 647 */
{ 0x6e02645460adbd18U, 0xcd52ce1a1beab1c0U,
  0x36e468e9f350d69bU, 0x1d357d083a59f778U,
  0xc2cc262b4a29ce52U, 0x509bcf97349ba2bfU,
  0x22402d716b32517eU, 0x1941e18ace76cbd8U,
  0x5809701e70eaef96U, 0x9aac365c8a9fea5eU,
  0xc74d951db361f061U, 0xc4d14f000d806db4U,
  0xcd939110c7cab492U, 0x2f3ea4c4852ca469U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_15[] =	/* primes 3 to 683 */
{ 0x008723131f66758aU, 0x414bbebb2f8670bfU,
  0x01dc959d74468901U, 0x57c57f40e210c9c2U,
  0x74f544697c71cc1dU, 0xe2be67a203d8d56fU,
  0x6c363fca0a78676aU, 0x2b9777896ea2db50U,
  0xdb31b73751992f73U, 0x0def293ebc028877U,
  0xdf95ac1b4d0c0128U, 0x9a0b05e00e6c0bc8U,
  0xe61b766ec0943254U, 0x1cd70f0fd5a0ce6bU,
  0x8ab998fb8ab36e0dU };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_16[] =	/* primes 3 to 739 */
{ 0x02c85ff870f24be8U, 0x0f62b1ba6c20bd72U,
  0xb837efdf121206d8U, 0x7db56b7d69fa4c02U,
  0x1c107c3ca206fe8fU, 0xa7080ef576effc82U,
  0xf9b10f5750656b77U, 0x94b16afd70996e91U,
  0xaef6e0ad15e91b07U, 0x1ac9b24d98b233adU,
  0x86ee055518e58e56U, 0x638ef18bac5c74cbU,
  0x35bbb6e5dae2783dU, 0xd1c0ce7dec4fc70eU,
  0x5186d411df36368fU, 0x061aa36011f30179U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_17[] =	/* primes 3 to 787 */
{ 0x16af5c18a2bef8efU, 0xf2278332182d0fbfU,
  0x0038cc205148b83dU, 0x06e3d7d932828b18U,
  0xe11e094028c7eaedU, 0xa3395017e07d8ae9U,
  0xb594060451d05f93U, 0x084cb481663c94c6U,
  0xff980ddeccdb42adU, 0x37097f41a7837fc9U,
  0x5afe3f18ad76f234U, 0x83ae942e0f0c0bc6U,
  0xe40016123189872bU, 0xe58f6dfc239ca28fU,
  0xb0cfbf964c8f27ceU, 0x05d6c77a01f9d332U,
  0x36c9d442ad69ed33U };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_18[] =	/* primes 3 to 827 */
{ 0x005bfd2583ab7a44U, 0x13d4df0f537c686c,
  0xa8e6b583e491130eU, 0x96dfcc1c05ba298f,
  0x8701314b45bf6ff4U, 0xecf372ffe78bccdf,
  0xfc18365a6ae5ca41U, 0x2794281fbcc762f1,
  0x8ca1eb11fc8efe0bU, 0x6bb5a7a09954e758,
  0x074256ad443a8e4bU, 0xaa2675154c43d626,
  0x464119446e683d08U, 0xd4683db5757d1199,
  0x9513a9cbe3e67e3aU, 0xe501c1c522aa8ba9,
  0xf955789589161febU, 0xc69941a147aa9685 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_19[] =	/* primes 3 to 877 */
{ 0x06706918e8355b7f, 0xfd3f024da6b012e2,
  0xbb7338f30d51a968, 0x0f3d912035ed70e0,
  0x2d38d422e41812d4, 0xe29d637b318ce6f4,
  0xea117321ce8b712d, 0xcca9345fd03ccaf5,
  0x2e75dafcda909cd4, 0xb41a9f8753c8df3d,
  0x284198bcb759d059, 0x941360572b7ab25f,
  0x396b9fa37ae0a200, 0xd998ea09167edc30,
  0xf9d2c45c7e487029, 0x927500983f7fb4e8,
  0xe85d8e9434a37006, 0x8cebc96060ab2f87,
  0x81efeb182d0e724b };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_20[] =	/* primes 3 to 929 */
{ 0xa9e9591f7815617e, 0xcabe352fa13445c4,
  0xf8e319ba63042e1c, 0xb0a017d0e729a699,
  0x5480da4e5091cab4, 0x12910cf47bb0f24e,
  0x5e1db41264b9f96a, 0x2b327e901d9d0a39,
  0x12659a52d3792d52, 0x991bfa964fe7d212,
  0x60374c24a04de69d, 0xf5d4e46b249cafc7,
  0x347c6181bd6dc6b8, 0x13a29dc6d4f785ac,
  0x7806635513530cd5, 0xdb94de4858c157f0,
  0x30b96bfb6475393b, 0x5f43a549d95c5619,
  0x7e274850ad1a6d18, 0xb5eaa41dd42fda55 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_21[] =	/* primes 3 to 971 */
{ 0x06e1d136cb78cac5, 0x4da4bfcb6f2c4a24,
  0xfcf3796b77719c31, 0xd27915860001f03e,
  0x4347621bf62577e0, 0x280ebfdb77b4f1e9,
  0x0f954ecafd198609, 0x68629be91424c37a,
  0x8f320a34444953d5, 0x2c278d6485238798,
  0x709d0063e3fa8623, 0xea24bf2a2c5278e7,
  0x4460d05a0a708bd9, 0xc019d632e39e7300,
  0x22b9dbb913df73cf, 0xb959dffe348f9623,
  0xf697a822f4a11320, 0xbd044ecc74878f53,
  0x0d57d0f076647b0a, 0xb191f543dc08c392,
  0x3167e5ee56c66847 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_22[] =	/* primes 3 to 1013 */
{ 0x005ca1a92edd0e81, 0x9619289e1ecfe2d7,
  0xf3949eaf363a5fe8, 0xf6fee01ccd480490,
  0x30a1346ab83c4967, 0x8c7d58826caf81ca,
  0x1d02473bea8ad400, 0xd1ce270a5743c3cd,
  0x892c3bd93b84525d, 0x8a42071a508fdb8f,
  0x32952aaa2384cf5d, 0xf23ed81d10ac0031,
  0xd85d0e95e3c5bb51, 0x71a0e3f12b671f8f,
  0xb07965cc353a784b, 0x78f719681326c790,
  0x6e2b7f7b0782848e, 0xeb1aea5bab10b80e,
  0x5b7138fc36f7989c, 0xe85b07c2d4d59d42,
  0x1541c765f6c2111d, 0xb82eca06b437f757 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_23[] =	/* primes 3 to 1051 */
{ 0x18e5b310229f618d, 0xe0f54782f57fff33,
  0x10546ba8efc0a69c, 0xac4b573b749cc43d,
  0xd3ba4df61fe2800d, 0x733f4eb719a6ea7f,
  0xa88aebf2d35b26c8, 0x6e89fe0b27e198de,
  0xe12a14da03cef215, 0xe6651c60be9cf337,
  0x3620f4aba453eeb9, 0xeb439ba079201376,
  0x0e3cc7f8722f09a4, 0x685a5556b4efd158,
  0xb27a6b79b15f161f, 0xecf3fd802767da7a,
  0x37ceb764bebfcc2b, 0x2d833be00b21bb68,
  0xeab326b9ebb20cc2, 0xd76273edefa152ad,
  0x531bccbf17e3c78d, 0x5c43d8f6866ad640,
  0xfdbbba0fe997b27b };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_24[] =	/* primes 3 to 1093 */
{ 0x021bf9497091b8c3, 0x68cc7c8e00c1990c,
  0x6027481b79215ac8, 0xa7517749a2151377,
  0x9a993d2958fcb49a, 0x7368029268527994,
  0xc6cc1928add41295, 0x96765f4cc3141a04,
  0x4eb1d61578881667, 0x57d8618781813062,
  0x032267987df0d471, 0x9cd38f1b7085fca5,
  0x334be3a6003a3ce7, 0xe19aba553e80cc5a,
  0xe4060eff6e180666, 0x1da5eeb7d142d3b2,
  0xe40739f1443dee3a, 0x198637f03c062845,
  0xeaff3ff27ea38d93, 0x44d8a90222472df0,
  0x7dfb5c9c8ada77cd, 0x0d5b94eff021e02e,
  0x307d08010312d57c, 0xb5d975764697842d };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_25[] =	/* primes 3 to 1151 */
{ 0xfa1bd62baae1e767, 0x47535af3830fc07d,
  0xebcf3ef7e5a8e46b, 0x8937c4afe02aef0a,
  0xce420c7b2c3f2fac, 0xb9dc94e5100a7191,
  0xb47cf523520f613b, 0xee8e095a7b06d781,
  0xb6204bde1648e17f, 0x0f1bd4aba00f7e90,
  0xd8fc2a05f5f1e832, 0x6e88a4a67e73cae1,
  0xc4a93d89ad6b301b, 0x1f185b130246ab44,
  0x5cadc384931189b5, 0x566b3ed9dafba4e6,
  0x59f5446e5a70c8d1, 0x4626b66d0f1ccfbf,
  0xd4238b6884af7dd3, 0xa91d2063ceb2c2f7,
  0xf273b1da4cb542ea, 0x62c624cf4fcb0486,
  0x138b42a3c1d9593c, 0xe1254fb3214d2b08,
  0x52532bc528bc6467 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_26[] =	/* primes 3 to 1193 */
{ 0x239afcd438799705, 0xab8a0cda4802bc8f,
  0xb0e87f44a568f618, 0x7c604708dfb79072,
  0xe24b49cb8b2ac531, 0x005cf2982437b16e,
  0x027fa01414e3dbf5, 0xbf76681166e276ff,
  0xcf6768550bc1cd9a, 0x1b387ebaaa8550ae,
  0xfc10c69c372a0254, 0xb84666ff35044b9a,
  0xa34fcf7c817b33f3, 0x7088a289a17891a7,
  0xe66f88e8ec2ba784, 0xb2a09a9102609726,
  0x17a3dbea8463439d, 0x47972d09b0e63752,
  0xbac58d339b402dc1, 0xa09915543360cd68,
  0x4df24e437487571d, 0xfaf68f4fe0a93546,
  0x66aa84bf84d4448d, 0x2119029166db27bd,
  0x515599cdcd147810, 0x3acf73e7fe62aed9 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_27[] =	/* primes 3 to 1231 */
{ 0x0654f0d4cdacb307, 0x5419612fae3cf746,
  0xfbab751fd0887955, 0x28adc68d26f32877,
  0xeb1b772db48e49f6, 0xcb445987c4966560,
  0xdff8473702bb0fd4, 0xf8b68b5ce2d496a6,
  0x0dc7d7e43c3cb0bf, 0x72665c6e4c86a7ce,
  0xb78c9da40f4d90a8, 0xf5dfe2a4dc559b8a,
  0xba10a63a0ca25d3a, 0xdec2c4198b688d80,
  0x71c05d3b694f19de, 0xda32955f77fbb577,
  0x27eb652140495e56, 0x2f4a13e8b648daf2,
  0x13d1da75e3f04bb0, 0x43fedcd2b2a0cd30,
  0xa4339e3a03b7f3a0, 0xe02a31c28394368c,
  0x7f73bbf32712e69e, 0x7ac58373e5f7c7e7,
  0x55e0d645628c5475, 0x6217c0bdf119900b,
  0x05ea71dd714fd2c9 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_28[] =	/* primes 3 to 1283 */
{ 0x01662c66dab7a4fa, 0xdba4265ac2075912,
  0x59e9c885e1330cb6, 0xc91bee92f1b334ff,
  0x384f827cc8057aa7, 0xc3b65fc6de53dcac,
  0x2db6d7903febbe07, 0xcc4012326b128eb7,
  0x1afd3136a9e7f786, 0x14648da17b4f50c7,
  0xbd4129ca746dab21, 0x09583797fc1c2ecd,
  0x4c0768a81892bd16, 0xdfea8227bcb2b8bf,
  0x168a1452370b0863, 0xb299d0888434c213,
  0x2383a6c7b6b4bf20, 0x5addc8da76d2b172,
  0xb416f5b0b9a38d87, 0x738c1cca3fe33dd2,
  0xf9b7570e3f663f8b, 0x3416907651b1dd42,
  0x2192331d9436304a, 0x0303422f4d420389,
  0x4548a05562ed1c09, 0x1a63309bf1a9df8b,
  0xf0c59af912a62c22, 0xe1e1f49bb0115c17 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_29[] =	/* primes 3 to 1307 */
{ 0x005cda0c54b07f4f, 0xff0caca07cc89b95,
  0x1c021191164be693, 0x6665357ebb2f689c,
  0x7157ea4f98037ce1, 0x5aca14ca3cf1a386,
  0xb03e831ee09a8d5c, 0x48d51f5e6646ed8a,
  0x7ec2b955216587f0, 0x7f3c42ee06ae3844,
  0x4c776b8c3ef32747, 0x97cd2ac1c7cce7ec,
  0xe75bb0290f5b5a0e, 0x2c96c4600c678a21,
  0x0d992d36d441b1fd, 0x682adf0ef289947e,
  0x6d3de1a2af0ca945, 0x859aa1f2b2bb793d,
  0x351dbebfe05144ee, 0xfe9c752d75ec602c,
  0x0e0344ddcfcb642b, 0x6cfc872219d69873,
  0xb8c4ace3ffd460e9, 0x43d903b45de9d402,
  0x958a41fb5e008a94, 0xc93610814e5e2811,
  0xd052c10abfc67bf6, 0x915d44352688091b,
  0x1eb1c7117c91eae5 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_30[] =	/* primes 3 to 1381 */
{ 0xa0604bc54c251ade, 0xcf22bf075a150bb1,
  0x2a67d65a5045c183, 0x172466270d72a8c6,
  0x3e2dd1c46694a251, 0xf55bca5e7d834c87,
  0x2a8d10e5ea91ba4d, 0xcce166f16b1be0ef,
  0xba025bf362f29284, 0xa36db51675c7d25e,
  0xac7519925560c7a1, 0xc70470938bdf2818,
  0xed42d04253130bef, 0x0d92e596844e073b,
  0xdd40bd156f433f09, 0xbdfd3e38769a485c,
  0xf29380b79c18989c, 0xed0e6ec43bcc7b73,
  0x087e1fb94e8cf2d3, 0x475c77605c707f6b,
  0x31f7217c4c628da2, 0xe3263e30a83c1066,
  0x1378f41533ca7d71, 0x5d4e2b87c0e142ba,
  0x462e6ffb506e09f9, 0x7850c73e4b3f7a24,
  0xca98bda05c0c6ac6, 0x666daad014d2ff3f,
  0x7138fa68ddd5e9f0, 0xe92edcaa62b56483 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_31[] =	/* primes 3 to 1433 */
{ 0x4742fdaff7e8231a, 0xded6827758493423,
  0x12b13d2f5925c539, 0x82d876ef7ff69e7f,
  0x5b4ff04e8454faea, 0x620dc9600c65fd57,
  0x2aecce4c9656588f, 0x79dfb5dfd7f99148,
  0x196c24df6d8c704b, 0xd6ffb8d9cedb8ee8,
  0x448d4352d834cef7, 0xfce9b92907eeca6a,
  0xcc107008fa118ff7, 0xedcc0b84207c3eef,
  0xdb5ea3ef89c684d8, 0x89c4187a10775358,
  0xc429d4d2a76bb2c3, 0x9f406fdc49dcf4b6,
  0xed773586770e4651, 0xcb63c78354d2a578,
  0x5f52816b14d29d62, 0x06d952ca4428030e,
  0x2e793590f75f1d07, 0x79363fa6047f0c64,
  0xf3ed6a912dbc4437, 0x673d418400d005ca,
  0x9ca42ff6841c84dd, 0xaaff5fb087f85954,
  0x177c5dc0fbfbb491, 0xa1e5e03e5715875c,
  0xa02a0fa41fde7abd };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_32[] =	/* primes 3 to 1471 */
{ 0x2465a7bd85011e1c, 0x9e0527929fff268c,
  0x82ef7efa416863ba, 0xa5acdb0971dba0cc,
  0xac3ee4999345029f, 0x2cf810b99e406aac,
  0x5fce5dd69d1c717d, 0xaea5d18ab913f456,
  0x505679bc91c57d46, 0xd9888857862b36e2,
  0xede2e473c1f0ab35, 0x9da25271affe15ff,
  0x240e299d0b04f4cd, 0x0e4d7c0e47b1a7ba,
  0x007de89aae848fd5, 0xbdcd7f9815564eb0,
  0x60ae14f19cb50c29, 0x1f0bbd8ed1c4c7f8,
  0xfc5fba5166200193, 0x9b532d92dac844a8,
  0x431d400c832d039f, 0x5f900b278a75219c,
  0x2986140c79045d77, 0x59540854c31504dc,
  0x56f1df5eebe7bee4, 0x47658b917bf696d6,
  0x927f2e2428fbeb34, 0x0e515cb9835d6387,
  0x1be8bbe09cf13445, 0x799f2e6778815157,
  0x1a93b4c1eee55d1b, 0x9072e0b2f5c4607f };

#elif (MP_WBYTES == 4)

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_01[] =	/* primes 3 to 29 */
{ 0xc0cfd797 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_02[] =	/* primes 3 to 53 */
{ 0xe221f97c, 0x30e94e1d };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_03[] =	/* primes 3 to 73 */
{ 0x41cd66ac, 0xc237b226, 0x81a18067 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_04[] =	/* primes 3 to 101 */
{ 0x5797d47c, 0x51681549, 0xd734e4fc, 0x4c3eaf7f };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_05[] =	/* primes 3 to 113 */
{ 0x02c4b8d0, 0xd2e0d937, 0x3935200f, 0xb49be231,
  0x5ce1a307 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_06[] =	/* primes 3 to 149 */
{ 0x1e6d8e2a, 0x0ffceafb, 0xbcbfc14a, 0x4c3bc1e1,
  0x009c6a22, 0xa0a7adf5 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_07[] =	/* primes 3 to 167 */
{ 0x049265d3, 0x574cefd0, 0x4229bfd6, 0x62a4a46f,
  0x8611ed02, 0x26c655f0, 0x76ebade3 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_08[] =	/* primes 3 to 193 */
{ 0xdbf05b6f, 0x5654b3c0, 0xf5243551, 0x43958688,
  0x9f155887, 0x819aed2a, 0xc05b9335, 0x2be98677 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_09[] =	/* primes 3 to 223 */
{ 0x5e75cec8, 0xb5de5ea1, 0x5da8302a, 0x2f28b4ad,
  0x2735bdc3, 0x9344c52e, 0x67570925, 0x6feb71ef,
  0x6811d741 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_10[] =	/* primes 3 to 239 */
{ 0x3faa5dad, 0xb695ce58, 0x4a579328, 0xeab20f1f,
  0xef00fe27, 0xffc36456, 0x0a65723e, 0x27d8884a,
  0xd59da0a9, 0x92f77529 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_11[] =	/* primes 3 to 263 */
{ 0x3c9b6e49, 0xb7cf685b, 0xe7f3a239, 0xfb4084cb,
  0x166885e3, 0x9d4f65b4, 0x0bb0e51c, 0x0a5d36fe,
  0x98c32069, 0xfd5c441c, 0x6d82f115 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_12[] =	/* primes 3 to 281 */
{ 0x501201cc, 0x51a492a5, 0x44d3900a, 0xd4f8b32a,
  0x203c8584, 0x06a4457c, 0xab0b4f80, 0x5ab18ac6,
  0xeb9572ac, 0x6e9394fa, 0x522bffb6, 0xf44af2f3 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_13[] =	/* primes 3 to 311 */
{ 0x9397b5b4, 0x414dc331, 0x04561364, 0x79958cc8,
  0xfd5ea01f, 0x5d5e9f61, 0xbd0f1cb6, 0x24af7e6a,
  0x3284dbb2, 0x9857622b, 0x8be980a6, 0x5456a5c1,
  0xed928009 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_14[] =	/* primes 3 to 331 */
{ 0x0120eb4d, 0x70279230, 0x9ed122fc, 0xe0488be4,
  0x1d0c99f5, 0xd8c039ad, 0x058c90b4, 0x780500fe,
  0xf39c05cc, 0x09817a27, 0xc3e1776a, 0x246b6af2,
  0x946a10d6, 0x6eafaedf };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_15[] =	/* primes 3 to 353 */
{ 0x03c91dd1, 0x2e893191, 0x94095649, 0x874b41d6,
  0x05810c06, 0x195d70eb, 0xbd54a862, 0x50c52733,
  0x06dc6648, 0x1c251ca4, 0xa02c9a04, 0x78c96f0d,
  0x02f0db0b, 0x39d624ca, 0x0b0441c1 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_16[] =	/* primes 3 to 379 */
{ 0x106aa9fb, 0x7646fa6e, 0xb0813c28, 0xc5d5f09f,
  0x077ec3ba, 0x238bfb99, 0xc1b631a2, 0x03e81187,
  0x233db117, 0xcbc38405, 0x6ef04659, 0xa4a11de4,
  0x9f7ecb29, 0xbada8f98, 0x0decece9, 0x2e30c48f };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_17[] =	/* primes 3 to 401 */
{ 0x5aa88d8c, 0x594bb372, 0xc4bc813f, 0x4a87a266,
  0x1f984840, 0xdab15692, 0x2c2a177d, 0x95843665,
  0x6f36d41a, 0x11c35ccc, 0x2904b7e9, 0xc424eb61,
  0x3b3536a4, 0x0b2745bd, 0xadf1a6c9, 0x7b23e85a,
  0xdc6695c1 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_18[] =	/* primes 3 to 421 */
{ 0x0185dbeb, 0x2b8b11d3, 0x7633e9dc, 0x1eec5415,
  0x65c6ce84, 0x31d227ee, 0x28f0328a, 0x60c90118,
  0xae031cc5, 0xa781c824, 0xd1f16d25, 0xf4f0cccf,
  0xf35e9745, 0x79072ec8, 0xcaf1ac8e, 0xefd5566f,
  0xa15fb94f, 0xe34f5d37 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_19[] =	/* primes 3 to 443 */
{ 0x0cde6fd1, 0xcf108066, 0xcc548df9, 0x070e102c,
  0x2c651b88, 0x5f24f503, 0xaaffe276, 0xfeb57311,
  0x0c1e4592, 0xa35890d7, 0x678aaeee, 0x9f44800f,
  0xc43f999d, 0x5d06b89f, 0xcb22e533, 0x5a9287bc,
  0x6d75a3e9, 0x1e53906d, 0x413163d5 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_20[] =	/* primes 3 to 463 */
{ 0x833a505c, 0xf9922bee, 0xc80265a6, 0xd50e1cce,
  0xa22f6fec, 0x2eb84450, 0xcec64a3c, 0x0e10d472,
  0xdd653b9b, 0x51d81d0e, 0x3a3142ea, 0x49b91e3a,
  0x5e210232, 0x67bda426, 0x738730cf, 0xb8e6e2ae,
  0xc08c9d4b, 0xd2420066, 0xdccf95ef, 0x49a560b7 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_21[] =	/* primes 3 to 487 */
{ 0x035417f1, 0xe321c06c, 0xbe32ffce, 0xae752cc9,
  0xa9fe11a6, 0x3d94c946, 0x456edd7d, 0x5a060de1,
  0x84a826a6, 0xf0740c13, 0x48fa1038, 0x911d771d,
  0xb3773e87, 0x52300c29, 0xc82c3012, 0x131673bb,
  0x491cbd61, 0x55e565af, 0x4a9f4331, 0x0adbb0d7,
  0x06e86f6d };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_22[] =	/* primes 3 to 509 */
{ 0x309d024b, 0xd5380319, 0x2ca33469, 0x0bafb43a,
  0x0abd5840, 0xfbeb24d1, 0xf49b6330, 0x47902bae,
  0x581ca4cb, 0xa778fdb1, 0x6dc0a6af, 0xef960687,
  0x16855d95, 0x93746604, 0x201f1919, 0xb725fcb7,
  0x8ffd0db8, 0xe8fa61a1, 0x6e1c0970, 0xbeb81adc,
  0xf49c82df, 0xf960d36f };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_23[] =	/* primes 3 to 541 */
{ 0x01ab244a, 0x33bc047e, 0x804590b4, 0xc3207237,
  0xea503fa0, 0x7541b251, 0x57cfd03f, 0xf602c9d0,
  0x3dcd12ba, 0xa4947ae6, 0xc6ee61be, 0xedf6c716,
  0xfa45377d, 0x5b3c84fa, 0x5fb78b41, 0x395251eb,
  0xb6a5129c, 0x7699fb5c, 0xccec6d45, 0x56c9b8ea,
  0xfa05897c, 0xb8c5cf72, 0xb77603d9 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_24[] =	/* primes 3 to 569 */
{ 0x25eac89f, 0x8d4da338, 0x337b4985, 0x0d2d1489,
  0x2663177b, 0x4010af3d, 0xd23eeb0b, 0x228f3832,
  0xffcee2e5, 0xcbd1acc9, 0x8f47f251, 0x873380ae,
  0x10f0ffdd, 0x8e602ffa, 0x210f41f6, 0x69a1570a,
  0x93c158c1, 0xa9a8227f, 0xf81a90c5, 0x630e9c44,
  0x845c755c, 0x7df35a7d, 0x430c679a, 0x11575655 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_25[] =	/* primes 3 to 587 */
{ 0x01b515a8, 0xdca3d6e4, 0x69090373, 0x84febfe8,
  0xf32e06cf, 0x9bde8c89, 0x6b3f992f, 0x2ff23508,
  0xe1c01024, 0x3b8ad0c4, 0xac54e7c7, 0x3f4081d8,
  0xe495d54d, 0x74ed01e8, 0x9dfcbdde, 0x1fe7e61a,
  0x839bd902, 0xf43bf273, 0x2441f0ae, 0xb4211c70,
  0x6b3faafc, 0x0f200b35, 0x7485ce4a, 0x2f08f148,
  0xcce6887d };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_26[] =	/* primes 3 to 607 */
{ 0x3383219d, 0x26454f06, 0xe2789b7f, 0x9c3b940e,
  0x03be2105, 0x798e3ff7, 0x945bd325, 0x997bc262,
  0x025598f8, 0x8577748e, 0xc7155ff8, 0x8a1ff4c9,
  0x2ce95bd8, 0xb015101f, 0x19b73b14, 0x81627f9a,
  0x6f83da3a, 0x03259fbd, 0x41f92a6e, 0x85ac6efa,
  0xde195be8, 0x6e66ba89, 0xb0ab042d, 0x3276976c,
  0x3dbeb3d7, 0x413ea96d };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_27[] =	/* primes 3 to 619 */
{ 0x02ced4b7, 0xf15179e8, 0x7fcba6da, 0x7b07a6f3,
  0xf9311218, 0xa7b88985, 0xac74b503, 0xbf745330,
  0x6d0a23f5, 0x27a1fa9a, 0xc2b85f1a, 0x26152470,
  0x6ac242f3, 0x518cc497, 0x09a23d74, 0xff28da52,
  0xe7bbf7f7, 0xa63c1c88, 0x6f684195, 0x65e472ce,
  0x80751585, 0xc70e20c2, 0x2d15d3fe, 0xc1b40c7f,
  0x8e25dd07, 0xdb09dd86, 0x791aa9e3 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_28[] =	/* primes 3 to 647 */
{ 0x6e026454, 0x60adbd18, 0xcd52ce1a, 0x1beab1c0,
  0x36e468e9, 0xf350d69b, 0x1d357d08, 0x3a59f778,
  0xc2cc262b, 0x4a29ce52, 0x509bcf97, 0x349ba2bf,
  0x22402d71, 0x6b32517e, 0x1941e18a, 0xce76cbd8,
  0x5809701e, 0x70eaef96, 0x9aac365c, 0x8a9fea5e,
  0xc74d951d, 0xb361f061, 0xc4d14f00, 0x0d806db4,
  0xcd939110, 0xc7cab492, 0x2f3ea4c4, 0x852ca469 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_29[] =	/* primes 3 to 661 */
{ 0x074921f7, 0x6a76cec3, 0xaeb05f74, 0x60b21f16,
  0x49dece2f, 0x21bb3ed9, 0xe4cb4ebc, 0x05d6f408,
  0xed3d408a, 0xdee16505, 0xdc657c6d, 0x93877982,
  0xf2d11ce6, 0xcb5b0bb0, 0x579b3189, 0xb339c2cc,
  0xcf81d846, 0xa9fbde0c, 0x723afbc7, 0x36655d41,
  0x0018d768, 0x21779cf3, 0x52642f1b, 0x2d17165d,
  0xc7001c45, 0x4a84a45d, 0x66007591, 0x27e85693,
  0x2288d0fb };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_30[] =	/* primes 3 to 683 */
{ 0x00872313, 0x1f66758a, 0x414bbebb, 0x2f8670bf,
  0x01dc959d, 0x74468901, 0x57c57f40, 0xe210c9c2,
  0x74f54469, 0x7c71cc1d, 0xe2be67a2, 0x03d8d56f,
  0x6c363fca, 0x0a78676a, 0x2b977789, 0x6ea2db50,
  0xdb31b737, 0x51992f73, 0x0def293e, 0xbc028877,
  0xdf95ac1b, 0x4d0c0128, 0x9a0b05e0, 0x0e6c0bc8,
  0xe61b766e, 0xc0943254, 0x1cd70f0f, 0xd5a0ce6b,
  0x8ab998fb, 0x8ab36e0d };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_31[] =	/* primes 3 to 719 */
{ 0x1e595df4, 0x3064a8c9, 0xd61ae17b, 0xde1938f0,
  0x22ee6357, 0x35f4cadd, 0x3d39f473, 0xafed7df5,
  0x92ae0fd3, 0xfe910508, 0x9ad9e939, 0x988b0227,
  0x60dec749, 0xae7ee54f, 0xeb0572ac, 0x0aed266d,
  0x92daafd8, 0x6135f7a3, 0xe4e8bf05, 0x0124c928,
  0xb0d719d5, 0x2181aec8, 0x0f79820f, 0xcb158642,
  0x20969ec0, 0x1a480d31, 0x331b3252, 0x01b36fab,
  0x3d5b415b, 0x1a4567e7, 0x3baf6389 };

/**
 */
/*@observer@*/ /*@unchecked@*/
static mpw spp_32[] =	/* primes 3 to 739 */
{ 0x02c85ff8, 0x70f24be8, 0x0f62b1ba, 0x6c20bd72,
  0xb837efdf, 0x121206d8, 0x7db56b7d, 0x69fa4c02,
  0x1c107c3c, 0xa206fe8f, 0xa7080ef5, 0x76effc82,
  0xf9b10f57, 0x50656b77, 0x94b16afd, 0x70996e91,
  0xaef6e0ad, 0x15e91b07, 0x1ac9b24d, 0x98b233ad,
  0x86ee0555, 0x18e58e56, 0x638ef18b, 0xac5c74cb,
  0x35bbb6e5, 0xdae2783d, 0xd1c0ce7d, 0xec4fc70e,
  0x5186d411, 0xdf36368f, 0x061aa360, 0x11f30179 };

#else
# error
#endif

mpw* mpspprod[SMALL_PRIMES_PRODUCT_MAX] =
{
	spp_01,
	spp_02,
	spp_03,
	spp_04,
	spp_05,
	spp_06,
	spp_07,
	spp_08,
	spp_09,
	spp_10,
	spp_11,
	spp_12,
	spp_13,
	spp_14,
	spp_15,
	spp_16,
	spp_17,
	spp_18,
	spp_19,
	spp_20,
	spp_21,
	spp_22,
	spp_23,
	spp_24,
	spp_25,
	spp_26,
	spp_27,
	spp_28,
	spp_29,
	spp_30,
	spp_31,
	spp_32
};

int mpptrials(size_t bits)
{
	if (bits >= 1854)
		return 2;
	if (bits >= 1223)
		return 3;
	if (bits >= 927)
		return 4;
	if (bits >= 747)
		return 5;
	if (bits >= 627)
		return 6;
	if (bits >= 543)
		return 7;
	if (bits >= 480)
		return 8;
	if (bits >= 431)
		return 9;
	if (bits >= 393)
		return 10;
	if (bits >= 361)
		return 11;
	if (bits >= 335)
		return 12;
	if (bits >= 314)
		return 13;
	if (bits >= 295)
		return 14;
	if (bits >= 279)
		return 15;
	if (bits >= 265)
		return 16;
	if (bits >= 253)
		return 17;
	if (bits >= 242)
		return 18;
	if (bits >= 232)
		return 19;
	if (bits >= 223)
		return 20;
	if (bits >= 216)
		return 21;
	if (bits >= 209)
		return 22;
	if (bits >= 202)
		return 23;
	if (bits >= 196)
		return 24;
	if (bits >= 191)
		return 25;
	if (bits >= 186)
		return 26;
	if (bits >= 182)
		return 27;
	if (bits >= 178)
		return 28;
	if (bits >= 174)
		return 29;
	if (bits >= 170)
		return 30;
	if (bits >= 167)
		return 31;
	if (bits >= 164)
		return 32;
	if (bits >= 161)
		return 33;
	if (bits >= 160)
		return 34;
	return 35;
}

/**
 */
static void mpprndbits(mpbarrett* p, size_t msbclr, size_t lsbset, randomGeneratorContext* rc)
	/*@modifies p @*/
{
	register size_t size = p->size;

	if (p == (mpbarrett*) 0 || p->modl == (mpw*) 0)
		return;

	(void) rc->rng->next(rc->param, p->modl, size);

	if (msbclr != 0)
		p->modl[0] &= (MP_ALLMASK >> msbclr);

	p->modl[0] |= (MP_MSBMASK >> msbclr);

	if (lsbset != 0)
		p->modl[size-1] |= (MP_ALLMASK >> (MP_WBITS - lsbset));
}

/**
 * mppsppdiv_w
 *  needs workspace of (3*size) words
 */
static
int mppsppdiv_w(const mpbarrett* p, /*@out@*/ mpw* wksp)
	/*@globals mpspprod @*/
	/*@modifies wksp @*/
{
	/* small prime product trial division test */
	register size_t size = p->size;

	if (size > SMALL_PRIMES_PRODUCT_MAX)
	{
		mpsetx(size, wksp+size, SMALL_PRIMES_PRODUCT_MAX, mpspprod[SMALL_PRIMES_PRODUCT_MAX-1]);
		/*@-compdef@*/ /* LCL: wksp+size undef */
		mpgcd_w(size, p->modl, wksp+size, wksp, wksp+2*size);
		/*@=compdef@*/
	}
	else
	{
		mpgcd_w(size, p->modl, mpspprod[size-1], wksp, wksp+2*size);
	}

	return mpisone(size, wksp);
}

/**
 * mppmilrabtwo_w
 * needs workspace of (5*size+2)
 */
static
int mppmilrabtwo_w(const mpbarrett* p, int s, const mpw* rdata, const mpw* ndata, /*@out@*/ mpw* wksp)
	/*@modifies wksp @*/
{
	register size_t size = p->size;
	register int j = 0;

	mpbtwopowmod_w(p, size, rdata, wksp, wksp+size);

	while (1)
	{
		if (mpisone(size, wksp))
			return (j == 0);

		if (mpeq(size, wksp, ndata))
			return 1;

		if (++j < s)
			mpbsqrmod_w(p, size, wksp, wksp, wksp+size);
		else
			return 0;
	}
}

/**
 * mppmilraba_w
 * needs workspace of (5*size+2) words
 */
static
int mppmilraba_w(const mpbarrett* p, const mpw* adata, int s, const mpw* rdata, const mpw* ndata, /*@out@*/ mpw* wksp)
	/*@modifies wksp @*/
{
	register size_t size = p->size;
	register int j = 0;

	mpbpowmod_w(p, size, adata, size, rdata, wksp, wksp+size);

	while (1)
	{
		if (mpisone(size, wksp))
			return (j == 0);

		if (mpeq(size, wksp, ndata))
			return 1;

		if (++j < s)
			mpbsqrmod_w(p, size, wksp, wksp, wksp+size);
		else
			return 0;
	}
}

/**
 * needs workspace of (8*size+2) words
 */
int mppmilrab_w(const mpbarrett* p, randomGeneratorContext* rc, int t, mpw* wksp)
{
	/*
	 * Miller-Rabin probabilistic primality test, with modification
	 *
	 * For more information, see:
	 * "Handbook of Applied Cryptography"
	 *  Chapter 4.24
	 *
	 * Modification to the standard algorithm:
	 *  The first value of a is not obtained randomly, but set to two
	 */

	/* this routine uses (size*3) storage, and calls mpbpowmod, which needs (size*4+2) */
	/* (size) for a, (size) for r, (size) for n-1 */

	register size_t  size  = p->size;
	register mpw* ndata = wksp;
	register mpw* rdata = ndata+size;
	register mpw* adata = rdata+size;

	int s;

	mpcopy(size, ndata, p->modl);
	(void) mpsubw(size, ndata, 1);
	mpcopy(size, rdata, ndata);

	s = mprshiftlsz(size, rdata); /* we've split p-1 into (2^s)*r */

	/* should do an assert that s != 0 */

	/* do at least one test, with a = 2 */
	if (t == 0)
		t++;

	if (!mppmilrabtwo_w(p, s, rdata, ndata, wksp+3*size))
		return 0;

	while (t-- > 0)
	{
		/* generate a random 'a' into b->data */
		mpbrnd_w(p, rc, adata, wksp);

		if (!mppmilraba_w(p, adata, s, rdata, ndata, wksp+3*size))
			return 0;
	}

    return 1;
}

/**
 * needs workspace of (7*size+2) words
 */
void mpprnd_w(mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, const mpnumber* f, mpw* wksp)
{
	/*
	 * Generate a prime into p with (size*32) bits
	 *
	 * Conditions: size(f) <= size(p)
	 *
	 * Optional input f: if f is not null, then search p so that GCD(p-1,f) = 1
	 */

	size_t size = MP_BITS_TO_WORDS(bits + MP_WBITS - 1);

	mpbinit(p, size);

	if (p->modl != (mpw*) 0)
	{
		while (1)
		{
			/*
			 * Generate a random appropriate candidate prime, and test
			 * it with small prime divisor test BEFORE computing mu
			 */
			mpprndbits(p, MP_WORDS_TO_BITS(size) - bits, 1, rc);

			/* do a small prime product trial division test on p */
			if (!mppsppdiv_w(p, wksp))
				continue;

			/* if we have an f, do the congruence test */
			if (f != (mpnumber*) 0)
			{
				mpcopy(size, wksp, p->modl);
				(void) mpsubw(size, wksp, 1);
				mpsetx(size, wksp+size, f->size, f->data);
				mpgcd_w(size, wksp, wksp+size, wksp+2*size, wksp+3*size);

				if (!mpisone(size, wksp+2*size))
					continue;
			}

			/* candidate has passed so far, now we do the probabilistic test */
			mpbmu_w(p, wksp);

			if (mppmilrab_w(p, rc, t, wksp))
				return;
		}
	}
}

/**
 * needs workspace of (7*size+2) words
 */
void mpprndconone_w(mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, const mpbarrett* q, const mpnumber* f, mpnumber* r, int cofactor, mpw* wksp)
{
	/*
	 * Generate a prime p with n bits such that p mod q = 1, and p = qr+1 where r = 2s
	 *
	 * Conditions: q > 2 and size(q) < size(p) and size(f) <= size(p)
	 *
	 * Conditions: r must be chosen so that r is even, otherwise p will be even!
	 *
	 * if cofactor == 0, then s will be chosen randomly
	 * if cofactor == 1, then make sure that q does not divide r, i.e.:
	 *    q cannot be equal to r, since r is even, and q > 2; hence if q <= r make sure that GCD(q,r) == 1
	 * if cofactor == 2, then make sure that s is prime
	 * 
	 * Optional input f: if f is not null, then search p so that GCD(p-1,f) = 1
	 */

	mpbinit(p, MP_BITS_TO_WORDS(bits + MP_WBITS - 1));

	if (p->modl != (mpw*) 0)
	{
		size_t sbits = bits - mpbits(q->size, q->modl) - 1;
		mpbarrett s;

		mpbzero(&s);
		mpbinit(&s, MP_BITS_TO_WORDS(sbits + MP_WBITS - 1));

		while (1)
		{
			mpprndbits(&s, MP_WORDS_TO_BITS(s.size) - sbits, 0, rc);

			/*@-usedef@*/ /* s is set */
			if (cofactor == 1)
			{
				mpsetlsb(s.size, s.modl);

				/* if (q <= s) check if GCD(q,s) != 1 */
				if (mplex(q->size, q->modl, s.size, s.modl))
				{
					/* we can find adequate storage for computing the gcd in s->wksp */
					mpsetx(s.size, wksp, q->size, q->modl);
					mpgcd_w(s.size, s.modl, wksp, wksp+s.size, wksp+2*s.size);

					if (!mpisone(s.size, wksp+s.size))
						continue;
				}
			}
			else if (cofactor == 2)
			{
				mpsetlsb(s.size, s.modl);
			}
			else
				{};

			if (cofactor == 2)
			{
				/* do a small prime product trial division test on r */
				if (!mppsppdiv_w(&s, wksp))
					continue;
			}

			/* multiply q*s */
			mpmul(wksp, s.size, s.modl, q->size, q->modl);
			/* s.size + q.size may be greater than p.size by 1, but the product will fit exactly into p */
			mpsetx(p->size, p->modl, s.size+q->size, wksp);
			/* multiply by two and add 1 */
			(void) mpmultwo(p->size, p->modl);
			(void) mpaddw(p->size, p->modl, 1);
			/* test if the product actually contains enough bits */
			if (mpbits(p->size, p->modl) < bits)
				continue;

			/* do a small prime product trial division test on p */
			if (!mppsppdiv_w(p, wksp))
				continue;

			/* if we have an f, do the congruence test */
			if (f != (mpnumber*) 0)
			{
				mpcopy(p->size, wksp, p->modl);
				(void) mpsubw(p->size, wksp, 1);
				mpsetx(p->size, wksp, f->size, f->data);
				mpgcd_w(p->size, wksp, wksp+p->size, wksp+2*p->size, wksp+3*p->size);
				if (!mpisone(p->size, wksp+2*p->size))
					continue;
			}

			/* if cofactor is two, test if s is prime */
			if (cofactor == 2)
			{
				mpbmu_w(&s, wksp);

				if (!mppmilrab_w(&s, rc, mpptrials(sbits), wksp))
					continue;
			}

			/* candidate has passed so far, now we do the probabilistic test on p */
			mpbmu_w(p, wksp);

			if (!mppmilrab_w(p, rc, t, wksp))
				continue;

			mpnset(r, s.size, s.modl);
			(void) mpmultwo(r->size, r->data);
			mpbfree(&s);

			return;
			/*@=usedef@*/
		}
	}
}

void mpprndsafe_w(mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, mpw* wksp)
{
	/*
	 * Initialize with a probable safe prime of 'size' words, with probability factor t
	 *
	 * A safe prime p has the property that p = 2q+1, where q is also prime
	 * Use for ElGamal type schemes, where a generator of order (p-1) is required
	 */
	size_t size = MP_BITS_TO_WORDS(bits + MP_WBITS - 1);

	mpbinit(p, size);

	if (p->modl != (mpw*) 0)
	{
		mpbarrett q;

		mpbzero(&q);
		mpbinit(&q, size);

		/*@-usedef@*/	/* q is set */
		while (1)
		{
			/*
			 * Generate a random appropriate candidate prime, and test
			 * it with small prime divisor test BEFORE computing mu
			 */

			mpprndbits(p, 0, 2, rc);

			mpcopy(size, q.modl, p->modl);
			mpdivtwo(size, q.modl);

			/* do a small prime product trial division on q */
			if (!mppsppdiv_w(&q, wksp))
				continue;

			/* do a small prime product trial division on p */
			if (!mppsppdiv_w(p, wksp))
				continue;

			/* candidate prime has passed small prime division test for p and q */
			mpbmu_w(&q, wksp);

			if (!mppmilrab_w(&q, rc, t, wksp))
				continue;

			mpbmu_w(p, wksp);

			if (!mppmilrab_w(p, rc, t, wksp))
				continue;

			mpbfree(&q);

			return;
		}
		/*@=usedef@*/
	}
}
