/*
 * beecrypt.h
 *
 * BeeCrypt library hooks & stubs, header
 *
 * Copyright (c) 1999, 2000, 2001 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
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

#ifndef _BEECRYPT_H
#define _BEECRYPT_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "memchunk.h"
#include "mp32number.h"

/** \name Entropy sources */
/*@{*/

/** \ingroup ES_m
 * Return an array of 32-bit unsigned integers of given size with
 * entropy data.
 *
 * @retval data		entropy data
 * @param size		no. of ints of data
 * @return		0 on success, -1 on failure
 */
typedef int (*entropyNext) (/*@out@*/ uint32* data, int size)
	/*@modifies data @*/;

/** \ingroup ES_m
 * Methods and parameters for entropy sources.
 * Each specific entropy source MUST be written to be multithread-safe.
 */
typedef struct
{
/*@observer@*/ const char* name;	/*!< entropy source name */
/*@unused@*/ const entropyNext next;	/*!< return entropy function */
} entropySource;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup ES_m
 * Return the number of entropy sources available.
 * @return		number of entropy sources available
 */
BEEDLLAPI
int entropySourceCount(void)
	/*@*/;

/** \ingroup ES_m
 * Retrieve a entropy source by index.
 * @param index		entropy source index
 * @return		entropy source pointer (or NULL)
 */
BEEDLLAPI /*@observer@*/ /*@null@*/
const entropySource* entropySourceGet(int index)
	/*@*/;

/** \ingroup ES_m
 * Retrieve a entropy source by name.
 * @param name		entropy source name
 * @return		entropy source pointer (or NULL)
 */
/*@-exportlocal@*/
BEEDLLAPI /*@observer@*/ /*@null@*/
const entropySource* entropySourceFind(const char* name)
	/*@*/;
/*@=exportlocal@*/

/** \ingroup ES_m
 * Retrieve the default entropy source.
 * If the BEECRYPT_ENTROPY environment variable is set, use that
 * entropy source. Otherwise, use the 1st entry in the internal table.
 * @return		entropy source pointer (or NULL)
 */
BEEDLLAPI /*@observer@*/ /*@null@*/ /*@unused@*/
const entropySource* entropySourceDefault(void)
	/*@*/;

/** \ingroup ES_m
 * Gather entropy from multiple sources (if BEECRYPT_ENTROPY is not set).
 *
 * @retval data		entropy data
 * @param size		no. of ints of data
 * @return		0 on success, -1 on failure
 */
BEEDLLAPI
int entropyGatherNext(uint32* data, int size)
	/*@*/;

#ifdef __cplusplus
}
#endif

/*@}*/
/** \name Pseudo-random Number Generators */
/*@{*/

/** \ingroup PRNG_m
 */
typedef void randomGeneratorParam;

/** \ingroup PRNG_m
 * Initialize the parameters for use, and seed the generator
 * with entropy from the default entropy source.
 *
 * @param param		generator parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorSetup) (randomGeneratorParam* param)
	/*@modifies *param @*/;

/** \ingroup PRNG_m
 * Re-seed the random generator with user-provided entropy.
 *
 * @param param		generator parameters
 * @param data		user entropy
 * @param size		no. of ints of entropy
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorSeed) (randomGeneratorParam* param, const uint32* data, int size)
	/*@modifies *param @*/;

/** \ingroup PRNG_m
 * Return an array of 32-bit unsigned integers of given size with
 * pseudo-random data.
 *
 * @param param		generator parameters
 * @retval data		pseudo-random data
 * @param size		no. of ints of data
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorNext) (randomGeneratorParam* param, /*@out@*/ uint32* data, int size)
	/*@modifies *param, *data @*/;

/** \ingroup PRNG_m
 * Cleanup after using a generator.
 *
 * @param param		generator parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorCleanup) (randomGeneratorParam* param)
	/*@modifies *param @*/;

/** \ingroup PRNG_m
 * Methods and parameters for random generators.
 * Each specific random generator MUST be written to be multithread safe.
 *
 * @warning Each randomGenerator, when used in cryptographic applications, MUST
 * be guaranteed to be of suitable quality and strength (i.e. don't use the
 * random() function found in most UN*X-es).
 *
 * Multiple instances of each randomGenerator can be used (even concurrently),
 * provided they each use their own randomGeneratorParam parameters, a chunk
 * of memory which must be at least as large as indicated by the paramsize
 * field.
 *
 */
