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

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/mpprime.h"

/*
 * A word of explanation here on what this table accomplishes:
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

#if (MP_WBITS == 64)

static mpw spp_01[] =	/* primes 3 to 53 */
{ 0xe221f97c30e94e1dU };

static mpw spp_02[] =	/* primes 3 to 101 */
{ 0x5797d47c51681549U, 0xd734e4fc4c3eaf7fU };

static mpw spp_03[] =	/* primes 3 to 149 */
{ 0x1e6d8e2a0ffceafbU, 0xbcbfc14a4c3bc1e1U,
  0x009c6a22a0a7adf5U };

static mpw spp_04[] =	/* primes 3 to 193 */
{ 0xdbf05b6f5654b3c0U, 0xf524355143958688U,
  0x9f155887819aed2aU, 0xc05b93352be98677U };

static mpw spp_05[] =	/* primes 3 to 239 */
{ 0x3faa5dadb695ce58U, 0x4a579328eab20f1fU,
  0xef00fe27ffc36456U, 0x0a65723e27d8884aU,
  0xd59da0a992f77529U };

static mpw spp_06[] =	/* primes 3 to 281 */
{ 0x501201cc51a492a5U, 0x44d3900ad4f8b32aU,
  0x203c858406a4457cU, 0xab0b4f805ab18ac6U,
  0xeb9572ac6e9394faU, 0x522bffb6f44af2f3U };

static mpw spp_07[] =	/* primes 3 to 331 */
{ 0x0120eb4d70279230U, 0x9ed122fce0488be4U,
  0x1d0c99f5d8c039adU, 0x058c90b4780500feU,
  0xf39c05cc09817a27U, 0xc3e1776a246b6af2U,
  0x946a10d66eafaedfU };

static mpw spp_08[] =	/* primes 3 to 379 */
{ 0x106aa9fb7646fa6eU, 0xb0813c28c5d5f09fU,
  0x077ec3ba238bfb99U, 0xc1b631a203e81187U,
  0x233db117cbc38405U, 0x6ef04659a4a11de4U,
  0x9f7ecb29bada8f98U, 0x0decece92e30c48fU };

static mpw spp_09[] =	/* primes 3 to 421 */
{ 0x0185dbeb2b8b11d3U, 0x7633e9dc1eec5415U,
  0x65c6ce8431d227eeU, 0x28f0328a60c90118U,
  0xae031cc5a781c824U, 0xd1f16d25f4f0cccfU,
  0xf35e974579072ec8U, 0xcaf1ac8eefd5566fU,
  0xa15fb94fe34f5d37U };

static mpw spp_10[] =	/* primes 3 to 463 */
{ 0x833a505cf9922beeU, 0xc80265a6d50e1cceU,
  0xa22f6fec2eb84450U, 0xcec64a3c0e10d472U,
  0xdd653b9b51d81d0eU, 0x3a3142ea49b91e3aU,
  0x5e21023267bda426U, 0x738730cfb8e6e2aeU,
  0xc08c9d4bd2420066U, 0xdccf95ef49a560b7U };

static mpw spp_11[] =	/* primes 3 to 509 */
{ 0x309d024bd5380319U, 0x2ca334690bafb43aU,
  0x0abd5840fbeb24d1U, 0xf49b633047902baeU,
  0x581ca4cba778fdb1U, 0x6dc0a6afef960687U,
  0x16855d9593746604U, 0x201f1919b725fcb7U,
  0x8ffd0db8e8fa61a1U, 0x6e1c0970beb81adcU,
  0xf49c82dff960d36fU };

static mpw spp_12[] =	/* primes 3 to 569 */
{ 0x25eac89f8d4da338U, 0x337b49850d2d1489U,
  0x2663177b4010af3dU, 0xd23eeb0b228f3832U,
  0xffcee2e5cbd1acc9U, 0x8f47f251873380aeU,
  0x10f0ffdd8e602ffaU, 0x210f41f669a1570aU,
  0x93c158c1a9a8227fU, 0xf81a90c5630e9c44U,
  0x845c755c7df35a7dU, 0x430c679a11575655U };

static mpw spp_13[] =	/* primes 3 to 607 */
{ 0x3383219d26454f06U, 0xe2789b7f9c3b940eU,
  0x03be2105798e3ff7U, 0x945bd325997bc262U,
  0x025598f88577748eU, 0xc7155ff88a1ff4c9U,
  0x2ce95bd8b015101fU, 0x19b73b1481627f9aU,
  0x6f83da3a03259fbdU, 0x41f92a6e85ac6efaU,
  0xde195be86e66ba89U, 0xb0ab042d3276976cU,
  0x3dbeb3d7413ea96dU };

static mpw spp_14[] =	/* primes 3 to 647 */
{ 0x6e02645460adbd18U, 0xcd52ce1a1beab1c0U,
  0x36e468e9f350d69bU, 0x1d357d083a59f778U,
  0xc2cc262b4a29ce52U, 0x509bcf97349ba2bfU,
  0x22402d716b32517eU, 0x1941e18ace76cbd8U,
  0x5809701e70eaef96U, 0x9aac365c8a9fea5eU,
  0xc74d951db361f061U, 0xc4d14f000d806db4U,
  0xcd939110c7cab492U, 0x2f3ea4c4852ca469U };

