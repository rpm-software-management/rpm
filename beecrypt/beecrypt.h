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

/**
 * Return an array of 32-bit unsigned integers of given size with
 * entropy data.
 *
 * @retval data		entropy data
 * @param size		no. of ints of data
 * @return		0 on success, -1 on failure
 */
typedef int (*entropyNext) (uint32* data, int size);

/**
 * The struct 'entropySource' holds information and pointers to code specific
 * to each entropy source. Each specific entropy source MUST be written to be
 * multithread-safe.
 *
 */

typedef struct
{
/*@unused@*/	const char*		name;
/*@unused@*/	const entropyNext	next;
} entropySource;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the number of entropy sources available.
 * @return		number of entropy sources available
 */
BEEDLLAPI
int entropySourceCount(void)
	/*@*/;

/**
 * Retrieve a entropy source by index.
 * @param index		entropy source index
 * @return		entropy source pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const entropySource* entropySourceGet(int index)
	/*@*/;

/**
 * Retrieve a entropy source by name.
 * @param name		entropy source name
 * @return		entropy source pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const entropySource* entropySourceFind(const char* name)
	/*@*/;

/**
 * Retrieve the default entropy source.
 * If the BEECRYPT_ENTROPY environment variable is set, use that
 * entropy source. Otherwise, use the 1st entry
 * @return		entropy source pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const entropySource* entropySourceDefault(void)
	/*@*/;

/**
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

/*
 * Pseudo-random Number Generators
 */

typedef void randomGeneratorParam;

/**
 * Initialize the parameters for use, and seed the generator
 * with entropy from the default entropy source.
 *
 * @param param		generator parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorSetup) (randomGeneratorParam* param)
	/*@modifies param @*/;

/**
 * Re-seed the random generator with user-provided entropy.
 *
 * @param param		generator parameters
 * @param data		user entropy
 * @param size		no. of ints of entropy
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorSeed) (randomGeneratorParam* param, const uint32* data, int size)
	/*@modifies param @*/;

/**
 * Return an array of 32-bit unsigned integers of given size with
 * pseudo-random data.
 *
 * @param param		generator parameters
 * @retval data		pseudo-random data
 * @param size		no. of ints of data
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorNext) (randomGeneratorParam* param, uint32* data, int size)
	/*@modifies param, data @*/;

/**
 * Cleanup after using a generator.
 *
 * @param param		generator parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*randomGeneratorCleanup) (randomGeneratorParam* param)
	/*@modifies param @*/;

/**
 * The struct 'randomGenerator' holds information and pointers to code specific
 * to each random generator. Each specific random generator MUST be written to
 * be multithread safe.
 *
 * WARNING: each randomGenerator, when used in cryptographic applications, MUST
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
	const char* name;
	const unsigned int paramsize;
	const randomGeneratorSetup	setup;
	const randomGeneratorSeed	seed;
	const randomGeneratorNext	next;
	const randomGeneratorCleanup	cleanup;
} randomGenerator;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the number of generators available.
 * @return		number of generators available
 */
BEEDLLAPI
int randomGeneratorCount(void)
	/*@*/;

/**
 * Retrieve a generator by index.
 * @param index		generator index
 * @return		generator pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const randomGenerator* randomGeneratorGet(int index)
	/*@*/;

/**
 * Retrieve a generator by name.
 * @param name		generator name
 * @return		generator pointer (or NULL)
 */
BEEDLLAPI
const randomGenerator* randomGeneratorFind(const char* name)
	/*@*/;

/**
 * Retrieve the default generator.
 * If the BEECRYPT_RANDOM environment variable is set, use that
 * generator. Otherwise, use "fips186prng".
 * @return		generator pointer
 */
BEEDLLAPI
const randomGenerator* randomGeneratorDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/**
 * The struct 'randomGeneratorContext' is used to contain both the functional
 * part (the randomGenerator), and its parameters.
 */