typedef struct
{
/*@observer@*/ const char* name;	/*!< random generator name */
    const unsigned int paramsize;
    const randomGeneratorSetup setup;
    const randomGeneratorSeed seed;
    const randomGeneratorNext next;
    const randomGeneratorCleanup cleanup;
} randomGenerator;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup PRNG_m
 * Return the number of generators available.
 * @return		number of generators available
 */
BEEDLLAPI
int randomGeneratorCount(void)
	/*@*/;

/** \ingroup PRNG_m
 * Retrieve a generator by index.
 * @param index		generator index
 * @return		generator pointer (or NULL)
 */
BEEDLLAPI /*@observer@*/ /*@null@*/
const randomGenerator* randomGeneratorGet(int index)
	/*@*/;

/** \ingroup PRNG_m
 * Retrieve a generator by name.
 * @param name		generator name
 * @return		generator pointer (or NULL)
 */
/*@-exportlocal@*/
BEEDLLAPI /*@observer@*/ /*@null@*/
const randomGenerator* randomGeneratorFind(const char* name)
	/*@*/;
/*@=exportlocal@*/

/** \ingroup PRNG_m
 * Retrieve the default generator.
 * If the BEECRYPT_RANDOM environment variable is set, use that
 * generator. Otherwise, use "fips186prng".
 * @return		generator pointer
 */
BEEDLLAPI /*@observer@*/ /*@null@*/
const randomGenerator* randomGeneratorDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/** \ingroup PRNG_m
 * A randomGenerator instance, global functions and specific parameters.
 */
typedef struct
{
/*@observer@*/ /*@dependent@*/ const randomGenerator* rng; /*!< global functions and parameters */
/*@only@*/ randomGeneratorParam* param;	/*!< specific parameters */
} randomGeneratorContext;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup PRNG_m
 * Initialize a randomGenerator instance.
 */
BEEDLLAPI
int randomGeneratorContextInit(randomGeneratorContext* ctxt, /*@observer@*/ /*@dependent@*/ const randomGenerator* rng)
	/*@modifies ctxt->rng, ctxt->param @*/;

/** \ingroup PRNG_m
 * Destroy a randomGenerator instance.
 */
BEEDLLAPI
int randomGeneratorContextFree(/*@special@*/ randomGeneratorContext* ctxt)
	/*@uses ctxt->rng @*/
	/*@releases ctxt->param @*/
	/*@modifies ctxt->rng, ctxt->param @*/;

#ifdef __cplusplus
}
#endif

/*@}*/
/** \name Hash Functions */
/*@{*/

/** \ingroup HASH_m
 */
BEEDLLAPI
typedef void hashFunctionParam;

/** \ingroup HASH_m
 * Re-initialize the parameters of the hash function.
 *
 * @param param		hash parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*hashFunctionReset) (hashFunctionParam* param)
	/*@modifies *param @*/;

/** \ingroup HASH_m
 * Update the hash function with an array of bytes.
 *
 * @param param		hash parameters
 * @param data		array of bytes
 * @param size		no. of bytes
 * @return		0 on success, -1 on failure
 */
typedef int (*hashFunctionUpdate) (hashFunctionParam* param, const byte* data, int size)
	/*@modifies *param @*/;

/** \ingroup HASH_m
 * Compute the digest of all the data passed to the hash function, and return
 * the result in data.
 *
 * @note data must be at least have a bytesize of 'digestsize' as described
 * in the hashFunction struct.
 *
 * @note For safety reasons, after calling digest, each specific implementation
 * MUST reset itself so that previous values in the parameters are erased.
 *
 * @param param		hash parameters
 * @retval data		digest
 * @return		0 on success, -1 on failure
 */
typedef int (*hashFunctionDigest) (hashFunctionParam* param, /*@out@*/ uint32* data)
	/*@modifies *param, *data @*/;

/** \ingroup HASH_m
 * Methods and parameters for hash functions.
 * Specific hash functions MAY be written to be multithread-safe.
 */