static mpw spp_15[] =	/* primes 3 to 683 */
{ 0x008723131f66758aU, 0x414bbebb2f8670bfU,
  0x01dc959d74468901U, 0x57c57f40e210c9c2U,
  0x74f544697c71cc1dU, 0xe2be67a203d8d56fU,
  0x6c363fca0a78676aU, 0x2b9777896ea2db50U,
  0xdb31b73751992f73U, 0x0def293ebc028877U,
  0xdf95ac1b4d0c0128U, 0x9a0b05e00e6c0bc8U,
  0xe61b766ec0943254U, 0x1cd70f0fd5a0ce6bU,
  0x8ab998fb8ab36e0dU };

static mpw spp_16[] =	/* primes 3 to 739 */
{ 0x02c85ff870f24be8U, 0x0f62b1ba6c20bd72U,
  0xb837efdf121206d8U, 0x7db56b7d69fa4c02U,
  0x1c107c3ca206fe8fU, 0xa7080ef576effc82U,
  0xf9b10f5750656b77U, 0x94b16afd70996e91U,
  0xaef6e0ad15e91b07U, 0x1ac9b24d98b233adU,
  0x86ee055518e58e56U, 0x638ef18bac5c74cbU,
  0x35bbb6e5dae2783dU, 0xd1c0ce7dec4fc70eU,
  0x5186d411df36368fU, 0x061aa36011f30179U };

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

static mpw spp_18[] =	/* primes 3 to 827 */
{ 0x005bfd2583ab7a44U, 0x13d4df0f537c686cU,
  0xa8e6b583e491130eU, 0x96dfcc1c05ba298fU,
  0x8701314b45bf6ff4U, 0xecf372ffe78bccdfU,
  0xfc18365a6ae5ca41U, 0x2794281fbcc762f1U,
  0x8ca1eb11fc8efe0bU, 0x6bb5a7a09954e758U,
  0x074256ad443a8e4bU, 0xaa2675154c43d626U,
  0x464119446e683d08U, 0xd4683db5757d1199U,
  0x9513a9cbe3e67e3aU, 0xe501c1c522aa8ba9U,
  0xf955789589161febU, 0xc69941a147aa9685U };

static mpw spp_19[] =	/* primes 3 to 877 */
{ 0x06706918e8355b7fU, 0xfd3f024da6b012e2U,
  0xbb7338f30d51a968U, 0x0f3d912035ed70e0U,
  0x2d38d422e41812d4U, 0xe29d637b318ce6f4U,
  0xea117321ce8b712dU, 0xcca9345fd03ccaf5U,
  0x2e75dafcda909cd4U, 0xb41a9f8753c8df3dU,
  0x284198bcb759d059U, 0x941360572b7ab25fU,
  0x396b9fa37ae0a200U, 0xd998ea09167edc30U,
  0xf9d2c45c7e487029U, 0x927500983f7fb4e8U,
  0xe85d8e9434a37006U, 0x8cebc96060ab2f87U,
  0x81efeb182d0e724bU };

static mpw spp_20[] =	/* primes 3 to 929 */
{ 0xa9e9591f7815617eU, 0xcabe352fa13445c4U,
  0xf8e319ba63042e1cU, 0xb0a017d0e729a699U,
  0x5480da4e5091cab4U, 0x12910cf47bb0f24eU,
  0x5e1db41264b9f96aU, 0x2b327e901d9d0a39U,
  0x12659a52d3792d52U, 0x991bfa964fe7d212U,
  0x60374c24a04de69dU, 0xf5d4e46b249cafc7U,
  0x347c6181bd6dc6b8U, 0x13a29dc6d4f785acU,
  0x7806635513530cd5U, 0xdb94de4858c157f0U,
  0x30b96bfb6475393bU, 0x5f43a549d95c5619U,
  0x7e274850ad1a6d18U, 0xb5eaa41dd42fda55U };

static mpw spp_21[] =	/* primes 3 to 971 */
{ 0x06e1d136cb78cac5U, 0x4da4bfcb6f2c4a24U,
  0xfcf3796b77719c31U, 0xd27915860001f03eU,
  0x4347621bf62577e0U, 0x280ebfdb77b4f1e9U,
  0x0f954ecafd198609U, 0x68629be91424c37aU,
  0x8f320a34444953d5U, 0x2c278d6485238798U,
  0x709d0063e3fa8623U, 0xea24bf2a2c5278e7U,
  0x4460d05a0a708bd9U, 0xc019d632e39e7300U,
  0x22b9dbb913df73cfU, 0xb959dffe348f9623U,
  0xf697a822f4a11320U, 0xbd044ecc74878f53U,
  0x0d57d0f076647b0aU, 0xb191f543dc08c392U,
  0x3167e5ee56c66847U };

static mpw spp_22[] =	/* primes 3 to 1013 */
{ 0x005ca1a92edd0e81U, 0x9619289e1ecfe2d7U,
  0xf3949eaf363a5fe8U, 0xf6fee01ccd480490U,
  0x30a1346ab83c4967U, 0x8c7d58826caf81caU,
  0x1d02473bea8ad400U, 0xd1ce270a5743c3cdU,
  0x892c3bd93b84525dU, 0x8a42071a508fdb8fU,
  0x32952aaa2384cf5dU, 0xf23ed81d10ac0031U,
  0xd85d0e95e3c5bb51U, 0x71a0e3f12b671f8fU,
  0xb07965cc353a784bU, 0x78f719681326c790U,
  0x6e2b7f7b0782848eU, 0xeb1aea5bab10b80eU,
  0x5b7138fc36f7989cU, 0xe85b07c2d4d59d42U,
  0x1541c765f6c2111dU, 0xb82eca06b437f757U };