typedef struct
{
	const randomGenerator* rng;
	randomGeneratorParam* param;
} randomGeneratorContext;

/**
 * The following functions can be used to initialize and free a
 * randomGeneratorContext. Initializing will allocate a buffer of the size
 * required by the randomGenerator, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int randomGeneratorContextInit(randomGeneratorContext*, const randomGenerator*)
	/*@*/;
BEEDLLAPI
int randomGeneratorContextFree(randomGeneratorContext*)
	/*@*/;

#ifdef __cplusplus
}
#endif

/*
 * Hash Functions
 */

typedef void hashFunctionParam;

/**
 * Re-initialize the parameters of the hash function.
 *
 * @param param		hash parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*hashFunctionReset) (hashFunctionParam* param)
	/*@modifies param @*/;

/**
 * Update the hash function with an array of bytes.
 *
 * @param param		hash parameters
 * @param data		array of bytes
 * @param size		no. of bytes
 * @return		0 on success, -1 on failure
 */
typedef int (*hashFunctionUpdate) (hashFunctionParam* param, const byte* data, int size)
	/*@modifies param @*/;

/**
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
typedef int (*hashFunctionDigest) (hashFunctionParam* param, uint32* data)
	/*@modifies param, data @*/;

/**
 * The struct 'hashFunction' holds information and pointers to code specific
 * to each hash function. Specific hash functions MAY be written to be
 * multithread-safe.
 *
 */

typedef struct
{
	const char* name;
	const unsigned int paramsize;	/*!< in bytes */
	const unsigned int blocksize;	/*!< in bytes */
	const unsigned int digestsize;	/*!< in bytes */
	const hashFunctionReset		reset;
	const hashFunctionUpdate	update;
	const hashFunctionDigest	digest;
} hashFunction;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the number of hash functions available.
 * @return		number of hash functions available
 */
BEEDLLAPI
int hashFunctionCount(void)
	/*@*/;

/**
 * Retrieve a hash function by index.
 * @param index		hash function index
 * @return		hash function pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const hashFunction* hashFunctionGet(int index)
	/*@*/;

/**
 * Retrieve a hash function by name.
 * @param name		hash function name
 * @return		hash function pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const hashFunction* hashFunctionFind(const char* name)
	/*@*/;

/**
 * Retrieve the default hash function.
 * If the BEECRYPT_HASH environment variable is set, use that
 * hash function. Otherwise, use "sha1".
 * @return		hash function pointer
 */
BEEDLLAPI
const hashFunction* hashFunctionDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/**
 * The struct 'hashFunctionContext' is used to contain both the functional
 * part (the hashFunction), and its parameters.
 */

typedef struct
{
/*@unused@*/	const hashFunction* algo;
/*@unused@*/	hashFunctionParam* param;
} hashFunctionContext;

/**
 * The following functions can be used to initialize and free a
 * hashFunctionContext. Initializing will allocate a buffer of the size
 * required by the hashFunction, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int hashFunctionContextInit(hashFunctionContext* ctxt, const hashFunction*)
	/*@modifies ctxt */;
BEEDLLAPI
int hashFunctionContextFree(hashFunctionContext* ctxt)
	/*@modifies ctxt */;
BEEDLLAPI
int hashFunctionContextReset(hashFunctionContext* ctxt)
	/*@modifies ctxt */;
BEEDLLAPI
int hashFunctionContextUpdate(hashFunctionContext* ctxt, const byte*, int)
	/*@modifies ctxt */;
BEEDLLAPI
int hashFunctionContextUpdateMC(hashFunctionContext* ctxt, const memchunk*)
	/*@modifies ctxt */;
BEEDLLAPI
int hashFunctionContextUpdateMP32(hashFunctionContext* ctxt, const mp32number*)
	/*@modifies ctxt */;
BEEDLLAPI
int hashFunctionContextDigest(hashFunctionContext* ctxt, mp32number*)
	/*@modifies ctxt */;