typedef struct
{
/*@observer@*/ const char* name;	/*!< hash function name */
    const unsigned int paramsize;	/*!< in bytes */
    const unsigned int blocksize;	/*!< in bytes */
    const unsigned int digestsize;	/*!< in bytes */
    const hashFunctionReset reset;
    const hashFunctionUpdate update;
    const hashFunctionDigest digest;
} hashFunction;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HASH_m
 * Return the number of hash functions available.
 * @return		number of hash functions available
 */
BEEDLLAPI
int hashFunctionCount(void)
	/*@*/;

/** \ingroup HASH_m
 * Retrieve a hash function by index.
 * @param index		hash function index
 * @return		hash function pointer (or NULL)
 */
BEEDLLAPI /*@observer@*/ /*@null@*/
const hashFunction* hashFunctionGet(int index)
	/*@*/;

/** \ingroup HASH_m
 * Retrieve a hash function by name.
 * @param name		hash function name
 * @return		hash function pointer (or NULL)
 */
/*@-exportlocal@*/
BEEDLLAPI /*@observer@*/ /*@null@*/
const hashFunction* hashFunctionFind(const char* name)
	/*@*/;
/*@=exportlocal@*/

/** \ingroup HASH_m
 * Retrieve the default hash function.
 * If the BEECRYPT_HASH environment variable is set, use that
 * hash function. Otherwise, use "sha1".
 * @return		hash function pointer (or NULL)
 */
BEEDLLAPI /*@observer@*/ /*@null@*/ /*@unused@*/
const hashFunction* hashFunctionDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/** \ingroup HASH_m
 * A hashFunction instance, global functions and specific parameters.
 */
typedef struct
{
/*@observer@*/ /*@dependent@*/ const hashFunction* algo;/*!< global functions and parameters */
/*@only@*/ hashFunctionParam* param;	/*!< specific parameters */
} hashFunctionContext;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HASH_m
 * Initialize a hashFunction instance.
 */
BEEDLLAPI
int hashFunctionContextInit(hashFunctionContext* ctxt, /*@observer@*/ /*@dependent@*/ const hashFunction* hash)
	/*@modifies ctxt->algo, ctxt->param */;

/** \ingroup HASH_m
 * Destroy a hashFunction instance.
 */
BEEDLLAPI
int hashFunctionContextFree(/*@special@*/ hashFunctionContext* ctxt)
	/*@releases ctxt->param @*/
	/*@modifies ctxt->algo, ctxt->param */;

/** \ingroup HASH_m
 */
BEEDLLAPI
int hashFunctionContextReset(hashFunctionContext* ctxt)
	/*@modifies ctxt */;

/** \ingroup HASH_m
 */
BEEDLLAPI
int hashFunctionContextUpdate(hashFunctionContext* ctxt, const byte* data, int size)
	/*@modifies ctxt */;

/** \ingroup HASH_m
 */
BEEDLLAPI /*@unused@*/
int hashFunctionContextUpdateMC(hashFunctionContext* ctxt, const memchunk* m)
	/*@modifies ctxt */;

/** \ingroup HASH_m
 */
BEEDLLAPI
int hashFunctionContextUpdateMP32(hashFunctionContext* ctxt, const mp32number* n)
	/*@modifies ctxt */;

/** \ingroup HASH_m
 */
BEEDLLAPI
int hashFunctionContextDigest(hashFunctionContext* ctxt, mp32number* dig)
	/*@modifies ctxt, *dig */;

/** \ingroup HASH_m
 */
BEEDLLAPI /*@unused@*/
int hashFunctionContextDigestMatch(hashFunctionContext* ctxt, const mp32number* match)
	/*@modifies ctxt */;

#ifdef __cplusplus
}
#endif

/*@}*/
/** \name Keyed Hash Functions, a.k.a. Message Authentication Codes */
/*@{*/

/** \ingroup HMAC_m
 */
typedef void keyedHashFunctionParam;

/** \ingroup HMAC_m
 * Setup the keyed hash function parameters with the given secret key.
 * This can also be used to reset the parameters.
 *
 * @note After use, it is recommended to wipe the parameters by calling setup
 * again with another (dummy) key.
 *
 * @param param		keyed hash parameters
 * @param key		secret key
 * @param keybits	no. bits in secret key
 * @return		0 on success, -1 on failure
 */
typedef int (*keyedHashFunctionSetup) (keyedHashFunctionParam* param, const uint32* key, int keybits)
	/*@modifies *param @*/;

/** \ingroup HMAC_m
 * Re-initialize the parameters of a keyed hash function.
 *
 * @param param		keyed hash parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*keyedHashFunctionReset) (keyedHashFunctionParam* param)
	/*@modifies *param @*/;

/** \ingroup HMAC_m
 * Update the keyed hash function with an array of bytes.
 *
 * @param param		keyed hash parameters
 * @param data		array of bytes
 * @param size		no. of bytes
 * @return		0 on success, -1 on failure
 */
typedef int (*keyedHashFunctionUpdate) (keyedHashFunctionParam* param, const byte* data, int size)
	/*@modifies *param @*/;

/** \ingroup HMAC_m
 * Compute the digest (or authentication code) of all the data passed to
 * the keyed hash function, and return the result in data.
 *
 * @note data must be at least have a bytesize of 'digestsize' as described
 * in the keyedHashFunction struct.
 *
 * @note For safety reasons, after calling digest, each specific implementation
 * MUST reset itself so that previous values in the parameters are erased.
 *
 * @param param		keyed hash parameters
 * @retval data		digest (or authentication code)
 * @return		0 on success, -1 on failure
 */
typedef int (*keyedHashFunctionDigest) (keyedHashFunctionParam* param, /*@out@*/ uint32* data)
	/*@modifies *param, *data @*/;

/** \ingroup HMAC_m
 * Methods and parameters for keyed hash functions.
 * Specific keyed hash functions MAY be written to be multithread-safe.
 */
typedef struct
{
/*@observer@*/ const char* name;	/*!< keyed hash function name */
    const unsigned int paramsize;	/*!< in bytes */
    const unsigned int blocksize;	/*!< in bytes */
    const unsigned int digestsize;	/*!< in bytes */
    const unsigned int keybitsmin;	/*!< min keysize in bits */
    const unsigned int keybitsmax;	/*!< max keysize in bits */
    const unsigned int keybitsinc;	/*!< keysize increment in bits */
    const keyedHashFunctionSetup setup;
    const keyedHashFunctionReset reset;
    const keyedHashFunctionUpdate update;
    const keyedHashFunctionDigest digest;
} keyedHashFunction;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HMAC_m
 * Return the number of keyed hash functions available.
 * @return		number of keyed hash functions available
 */
BEEDLLAPI
int keyedHashFunctionCount(void)
	/*@*/;

/** \ingroup HMAC_m
 * Retrieve a keyed hash function by index.
 * @param index		keyed hash function index
 * @return		keyed hash function pointer (or NULL)
 */
BEEDLLAPI /*@observer@*/ /*@null@*/
const keyedHashFunction* keyedHashFunctionGet(int index)
	/*@*/;

/** \ingroup HMAC_m
 * Retrieve a keyed hash function by name.
 * @param name		keyed hash function name
 * @return		keyed hash function pointer (or NULL)
 */
/*@-exportlocal@*/
BEEDLLAPI /*@observer@*/ /*@null@*/
const keyedHashFunction* keyedHashFunctionFind(const char* name)
	/*@*/;
/*@=exportlocal@*/

/** \ingroup HMAC_m
 * Retrieve the default keyed hash function.
 * If the BEECRYPT_KEYEDHASH environment variable is set, use that keyed
 * hash function. Otherwise, use "hmacsha1".
 * @return		keyed hash function pointer
 */
BEEDLLAPI /*@observer@*/ /*@null@*/ /*@unused@*/
const keyedHashFunction* keyedHashFunctionDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/** \ingroup HMAC_m
 * A keyedHashFunction instance, global functions and specific parameters.
 */
typedef struct
{
/*@observer@*/ /*@dependent@*/ const keyedHashFunction* algo;	/*!< global functions and parameters */
/*@only@*/ keyedHashFunctionParam* param;	/*!< specific parameters */
} keyedHashFunctionContext;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HMAC_m
 * Initialize a keyedHashFunction instance.
 */
BEEDLLAPI
int keyedHashFunctionContextInit(keyedHashFunctionContext* ctxt, /*@observer@*/ /*@dependent@*/ const keyedHashFunction* mac)
	/*@modifies ctxt->algo, ctxt->param @*/;

/** \ingroup HMAC_m
 * Destroy a keyedHashFunction instance.
 */
BEEDLLAPI
int keyedHashFunctionContextFree(/*@special@*/ keyedHashFunctionContext* ctxt)
	/*@uses ctxt->algo @*/
	/*@releases ctxt->param @*/
	/*@modifies ctxt->algo, ctxt->param @*/;

/** \ingroup HMAC_m
 */
BEEDLLAPI
int keyedHashFunctionContextSetup(keyedHashFunctionContext* ctxt, const uint32* key, int keybits)
	/*@modifies ctxt @*/;

/** \ingroup HMAC_m
 */
BEEDLLAPI /*@unused@*/
int keyedHashFunctionContextReset(keyedHashFunctionContext* ctxt)
	/*@modifies ctxt @*/;

/** \ingroup HMAC_m
 */
BEEDLLAPI /*@unused@*/
int keyedHashFunctionContextUpdate(keyedHashFunctionContext* ctxt, const byte* data, int size)
	/*@modifies ctxt @*/;

/** \ingroup HMAC_m
 */
BEEDLLAPI
int keyedHashFunctionContextUpdateMC(keyedHashFunctionContext* ctxt, const memchunk* m)
	/*@modifies ctxt @*/;

/** \ingroup HMAC_m
 */
BEEDLLAPI /*@unused@*/
int keyedHashFunctionContextUpdateMP32(keyedHashFunctionContext* ctxt, const mp32number* n)
	/*@modifies ctxt @*/;

/** \ingroup HMAC_m
 */
BEEDLLAPI
int keyedHashFunctionContextDigest(keyedHashFunctionContext* ctxt, mp32number* dig)
	/*@modifies ctxt, *dig @*/;

/** \ingroup HMAC_m
 */
BEEDLLAPI
int keyedHashFunctionContextDigestMatch(keyedHashFunctionContext* ctxt, const mp32number* match)
	/*@modifies ctxt @*/;

#ifdef __cplusplus
}
#endif

/*@}*/
/** \name Block ciphers */
/*@{*/

/** \ingroup BC_m
 */
typedef void blockCipherParam;

/** \ingroup BC_m
 * Block cipher operations.
 */
typedef enum
{
	ENCRYPT,
	DECRYPT
} cipherOperation;

/** \ingroup BC_m
 * Block cipher modes.
 */
typedef enum
{
	ECB,
	CBC
} cipherMode;

/** \ingroup BC_m
 * @param param		blockcipher parameters
 * @param size		no. of ints
 * @retval dst		ciphertext block
 * @param src		plaintext block
 * @return		0 on success, -1 on failure
 */
typedef int (*blockModeEncrypt) (blockCipherParam* param, int count, uint32* dst, const uint32* src)
	/*@modifies *param, *dst @*/;

/** \ingroup BC_m
 * @param param		blockcipher parameters
 * @param size		no. of ints
 * @retval dst		plainttext block
 * @param src		ciphertext block
 * @return		0 on success, -1 on failure
 */
typedef int (*blockModeDecrypt) (blockCipherParam* param, int count, uint32* dst, const uint32* src)
	/*@modifies *param, *dst @*/;

/** \ingroup BC_m
 */
typedef struct
{
	const blockModeEncrypt	encrypt;
	const blockModeDecrypt	decrypt;
} blockMode;

/** \ingroup BC_m
 * Setup the blockcipher parameters with the given secret key for either
 * encryption or decryption.
 *
 * @note After use, it is recommended to wipe the parameters by calling setup
 * again with another (dummy) key.
 *
 * @param param		blockcipher parameters
 * @param key		secret key
 * @param keybits	no. bits in secret key
 * @param cipherOperation
 * @return		0 on success, -1 on failure
 */
typedef int (*blockCipherSetup) (blockCipherParam* param, const uint32* key, int keybits, cipherOperation cipherOperation)
	/*@modifies param @*/;

/** \ingroup BC_m
 * Initialize IV for blockcipher.
 * @param param		blockcipher parameters
 * @param data		iv data
 * @return		0 on success, -1 on failure
 */
typedef int (*blockCipherSetIV) (blockCipherParam* param, const uint32* data)
	/*@modifies param @*/;

/** \ingroup BC_m
 * Encrypt one block of data (with bit size chosen by the blockcipher).
 * @note This is raw encryption, without padding, etc.
 *
 * @param param		blockcipher parameters
 * @retval dst		ciphertext block
 * @param src		plaintext block
 * @return		0 on success, -1 on failure
 */
typedef int (*blockCipherEncrypt) (blockCipherParam* param, uint32* dst, const uint32* src)
	/*@modifies param, dst @*/;

/** \ingroup BC_m
 * Decrypt one block of data (with bit size chosen by the blockcipher).
 * @note This is raw decryption, without padding, etc.
 *
 * @param param		blockcipher parameters
 * @retval dst		plaintext block
 * @param src		ciphertext block
 * @return		0 on success, -1 on failure
 */
typedef int (*blockCipherDecrypt) (blockCipherParam* param, uint32* dst, const uint32* src)
	/*@modifies param, dst @*/;

/** \ingroup BC_m
 * Methods and parameters for block ciphers.
 * Specific block ciphers MAY be written to be multithread-safe.
 */
typedef struct
{
/*@observer@*/ const char* name;	/*!< block cipher name */
    const unsigned int paramsize;	/*!< in bytes */
    const unsigned int blocksize;	/*!< in bytes */
    const unsigned int keybitsmin;	/*!< min keysize in bits */
    const unsigned int keybitsmax;	/*!< max keysize in bits */
    const unsigned int keybitsinc;	/*!< keysize increment in bits */
    const blockCipherSetup setup;
    const blockCipherSetIV setiv;
    const blockCipherEncrypt encrypt;
    const blockCipherDecrypt decrypt;
/*@dependent@*/ const blockMode* mode;
} blockCipher;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup BC_m
 * Return the number of blockciphers available.
 * @return		number of blockciphers available
 */
BEEDLLAPI
int blockCipherCount(void)
	/*@*/;

/** \ingroup BC_m
 * Retrieve a blockcipher by index.
 * @param index		blockcipher index
 * @return		blockcipher pointer (or NULL)
 */
BEEDLLAPI /*@observer@*/ /*@null@*/
const blockCipher* blockCipherGet(int index)
	/*@*/;

/** \ingroup BC_m
 * Retrieve a blockcipher by name.
 * @param name		blockcipher name
 * @return		blockcipher pointer (or NULL)
 */
/*@-exportlocal@*/
BEEDLLAPI /*@observer@*/ /*@null@*/
const blockCipher* blockCipherFind(const char* name)
	/*@*/;
/*@=exportlocal@*/

/** \ingroup BC_m
 * Retrieve the default blockcipher.
 * If the BEECRYPT_CIPHER environment variable is set, use that blockcipher.
 * Otherwise, use "blowfish".
 * @return		blockcipher pointer
 */
BEEDLLAPI /*@observer@*/ /*@null@*/ /*@unused@*/
const blockCipher* blockCipherDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/** \ingroup BC_m
 * A blockCipher instance, global functions and specific parameters.
 */
typedef struct
{
/*@observer@*/ /*@dependent@*/ const blockCipher* algo;	/*!< global functions and parameters */
/*@only@*/ blockCipherParam* param;	/*!< specific parameters */
} blockCipherContext;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup BC_m
 * Initialize a blockCipher instance.
 */
BEEDLLAPI
int blockCipherContextInit(blockCipherContext* ctxt, /*@observer@*/ /*@dependent@*/ const blockCipher* ciph)
	/*@modifies ctxt->algo, ctxt->param @*/;

/** \ingroup BC_m
 */
BEEDLLAPI
int blockCipherContextSetup(blockCipherContext* ctxt, const uint32* key, int keybits, cipherOperation op)
	/*@modifies ctxt @*/;

/** \ingroup BC_m
 */
BEEDLLAPI /*@unused@*/
int blockCipherContextSetIV(blockCipherContext* ctxt, const uint32* iv)
	/*@modifies ctxt @*/;

/** \ingroup BC_m
 * Destroy a blockCipher instance.
 */
BEEDLLAPI
int blockCipherContextFree(/*@special@*/ blockCipherContext* ctxt)
	/*@releases ctxt->param @*/
	/*@modifies ctxt->algo, ctxt->param @*/;

#ifdef __cplusplus
}
#endif
/*@}*/

#endif