static mpw spp_23[] =	/* primes 3 to 1051 */
{ 0x18e5b310229f618dU, 0xe0f54782f57fff33U,
  0x10546ba8efc0a69cU, 0xac4b573b749cc43dU,
  0xd3ba4df61fe2800dU, 0x733f4eb719a6ea7fU,
  0xa88aebf2d35b26c8U, 0x6e89fe0b27e198deU,
  0xe12a14da03cef215U, 0xe6651c60be9cf337U,
  0x3620f4aba453eeb9U, 0xeb439ba079201376U,
  0x0e3cc7f8722f09a4U, 0x685a5556b4efd158U,
  0xb27a6b79b15f161fU, 0xecf3fd802767da7aU,
  0x37ceb764bebfcc2bU, 0x2d833be00b21bb68U,
  0xeab326b9ebb20cc2U, 0xd76273edefa152adU,
  0x531bccbf17e3c78dU, 0x5c43d8f6866ad640U,
  0xfdbbba0fe997b27bU };

static mpw spp_24[] =	/* primes 3 to 1093 */
{ 0x021bf9497091b8c3U, 0x68cc7c8e00c1990cU,
  0x6027481b79215ac8U, 0xa7517749a2151377U,
  0x9a993d2958fcb49aU, 0x7368029268527994U,
  0xc6cc1928add41295U, 0x96765f4cc3141a04U,
  0x4eb1d61578881667U, 0x57d8618781813062U,
  0x032267987df0d471U, 0x9cd38f1b7085fca5U,
  0x334be3a6003a3ce7U, 0xe19aba553e80cc5aU,
  0xe4060eff6e180666U, 0x1da5eeb7d142d3b2U,
  0xe40739f1443dee3aU, 0x198637f03c062845U,
  0xeaff3ff27ea38d93U, 0x44d8a90222472df0U,
  0x7dfb5c9c8ada77cdU, 0x0d5b94eff021e02eU,
  0x307d08010312d57cU, 0xb5d975764697842dU };

static mpw spp_25[] =	/* primes 3 to 1151 */
{ 0xfa1bd62baae1e767U, 0x47535af3830fc07dU,
  0xebcf3ef7e5a8e46bU, 0x8937c4afe02aef0aU,
  0xce420c7b2c3f2facU, 0xb9dc94e5100a7191U,
  0xb47cf523520f613bU, 0xee8e095a7b06d781U,
  0xb6204bde1648e17fU, 0x0f1bd4aba00f7e90U,
  0xd8fc2a05f5f1e832U, 0x6e88a4a67e73cae1U,
  0xc4a93d89ad6b301bU, 0x1f185b130246ab44U,
  0x5cadc384931189b5U, 0x566b3ed9dafba4e6U,
  0x59f5446e5a70c8d1U, 0x4626b66d0f1ccfbfU,
  0xd4238b6884af7dd3U, 0xa91d2063ceb2c2f7U,
  0xf273b1da4cb542eaU, 0x62c624cf4fcb0486U,
  0x138b42a3c1d9593cU, 0xe1254fb3214d2b08U,
  0x52532bc528bc6467U };

static mpw spp_26[] =	/* primes 3 to 1193 */
{ 0x239afcd438799705U, 0xab8a0cda4802bc8fU,
  0xb0e87f44a568f618U, 0x7c604708dfb79072U,
  0xe24b49cb8b2ac531U, 0x005cf2982437b16eU,
  0x027fa01414e3dbf5U, 0xbf76681166e276ffU,
  0xcf6768550bc1cd9aU, 0x1b387ebaaa8550aeU,
  0xfc10c69c372a0254U, 0xb84666ff35044b9aU,
  0xa34fcf7c817b33f3U, 0x7088a289a17891a7U,
  0xe66f88e8ec2ba784U, 0xb2a09a9102609726U,
  0x17a3dbea8463439dU, 0x47972d09b0e63752U,
  0xbac58d339b402dc1U, 0xa09915543360cd68U,
  0x4df24e437487571dU, 0xfaf68f4fe0a93546U,
  0x66aa84bf84d4448dU, 0x2119029166db27bdU,
  0x515599cdcd147810U, 0x3acf73e7fe62aed9U };

static mpw spp_27[] =	/* primes 3 to 1231 */
{ 0x0654f0d4cdacb307U, 0x5419612fae3cf746U,
  0xfbab751fd0887955U, 0x28adc68d26f32877U,
  0xeb1b772db48e49f6U, 0xcb445987c4966560U,
  0xdff8473702bb0fd4U, 0xf8b68b5ce2d496a6U,
  0x0dc7d7e43c3cb0bfU, 0x72665c6e4c86a7ceU,
  0xb78c9da40f4d90a8U, 0xf5dfe2a4dc559b8aU,
  0xba10a63a0ca25d3aU, 0xdec2c4198b688d80U,
  0x71c05d3b694f19deU, 0xda32955f77fbb577U,
  0x27eb652140495e56U, 0x2f4a13e8b648daf2U,
  0x13d1da75e3f04bb0U, 0x43fedcd2b2a0cd30U,
  0xa4339e3a03b7f3a0U, 0xe02a31c28394368cU,
  0x7f73bbf32712e69eU, 0x7ac58373e5f7c7e7U,
  0x55e0d645628c5475U, 0x6217c0bdf119900bU,
  0x05ea71dd714fd2c9U };

static mpw spp_28[] =	/* primes 3 to 1283 */
{ 0x01662c66dab7a4faU, 0xdba4265ac2075912U,
  0x59e9c885e1330cb6U, 0xc91bee92f1b334ffU,
  0x384f827cc8057aa7U, 0xc3b65fc6de53dcacU,
  0x2db6d7903febbe07U, 0xcc4012326b128eb7U,
  0x1afd3136a9e7f786U, 0x14648da17b4f50c7U,
  0xbd4129ca746dab21U, 0x09583797fc1c2ecdU,
  0x4c0768a81892bd16U, 0xdfea8227bcb2b8bfU,
  0x168a1452370b0863U, 0xb299d0888434c213U,
  0x2383a6c7b6b4bf20U, 0x5addc8da76d2b172U,
  0xb416f5b0b9a38d87U, 0x738c1cca3fe33dd2U,
  0xf9b7570e3f663f8bU, 0x3416907651b1dd42U,
  0x2192331d9436304aU, 0x0303422f4d420389U,
  0x4548a05562ed1c09U, 0x1a63309bf1a9df8bU,
  0xf0c59af912a62c22U, 0xe1e1f49bb0115c17U };

static mpw spp_29[] =	/* primes 3 to 1307 */
{ 0x005cda0c54b07f4fU, 0xff0caca07cc89b95U,
  0x1c021191164be693U, 0x6665357ebb2f689cU,
  0x7157ea4f98037ce1U, 0x5aca14ca3cf1a386U,
  0xb03e831ee09a8d5cU, 0x48d51f5e6646ed8aU,
  0x7ec2b955216587f0U, 0x7f3c42ee06ae3844U,
  0x4c776b8c3ef32747U, 0x97cd2ac1c7cce7ecU,
  0xe75bb0290f5b5a0eU, 0x2c96c4600c678a21U,
  0x0d992d36d441b1fdU, 0x682adf0ef289947eU,
  0x6d3de1a2af0ca945U, 0x859aa1f2b2bb793dU,
  0x351dbebfe05144eeU, 0xfe9c752d75ec602cU,
  0x0e0344ddcfcb642bU, 0x6cfc872219d69873U,
  0xb8c4ace3ffd460e9U, 0x43d903b45de9d402U,
  0x958a41fb5e008a94U, 0xc93610814e5e2811U,
  0xd052c10abfc67bf6U, 0x915d44352688091bU,
  0x1eb1c7117c91eae5U };

static mpw spp_30[] =	/* primes 3 to 1381 */
{ 0xa0604bc54c251adeU, 0xcf22bf075a150bb1U,
  0x2a67d65a5045c183U, 0x172466270d72a8c6U,
  0x3e2dd1c46694a251U, 0xf55bca5e7d834c87U,
  0x2a8d10e5ea91ba4dU, 0xcce166f16b1be0efU,
  0xba025bf362f29284U, 0xa36db51675c7d25eU,
  0xac7519925560c7a1U, 0xc70470938bdf2818U,
  0xed42d04253130befU, 0x0d92e596844e073bU,
  0xdd40bd156f433f09U, 0xbdfd3e38769a485cU,
  0xf29380b79c18989cU, 0xed0e6ec43bcc7b73U,
  0x087e1fb94e8cf2d3U, 0x475c77605c707f6bU,
  0x31f7217c4c628da2U, 0xe3263e30a83c1066U,
  0x1378f41533ca7d71U, 0x5d4e2b87c0e142baU,
  0x462e6ffb506e09f9U, 0x7850c73e4b3f7a24U,
  0xca98bda05c0c6ac6U, 0x666daad014d2ff3fU,
  0x7138fa68ddd5e9f0U, 0xe92edcaa62b56483U };

static mpw spp_31[] =	/* primes 3 to 1433 */
{ 0x4742fdaff7e8231aU, 0xded6827758493423U,
  0x12b13d2f5925c539U, 0x82d876ef7ff69e7fU,
  0x5b4ff04e8454faeaU, 0x620dc9600c65fd57U,
  0x2aecce4c9656588fU, 0x79dfb5dfd7f99148U,
  0x196c24df6d8c704bU, 0xd6ffb8d9cedb8ee8U,
  0x448d4352d834cef7U, 0xfce9b92907eeca6aU,
  0xcc107008fa118ff7U, 0xedcc0b84207c3eefU,
  0xdb5ea3ef89c684d8U, 0x89c4187a10775358U,
  0xc429d4d2a76bb2c3U, 0x9f406fdc49dcf4b6U,
  0xed773586770e4651U, 0xcb63c78354d2a578U,
  0x5f52816b14d29d62U, 0x06d952ca4428030eU,
  0x2e793590f75f1d07U, 0x79363fa6047f0c64U,
  0xf3ed6a912dbc4437U, 0x673d418400d005caU,
  0x9ca42ff6841c84ddU, 0xaaff5fb087f85954U,
  0x177c5dc0fbfbb491U, 0xa1e5e03e5715875cU,
  0xa02a0fa41fde7abdU };

static mpw spp_32[] =	/* primes 3 to 1471 */
{ 0x2465a7bd85011e1cU, 0x9e0527929fff268cU,
  0x82ef7efa416863baU, 0xa5acdb0971dba0ccU,
  0xac3ee4999345029fU, 0x2cf810b99e406aacU,
  0x5fce5dd69d1c717dU, 0xaea5d18ab913f456U,
  0x505679bc91c57d46U, 0xd9888857862b36e2U,
  0xede2e473c1f0ab35U, 0x9da25271affe15ffU,
  0x240e299d0b04f4cdU, 0x0e4d7c0e47b1a7baU,
  0x007de89aae848fd5U, 0xbdcd7f9815564eb0U,
  0x60ae14f19cb50c29U, 0x1f0bbd8ed1c4c7f8U,
  0xfc5fba5166200193U, 0x9b532d92dac844a8U,
  0x431d400c832d039fU, 0x5f900b278a75219cU,
  0x2986140c79045d77U, 0x59540854c31504dcU,
  0x56f1df5eebe7bee4U, 0x47658b917bf696d6U,
  0x927f2e2428fbeb34U, 0x0e515cb9835d6387U,
  0x1be8bbe09cf13445U, 0x799f2e6778815157U,
  0x1a93b4c1eee55d1bU, 0x9072e0b2f5c4607fU };

#elif (MP_WBITS == 32)

static mpw spp_01[] =	/* primes 3 to 29 */
{ 0xc0cfd797U };

static mpw spp_02[] =	/* primes 3 to 53 */
{ 0xe221f97cU, 0x30e94e1dU };

static mpw spp_03[] =	/* primes 3 to 73 */
{ 0x41cd66acU, 0xc237b226U, 0x81a18067U };

static mpw spp_04[] =	/* primes 3 to 101 */
{ 0x5797d47cU, 0x51681549U, 0xd734e4fcU, 0x4c3eaf7fU };

static mpw spp_05[] =	/* primes 3 to 113 */
{ 0x02c4b8d0U, 0xd2e0d937U, 0x3935200fU, 0xb49be231U,
  0x5ce1a307U };

static mpw spp_06[] =	/* primes 3 to 149 */
{ 0x1e6d8e2aU, 0x0ffceafbU, 0xbcbfc14aU, 0x4c3bc1e1U,
  0x009c6a22U, 0xa0a7adf5U };

static mpw spp_07[] =	/* primes 3 to 167 */
{ 0x049265d3U, 0x574cefd0U, 0x4229bfd6U, 0x62a4a46fU,
  0x8611ed02U, 0x26c655f0U, 0x76ebade3U };

static mpw spp_08[] =	/* primes 3 to 193 */
{ 0xdbf05b6fU, 0x5654b3c0U, 0xf5243551U, 0x43958688U,
  0x9f155887U, 0x819aed2aU, 0xc05b9335U, 0x2be98677U };

static mpw spp_09[] =	/* primes 3 to 223 */
{ 0x5e75cec8U, 0xb5de5ea1U, 0x5da8302aU, 0x2f28b4adU,
  0x2735bdc3U, 0x9344c52eU, 0x67570925U, 0x6feb71efU,
  0x6811d741U };

static mpw spp_10[] =	/* primes 3 to 239 */
{ 0x3faa5dadU, 0xb695ce58U, 0x4a579328U, 0xeab20f1fU,
  0xef00fe27U, 0xffc36456U, 0x0a65723eU, 0x27d8884aU,
  0xd59da0a9U, 0x92f77529U };

static mpw spp_11[] =	/* primes 3 to 263 */
{ 0x3c9b6e49U, 0xb7cf685bU, 0xe7f3a239U, 0xfb4084cbU,
  0x166885e3U, 0x9d4f65b4U, 0x0bb0e51cU, 0x0a5d36feU,
  0x98c32069U, 0xfd5c441cU, 0x6d82f115U };

static mpw spp_12[] =	/* primes 3 to 281 */
{ 0x501201ccU, 0x51a492a5U, 0x44d3900aU, 0xd4f8b32aU,
  0x203c8584U, 0x06a4457cU, 0xab0b4f80U, 0x5ab18ac6U,
  0xeb9572acU, 0x6e9394faU, 0x522bffb6U, 0xf44af2f3U };

static mpw spp_13[] =	/* primes 3 to 311 */
{ 0x9397b5b4U, 0x414dc331U, 0x04561364U, 0x79958cc8U,
  0xfd5ea01fU, 0x5d5e9f61U, 0xbd0f1cb6U, 0x24af7e6aU,
  0x3284dbb2U, 0x9857622bU, 0x8be980a6U, 0x5456a5c1U,
  0xed928009U };

static mpw spp_14[] =	/* primes 3 to 331 */
{ 0x0120eb4dU, 0x70279230U, 0x9ed122fcU, 0xe0488be4U,
  0x1d0c99f5U, 0xd8c039adU, 0x058c90b4U, 0x780500feU,
  0xf39c05ccU, 0x09817a27U, 0xc3e1776aU, 0x246b6af2U,
  0x946a10d6U, 0x6eafaedfU };

static mpw spp_15[] =	/* primes 3 to 353 */
{ 0x03c91dd1U, 0x2e893191U, 0x94095649U, 0x874b41d6U,
  0x05810c06U, 0x195d70ebU, 0xbd54a862U, 0x50c52733U,
  0x06dc6648U, 0x1c251ca4U, 0xa02c9a04U, 0x78c96f0dU,
  0x02f0db0bU, 0x39d624caU, 0x0b0441c1U };

static mpw spp_16[] =	/* primes 3 to 379 */
{ 0x106aa9fbU, 0x7646fa6eU, 0xb0813c28U, 0xc5d5f09fU,
  0x077ec3baU, 0x238bfb99U, 0xc1b631a2U, 0x03e81187U,
  0x233db117U, 0xcbc38405U, 0x6ef04659U, 0xa4a11de4U,
  0x9f7ecb29U, 0xbada8f98U, 0x0decece9U, 0x2e30c48fU };

static mpw spp_17[] =	/* primes 3 to 401 */
{ 0x5aa88d8cU, 0x594bb372U, 0xc4bc813fU, 0x4a87a266U,
  0x1f984840U, 0xdab15692U, 0x2c2a177dU, 0x95843665U,
  0x6f36d41aU, 0x11c35cccU, 0x2904b7e9U, 0xc424eb61U,
  0x3b3536a4U, 0x0b2745bdU, 0xadf1a6c9U, 0x7b23e85aU,
  0xdc6695c1U };

static mpw spp_18[] =	/* primes 3 to 421 */
{ 0x0185dbebU, 0x2b8b11d3U, 0x7633e9dcU, 0x1eec5415U,
  0x65c6ce84U, 0x31d227eeU, 0x28f0328aU, 0x60c90118U,
  0xae031cc5U, 0xa781c824U, 0xd1f16d25U, 0xf4f0cccfU,
  0xf35e9745U, 0x79072ec8U, 0xcaf1ac8eU, 0xefd5566fU,
  0xa15fb94fU, 0xe34f5d37U };

static mpw spp_19[] =	/* primes 3 to 443 */
{ 0x0cde6fd1U, 0xcf108066U, 0xcc548df9U, 0x070e102cU,
  0x2c651b88U, 0x5f24f503U, 0xaaffe276U, 0xfeb57311U,
  0x0c1e4592U, 0xa35890d7U, 0x678aaeeeU, 0x9f44800fU,
  0xc43f999dU, 0x5d06b89fU, 0xcb22e533U, 0x5a9287bcU,
  0x6d75a3e9U, 0x1e53906dU, 0x413163d5U };

static mpw spp_20[] =	/* primes 3 to 463 */
{ 0x833a505cU, 0xf9922beeU, 0xc80265a6U, 0xd50e1cceU,
  0xa22f6fecU, 0x2eb84450U, 0xcec64a3cU, 0x0e10d472U,
  0xdd653b9bU, 0x51d81d0eU, 0x3a3142eaU, 0x49b91e3aU,
  0x5e210232U, 0x67bda426U, 0x738730cfU, 0xb8e6e2aeU,
  0xc08c9d4bU, 0xd2420066U, 0xdccf95efU, 0x49a560b7U };

static mpw spp_21[] =	/* primes 3 to 487 */
{ 0x035417f1U, 0xe321c06cU, 0xbe32ffceU, 0xae752cc9U,
  0xa9fe11a6U, 0x3d94c946U, 0x456edd7dU, 0x5a060de1U,
  0x84a826a6U, 0xf0740c13U, 0x48fa1038U, 0x911d771dU,
  0xb3773e87U, 0x52300c29U, 0xc82c3012U, 0x131673bbU,
  0x491cbd61U, 0x55e565afU, 0x4a9f4331U, 0x0adbb0d7U,
  0x06e86f6dU };

static mpw spp_22[] =	/* primes 3 to 509 */
{ 0x309d024bU, 0xd5380319U, 0x2ca33469U, 0x0bafb43aU,
  0x0abd5840U, 0xfbeb24d1U, 0xf49b6330U, 0x47902baeU,
  0x581ca4cbU, 0xa778fdb1U, 0x6dc0a6afU, 0xef960687U,
  0x16855d95U, 0x93746604U, 0x201f1919U, 0xb725fcb7U,
  0x8ffd0db8U, 0xe8fa61a1U, 0x6e1c0970U, 0xbeb81adcU,
  0xf49c82dfU, 0xf960d36fU };

static mpw spp_23[] =	/* primes 3 to 541 */
{ 0x01ab244aU, 0x33bc047eU, 0x804590b4U, 0xc3207237U,
  0xea503fa0U, 0x7541b251U, 0x57cfd03fU, 0xf602c9d0U,
  0x3dcd12baU, 0xa4947ae6U, 0xc6ee61beU, 0xedf6c716U,
  0xfa45377dU, 0x5b3c84faU, 0x5fb78b41U, 0x395251ebU,
  0xb6a5129cU, 0x7699fb5cU, 0xccec6d45U, 0x56c9b8eaU,
  0xfa05897cU, 0xb8c5cf72U, 0xb77603d9U };

static mpw spp_24[] =	/* primes 3 to 569 */
{ 0x25eac89fU, 0x8d4da338U, 0x337b4985U, 0x0d2d1489U,
  0x2663177bU, 0x4010af3dU, 0xd23eeb0bU, 0x228f3832U,
  0xffcee2e5U, 0xcbd1acc9U, 0x8f47f251U, 0x873380aeU,
  0x10f0ffddU, 0x8e602ffaU, 0x210f41f6U, 0x69a1570aU,
  0x93c158c1U, 0xa9a8227fU, 0xf81a90c5U, 0x630e9c44U,
  0x845c755cU, 0x7df35a7dU, 0x430c679aU, 0x11575655U };

static mpw spp_25[] =	/* primes 3 to 587 */
{ 0x01b515a8U, 0xdca3d6e4U, 0x69090373U, 0x84febfe8U,
  0xf32e06cfU, 0x9bde8c89U, 0x6b3f992fU, 0x2ff23508U,
  0xe1c01024U, 0x3b8ad0c4U, 0xac54e7c7U, 0x3f4081d8U,
  0xe495d54dU, 0x74ed01e8U, 0x9dfcbddeU, 0x1fe7e61aU,
  0x839bd902U, 0xf43bf273U, 0x2441f0aeU, 0xb4211c70U,
  0x6b3faafcU, 0x0f200b35U, 0x7485ce4aU, 0x2f08f148U,
  0xcce6887dU };

static mpw spp_26[] =	/* primes 3 to 607 */
{ 0x3383219dU, 0x26454f06U, 0xe2789b7fU, 0x9c3b940eU,
  0x03be2105U, 0x798e3ff7U, 0x945bd325U, 0x997bc262U,
  0x025598f8U, 0x8577748eU, 0xc7155ff8U, 0x8a1ff4c9U,
  0x2ce95bd8U, 0xb015101fU, 0x19b73b14U, 0x81627f9aU,
  0x6f83da3aU, 0x03259fbdU, 0x41f92a6eU, 0x85ac6efaU,
  0xde195be8U, 0x6e66ba89U, 0xb0ab042dU, 0x3276976cU,
  0x3dbeb3d7U, 0x413ea96dU };

static mpw spp_27[] =	/* primes 3 to 619 */
{ 0x02ced4b7U, 0xf15179e8U, 0x7fcba6daU, 0x7b07a6f3U,
  0xf9311218U, 0xa7b88985U, 0xac74b503U, 0xbf745330U,
  0x6d0a23f5U, 0x27a1fa9aU, 0xc2b85f1aU, 0x26152470U,
  0x6ac242f3U, 0x518cc497U, 0x09a23d74U, 0xff28da52U,
  0xe7bbf7f7U, 0xa63c1c88U, 0x6f684195U, 0x65e472ceU,
  0x80751585U, 0xc70e20c2U, 0x2d15d3feU, 0xc1b40c7fU,
  0x8e25dd07U, 0xdb09dd86U, 0x791aa9e3U };

static mpw spp_28[] =	/* primes 3 to 647 */
{ 0x6e026454U, 0x60adbd18U, 0xcd52ce1aU, 0x1beab1c0U,
  0x36e468e9U, 0xf350d69bU, 0x1d357d08U, 0x3a59f778U,
  0xc2cc262bU, 0x4a29ce52U, 0x509bcf97U, 0x349ba2bfU,
  0x22402d71U, 0x6b32517eU, 0x1941e18aU, 0xce76cbd8U,
  0x5809701eU, 0x70eaef96U, 0x9aac365cU, 0x8a9fea5eU,
  0xc74d951dU, 0xb361f061U, 0xc4d14f00U, 0x0d806db4U,
  0xcd939110U, 0xc7cab492U, 0x2f3ea4c4U, 0x852ca469U };

static mpw spp_29[] =	/* primes 3 to 661 */
{ 0x074921f7U, 0x6a76cec3U, 0xaeb05f74U, 0x60b21f16U,
  0x49dece2fU, 0x21bb3ed9U, 0xe4cb4ebcU, 0x05d6f408U,
  0xed3d408aU, 0xdee16505U, 0xdc657c6dU, 0x93877982U,
  0xf2d11ce6U, 0xcb5b0bb0U, 0x579b3189U, 0xb339c2ccU,
  0xcf81d846U, 0xa9fbde0cU, 0x723afbc7U, 0x36655d41U,
  0x0018d768U, 0x21779cf3U, 0x52642f1bU, 0x2d17165dU,
  0xc7001c45U, 0x4a84a45dU, 0x66007591U, 0x27e85693U,
  0x2288d0fbU };

static mpw spp_30[] =	/* primes 3 to 683 */
{ 0x00872313U, 0x1f66758aU, 0x414bbebbU, 0x2f8670bfU,
  0x01dc959dU, 0x74468901U, 0x57c57f40U, 0xe210c9c2U,
  0x74f54469U, 0x7c71cc1dU, 0xe2be67a2U, 0x03d8d56fU,
  0x6c363fcaU, 0x0a78676aU, 0x2b977789U, 0x6ea2db50U,
  0xdb31b737U, 0x51992f73U, 0x0def293eU, 0xbc028877U,
  0xdf95ac1bU, 0x4d0c0128U, 0x9a0b05e0U, 0x0e6c0bc8U,
  0xe61b766eU, 0xc0943254U, 0x1cd70f0fU, 0xd5a0ce6bU,
  0x8ab998fbU, 0x8ab36e0dU };

static mpw spp_31[] =	/* primes 3 to 719 */
{ 0x1e595df4U, 0x3064a8c9U, 0xd61ae17bU, 0xde1938f0U,
  0x22ee6357U, 0x35f4caddU, 0x3d39f473U, 0xafed7df5U,
  0x92ae0fd3U, 0xfe910508U, 0x9ad9e939U, 0x988b0227U,
  0x60dec749U, 0xae7ee54fU, 0xeb0572acU, 0x0aed266dU,
  0x92daafd8U, 0x6135f7a3U, 0xe4e8bf05U, 0x0124c928U,
  0xb0d719d5U, 0x2181aec8U, 0x0f79820fU, 0xcb158642U,
  0x20969ec0U, 0x1a480d31U, 0x331b3252U, 0x01b36fabU,
  0x3d5b415bU, 0x1a4567e7U, 0x3baf6389U };

static mpw spp_32[] =	/* primes 3 to 739 */
{ 0x02c85ff8U, 0x70f24be8U, 0x0f62b1baU, 0x6c20bd72U,
  0xb837efdfU, 0x121206d8U, 0x7db56b7dU, 0x69fa4c02U,
  0x1c107c3cU, 0xa206fe8fU, 0xa7080ef5U, 0x76effc82U,
  0xf9b10f57U, 0x50656b77U, 0x94b16afdU, 0x70996e91U,
  0xaef6e0adU, 0x15e91b07U, 0x1ac9b24dU, 0x98b233adU,
  0x86ee0555U, 0x18e58e56U, 0x638ef18bU, 0xac5c74cbU,
  0x35bbb6e5U, 0xdae2783dU, 0xd1c0ce7dU, 0xec4fc70eU,
  0x5186d411U, 0xdf36368fU, 0x061aa360U, 0x11f30179U };

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

/*
 * needs workspace of (size*2) words
 */
static void mpprndbits(mpbarrett* p, size_t bits, size_t lsbset, const mpnumber* min, const mpnumber* max, randomGeneratorContext* rc, mpw* wksp)
{
	register size_t size = p->size;
	register size_t msbclr = MP_WORDS_TO_BITS(size) - bits;

	/* assume that mpbits(max) == bits */
	/* calculate k=max-min; generate q such that 0 <= q <= k; then set p = q + min */
	/* for the second step, set the appropriate number of bits */

	if (max)
	{
		mpsetx(size, wksp, max->size, max->data);
	} 
	else
	{
		mpfill(size, wksp, MP_ALLMASK);
		wksp[0] &= (MP_ALLMASK >> msbclr);
	}
	if (min)
	{
		mpsetx(size, wksp+size, min->size, min->data);
	}
	else
	{
		mpzero(size, wksp+size);
		wksp[size] |= (MP_MSBMASK >> msbclr);
	}

	mpsub(size, wksp, wksp+size);

    rc->rng->next(rc->param, (byte*) p->modl, MP_WORDS_TO_BYTES(size));

	p->modl[0] &= (MP_ALLMASK >> msbclr);

	while (mpgt(size, p->modl, wksp))
		mpsub(size, p->modl, wksp);

	mpadd(size, p->modl, wksp+size);

	if (lsbset)
		p->modl[size-1] |= (MP_ALLMASK >> (MP_WBITS - lsbset));
}

/*
 * mppsppdiv_w
 *  needs workspace of (3*size) words
 */
int mppsppdiv_w(const mpbarrett* p, mpw* wksp)
{
	/* small prime product trial division test */
	register size_t size = p->size;

	if (size > SMALL_PRIMES_PRODUCT_MAX)
	{
		mpsetx(size, wksp+size, SMALL_PRIMES_PRODUCT_MAX, mpspprod[SMALL_PRIMES_PRODUCT_MAX-1]);
		mpgcd_w(size, p->modl, wksp+size, wksp, wksp+2*size);
	}
	else
	{
		mpgcd_w(size, p->modl, mpspprod[size-1], wksp, wksp+2*size);
	}

	return mpisone(size, wksp);
}

/*
 * needs workspace of (5*size+2)
 */
int mppmilrabtwo_w(const mpbarrett* p, int s, const mpw* rdata, const mpw* ndata, mpw* wksp)
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

/*
 * needs workspace of (5*size+2) words
 */
int mppmilraba_w(const mpbarrett* p, const mpw* adata, int s, const mpw* rdata, const mpw* ndata, mpw* wksp)
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

/*
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
	mpsubw(size, ndata, 1);
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

/*
 * needs workspace of (8*size+2) words
 */
int mpprnd_w(mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, const mpnumber* f, mpw* wksp)
{
	return mpprndr_w(p, rc, bits, t, (const mpnumber*) 0, (const mpnumber*) 0, f, wksp);
}

/*
 * implements IEEE P1363 A.15.6
 *
 * f, min, max are optional
 */
int mpprndr_w(mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, const mpnumber* min, const mpnumber* max, const mpnumber* f, mpw* wksp)
{
	/*
	 * Generate a prime into p with the requested number of bits
	 *
	 * Conditions: size(f) <= size(p)
	 *
	 * Optional input min: if min is not null, then search p so that min <= p
	 * Optional input max: if max is not null, then search p so that p <= max
	 * Optional input f: if f is not null, then search p so that GCD(p-1,f) = 1
	 */

	size_t size = MP_BITS_TO_WORDS(bits + MP_WBITS - 1);

	/* if min has more bits than what was requested for p, bail out */
	if (min && (mpbits(min->size, min->data) > bits))
		return -1;

	/* if max has a different number of bits than what was requested for p, bail out */
	if (max && (mpbits(max->size, max->data) != bits))
		return -1;

	/* if min is not less than max, bail out */
	if (min && max && mpgex(min->size, min->data, max->size, max->data))
		return -1;

	mpbinit(p, size);

	if (p->modl)
	{
		while (1)
		{
			/*
			 * Generate a random appropriate candidate prime, and test
			 * it with small prime divisor test BEFORE computing mu
			 */
			mpprndbits(p, bits, 1, min, max, rc, wksp);

			/* do a small prime product trial division test on p */
			if (!mppsppdiv_w(p, wksp))
				continue;

			/* if we have an f, do the congruence test */
			if (f != (mpnumber*) 0)
			{
				mpcopy(size, wksp, p->modl);
				mpsubw(size, wksp, 1);
				mpsetx(size, wksp+size, f->size, f->data);
				mpgcd_w(size, wksp, wksp+size, wksp+2*size, wksp+3*size);

				if (!mpisone(size, wksp+2*size))
					continue;
			}

			/* candidate has passed so far, now we do the probabilistic test */
			mpbmu_w(p, wksp);

			if (mppmilrab_w(p, rc, t, wksp))
				return 0;
		}
	}
	return -1;
}

/*
 * needs workspace of (8*size+2) words
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
			mpprndbits(&s, sbits, 0, (mpnumber*) 0, (mpnumber*) 0, rc, wksp);

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
			mpmultwo(p->size, p->modl);
			mpaddw(p->size, p->modl, 1);
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
				mpsubw(p->size, wksp, 1);
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
			mpmultwo(r->size, r->data);
			mpbfree(&s);

			return;
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

		while (1)
		{
			/*
			 * Generate a random appropriate candidate prime, and test
			 * it with small prime divisor test BEFORE computing mu
			 */

			mpprndbits(p, bits, 2, (mpnumber*) 0, (mpnumber*) 0, rc, wksp);

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
	}
}