BEEDLLAPI
int hashFunctionContextDigestMatch(hashFunctionContext* ctxt, const mp32number*)
	/*@modifies ctxt */;

#ifdef __cplusplus
}
#endif

/**
 * Keyed Hash Functions, a.k.a. Message Authentication Codes
 */

typedef void keyedHashFunctionParam;

/**
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
	/*@modifies param @*/;

/**
 * Re-initialize the parameters of a keyed hash function.
 *
 * @param param		keyed hash parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*keyedHashFunctionReset) (keyedHashFunctionParam* param)
	/*@modifies param @*/;

/**
 * Update the keyed hash function with an array of bytes.
 *
 * @param param		keyed hash parameters
 * @param data		array of bytes
 * @param size		no. of bytes
 * @return		0 on success, -1 on failure
 *
 */
typedef int (*keyedHashFunctionUpdate) (keyedHashFunctionParam* param, const byte* data, int size)
	/*@modifies param @*/;

/**
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
	/*@modifies param, data @*/;

/**
 * The struct 'keyedHashFunction' holds information and pointers to code
 * specific to each keyed hash function. Specific keyed hash functions MAY be
 * written to be multithread-safe.
 *
 */

typedef struct
{
	const char* name;
	const unsigned int paramsize;	/*!< in bytes */
	const unsigned int blocksize;	/*!< in bytes */
	const unsigned int digestsize;	/*!< in bytes */
	const unsigned int keybitsmin;	/*!< min keysize in bits */
	const unsigned int keybitsmax;	/*!< max keysize in bits */
	const unsigned int keybitsinc;	/*!< keysize increment in bits */
	const keyedHashFunctionSetup	setup;
	const keyedHashFunctionReset	reset;
	const keyedHashFunctionUpdate	update;
	const keyedHashFunctionDigest	digest;
} keyedHashFunction;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the number of keyed hash functions available.
 * @return		number of keyed hash functions available
 */
BEEDLLAPI
int keyedHashFunctionCount(void)
	/*@*/;

/**
 * Retrieve a keyed hash function by index.
 * @param index		keyed hash function index
 * @return		keyed hash function pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const keyedHashFunction* keyedHashFunctionGet(int index)
	/*@*/;

/**
 * Retrieve a keyed hash function by name.
 * @param name		keyed hash function name
 * @return		keyed hash function pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const keyedHashFunction* keyedHashFunctionFind(const char* name)
	/*@*/;

/**
 * Retrieve the default keyed hash function.
 * If the BEECRYPT_KEYEDHASH environment variable is set, use that keyed
 * hash function. Otherwise, use "hmacsha1".
 * @return		keyed hash function pointer
 */
BEEDLLAPI
const keyedHashFunction* keyedHashFunctionDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/**
 * The struct 'keyedHashFunctionContext' is used to contain both the functional
 * part (the keyedHashFunction), and its parameters.
 */

typedef struct
{
/*@unused@*/	const keyedHashFunction*	algo;
/*@unused@*/	keyedHashFunctionParam*		param;
} keyedHashFunctionContext;

/**
 * The following functions can be used to initialize and free a
 * keyedHashFunctionContext. Initializing will allocate a buffer of the size
 * required by the keyedHashFunction, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int keyedHashFunctionContextInit(keyedHashFunctionContext*, const keyedHashFunction*)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextFree(keyedHashFunctionContext*)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextSetup(keyedHashFunctionContext*, const uint32*, int)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextReset(keyedHashFunctionContext*)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextUpdate(keyedHashFunctionContext*, const byte*, int)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextUpdateMC(keyedHashFunctionContext*, const memchunk*)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextUpdateMP32(keyedHashFunctionContext*, const mp32number*)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextDigest(keyedHashFunctionContext*, mp32number*)
	/*@*/;
BEEDLLAPI
int keyedHashFunctionContextDigestMatch(keyedHashFunctionContext*, const mp32number*)
	/*@*/;

#ifdef __cplusplus
}
#endif

/**
 * Block cipher operations.
 */
typedef enum
{
	ENCRYPT,
	DECRYPT
} cipherOperation;

/**
 * Block cipher modes.
 */
typedef enum
{
	/*@-enummemuse@*/
	ECB,
	/*@=enummemuse@*/
	CBC
} cipherMode;

typedef void blockCipherParam;

typedef int (*blockModeEncrypt)(blockCipherParam*, int, uint32*, const uint32*)
	/*@*/;
typedef int (*blockModeDecrypt)(blockCipherParam*, int, uint32*, const uint32*)
	/*@*/;

typedef struct
{
	const blockModeEncrypt	encrypt;
	const blockModeDecrypt	decrypt;
} blockMode;

/**
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
typedef int (*blockCipherSetup  )(blockCipherParam* param, const uint32* key, int keybits, cipherOperation cipherOperation)
	/*@*/;

/**
 * @param param		blockcipher parameters
 * @return		0 on success, -1 on failure
 */
typedef int (*blockCipherSetIV  )(blockCipherParam* param, const uint32* data)
	/*@*/;

/**
 * Encrypt one block of data (with bit size chosen by the blockcipher).
 * @note This is raw encryption, without padding, etc.
 *
 * @param param		blockcipher parameters
 * @retval dst		ciphertext block
 * @param src		plaintext block
 * @return		0 on success, -1 on failure
 */
typedef int (*blockCipherEncrypt)(blockCipherParam* param, uint32* dst, const uint32* src)
	/*@*/;

/**
 * Decrypt one block of data (with bit size chosen by the blockcipher).
 * @note This is raw decryption, without padding, etc.
 *
 * @param param		blockcipher parameters
 * @retval dst		plaintext block
 * @param src		ciphertext block
 * @return		0 on success, -1 on failure
 */
typedef int (*blockCipherDecrypt)(blockCipherParam* param, uint32* dst, const uint32* src)
	/*@*/;

/**
 * The struct 'blockCipher' holds information and pointers to code specific
 * to each blockcipher. Specific block ciphers MAY be written to be
 * multithread-safe.
 */
 
typedef struct
{
	const char* name;
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

/**
 * Return the number of blockciphers available.
 * @return		number of blockciphers available
 */
BEEDLLAPI
int blockCipherCount(void)
	/*@*/;

/**
 * Retrieve a blockcipher by index.
 * @param index		blockcipher index
 * @return		blockcipher pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const blockCipher* blockCipherGet(int index)
	/*@*/;

/**
 * Retrieve a blockcipher by name.
 * @param name		blockcipher name
 * @return		blockcipher pointer (or NULL)
 */
BEEDLLAPI /*@null@*/
const blockCipher* blockCipherFind(const char* name)
	/*@*/;

/**
 * Retrieve the default blockcipher.
 * If the BEECRYPT_CIPHER environment variable is set, use that blockcipher.
 * Otherwise, use "blowfish".
 * @return		blockcipher pointer
 */
BEEDLLAPI
const blockCipher* blockCipherDefault(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

/**
 * The struct 'blockCipherContext' is used to contain both the functional
 * part (the blockCipher), and its parameters.
 */

typedef struct
{
	const blockCipher* algo;
	blockCipherParam* param;
} blockCipherContext;

/**
 * The following functions can be used to initialize and free a
 * blockCipherContext. Initializing will allocate a buffer of the size
 * required by the blockCipher, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int blockCipherContextInit(blockCipherContext*, const blockCipher*)
	/*@*/;
BEEDLLAPI
int blockCipherContextSetup(blockCipherContext*, const uint32*, int, cipherOperation)
	/*@*/;
BEEDLLAPI
int blockCipherContextSetIV(blockCipherContext*, const uint32*)
	/*@*/;
BEEDLLAPI
int blockCipherContextFree(blockCipherContext*)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif
