/*
 * beecrypt.h
 *
 * Beecrypt library hooks & stubs, header
 *
 * Copyright (c) 1999-2000 Virtual Unlimited B.V.
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
 */

#ifndef _BEECRYPT_H
#define _BEECRYPT_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

typedef struct
{
	int		size;
	byte*	data;
} memchunk;

/*
 * Entropy Sources
 */

typedef int (*entropyNext)(uint32*, int);

/*
 * The struct 'entropySource' holds information and pointers to code specific
 * to each entropy source. Each specific entropy source MUST be written to be
 * multithread-safe.
 *
 * The struct contains the following function(s):
 *
 * int (*next)(uint32* data, int size);
 *
 * This function will fill an array of 32-bit unsigned integers of given size
 * with entropy.
 * Return value is 0 on success, or -1 on failure.
 */

typedef struct
{
	const char*			name;
	const entropyNext	next;
} entropySource;

/*
 * You can use the following functions to find entropy sources implemented by
 * the library:
 *
 * entropySourceCount returns the number of sources available.
 *
 * entropySourceGet returns the entropy source with a given index (starting
 * at zero, up to entropySourceCount() - 1), or NULL if the index was out of
 * bounds.
 *
 * entropySourceFind returns the entropy source with the given name, or NULL
 * if no entropy source exists with that name.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int						entropySourceCount();
BEEDLLAPI
const entropySource*	entropySourceGet(int);
BEEDLLAPI
const entropySource*	entropySourceFind(const char*);
BEEDLLAPI
const entropySource*	entropySourceDefault();

#ifdef __cplusplus
}
#endif

/*
 * Pseudo-random Number Generators
 */

typedef void randomGeneratorParam;

typedef int (*randomGeneratorSetup  )(randomGeneratorParam*);
typedef int (*randomGeneratorSeed   )(randomGeneratorParam*, const uint32*, int);
typedef int (*randomGeneratorNext   )(randomGeneratorParam*, uint32*, int);
typedef int (*randomGeneratorCleanup)(randomGeneratorParam*);

/*
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
 * The struct contains the following function(s):
 *
 * int (*setup)(randomGeneratorParam* param);
 *
 * This function will initialize the parameters for use, and seed the generator
 * with entropy from the default entropy source.
 * Return value is 0 on success, or -1 on failure.
 *
 * int (*seed)(randomGeneratorParam* param, const uint32* data, int size);
 *
 * This function reseeds the random generator with user-provided entropy.
 * Return value is 0 on success, or -1 on failure.
 *
 * int (*next)(randomGeneratorParam* param, uint32* data, int size);
 *
 * This function will fill an array of 32-bit unsigned integers of given size
 * with pseudo-random data.
 * Return value is 0 on success, or -1 on failure.
 *
 * int (*cleanup)(randomGeneratorParam* param);
 *
 * This function will cleanup after the use of a generator
 * Return value is 0 on success, or -1 on failure. 
 */

typedef struct
{
	const char*						name;
	const int						paramsize;
	const randomGeneratorSetup		setup;
	const randomGeneratorSeed		seed;
	const randomGeneratorNext		next;
	const randomGeneratorCleanup	cleanup;
} randomGenerator;

/*
 * You can use the following functions to find random generators implemented by
 * the library:
 *
 * randomGeneratorCount returns the number of generators available.
 *
 * randomGeneratorGet returns the random generator with a given index (starting
 * at zero, up to randomGeneratorCount() - 1), or NULL if the index was out of
 * bounds.
 *
 * randomGeneratorFind returns the random generator with the given name, or
 * NULL if no random generator exists with that name.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int						randomGeneratorCount();
BEEDLLAPI
const randomGenerator*	randomGeneratorGet(int);
BEEDLLAPI
const randomGenerator*	randomGeneratorFind(const char*);
BEEDLLAPI
const randomGenerator*	randomGeneratorDefault();

#ifdef __cplusplus
}
#endif

/*
 * The struct 'randomGeneratorContext' is used to contain both the functional
 * part (the randomGenerator), and its parameters.
 */

typedef struct
{
	const randomGenerator* rng;
	randomGeneratorParam* param;
} randomGeneratorContext;

/*
 * The following functions can be used to initialize and free a
 * randomGeneratorContext. Initializing will allocate a buffer of the size
 * required by the randomGenerator, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void randomGeneratorContextInit(randomGeneratorContext*, const randomGenerator*);
BEEDLLAPI
void randomGeneratorContextFree(randomGeneratorContext*);

#ifdef __cplusplus
}
#endif

/*
 * Hash Functions
 */

typedef void hashFunctionParam;

typedef int (*hashFunctionReset )(hashFunctionParam*);
typedef int (*hashFunctionUpdate)(hashFunctionParam*, const byte*, int);
typedef int (*hashFunctionDigest)(hashFunctionParam*, uint32*);

/*
 * The struct 'hashFunction' holds information and pointers to code specific
 * to each hash function. Specific hash functions MAY be written to be
 * multithread-safe.
 *
 * The struct contains the following function(s):
 *
 * int (*reset)(hashFunctionParam* param);
 *
 * This function will re-initialize the parameters of this hash function.
 * Return value is 0 on success, or -1 on failure.
 *
 * int (*update)(hashFunctionParam* param, const byte* data, int size);
 *
 * This function updates the hash function with an array of bytes.
 * Return value is 0 on success, or -1 on failure.
 *
 * int (*digest)(hashFunctionParam* param, uint32* data);
 *
 * This function computes the digest of all the data passed to the hash
 * function, and stores the result in data.
 * Return value is 0 on success, or -1 on failure.
 * NOTE: data must be at least have a bytesize of 'digestsize' as described
 * in the hashFunction struct.
 * NOTE: for safety reasons, after calling digest, each specific implementation
 * MUST reset itself so that previous values in the parameters are erased.
 */

typedef struct
{
	const char*					name;
	const int					paramsize;	/* in bytes */
	const int					digestsize;	/* in bytes */
	const hashFunctionReset		reset;
	const hashFunctionUpdate	update;
	const hashFunctionDigest	digest;
} hashFunction;

/*
 * You can use the following functions to find hash functions implemented by
 * the library:
 *
 * hashFunctionCount returns the number of hash functions available.
 *
 * hashFunctionGet returns the hash function with a given index (starting
 * at zero, up to hashFunctionCount() - 1), or NULL if the index was out of
 * bounds.
 *
 * hashFunctionFind returns the hash function with the given name, or
 * NULL if no hash function exists with that name.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int					hashFunctionCount();
BEEDLLAPI
const hashFunction*	hashFunctionGet(int);
BEEDLLAPI
const hashFunction*	hashFunctionFind(const char*);
BEEDLLAPI
const hashFunction*	hashFunctionDefault();

#ifdef __cplusplus
}
#endif

/*
 * The struct 'hashFunctionContext' is used to contain both the functional
 * part (the hashFunction), and its parameters.
 */

typedef struct
{
	const hashFunction* hash;
	hashFunctionParam* param;
} hashFunctionContext;

/*
 * The following functions can be used to initialize and free a
 * hashFunctionContext. Initializing will allocate a buffer of the size
 * required by the hashFunction, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void hashFunctionContextInit(hashFunctionContext*, const hashFunction*);
BEEDLLAPI
void hashFunctionContextFree(hashFunctionContext*);

#ifdef __cplusplus
}
#endif

/*
 * Keyed Hash Functions, a.k.a. Message Authentication Codes
 */

typedef void keyedHashFunctionParam;

typedef int (*keyedHashFunctionSetup  )(keyedHashFunctionParam*, const uint32*, int);
typedef int (*keyedHashFunctionReset  )(keyedHashFunctionParam*);
typedef int (*keyedHashFunctionUpdate )(keyedHashFunctionParam*, const byte*, int);
typedef int (*keyedHashFunctionDigest )(keyedHashFunctionParam*, uint32*);

/*
 * The struct 'keyedHashFunction' holds information and pointers to code
 * specific to each keyed hash function. Specific keyed hash functions MAY be
 * written to be multithread-safe.
 *
 * The struct field 'keybitsmin' contains the minimum number of bits a key
 * must contains, 'keybitsmax' the maximum number of bits a key may contain,
 * 'keybitsinc', the increment in bits that may be used between min and max.
 * 
 * The struct contains the following function(s):
 *
 * int (*setup)(keyedHashFunctionParam *param, const uint32* key, int keybits);
 *
 * This function will setup the keyed hash function parameters with the given
 * secret key; it will also 'reset' the parameters.
 * Return value is 0 on success, or -1 on failure.
 * NOTE: after use, it is recommended to wipe the parameters by calling setup
 * again with another (dummy) key.
 *
 * int (*reset)(keyedHashFunctionParam* param);
 *
 * This function will re-initialize the parameters of this keyed hash function.
 * Return value is 0 on success, or -1 on failure.
 *
 * int (*update)(keyedHashFunctionParam* param, const byte* data, int size);
 *
 * This function updates the keyed hash function with an array of bytes.
 * Return value is 0 on success, or -1 on failure.
 *
 * int (*digest)(keyedHashFunctionParam* param, uint32* data);
 *
 * This function computes the digest (or authentication code) of all the data
 * passed to the keyed hash function, and stores the result in data.
 * Return value is 0 on success, or -1 on failure.
 * NOTE: data must be at least have a bytesize of 'digestsize' as described
 * in the keyedHashFunction struct.
 * NOTE: for safety reasons, after calling digest, each specific implementation
 * MUST reset itself so that previous values in the parameters are erased.
 */

typedef struct
{
	const char*						name;
	const int						paramsize;	/* in bytes */
	const int						digestsize;	/* in bytes */
	const int						keybitsmin;	/* in bits */
	const int						keybitsmax;	/* in bits */
	const int						keybitsinc;	/* in bits */
	const keyedHashFunctionSetup	setup;
	const keyedHashFunctionReset	reset;
	const keyedHashFunctionUpdate	update;
	const keyedHashFunctionDigest	digest;
} keyedHashFunction;

/*
 * You can use the following functions to find keyed hash functions implemented
 * by the library:
 *
 * keyedHashFunctionCount returns the number of keyed hash functions available.
 *
 * keyedHashFunctionGet returns the random generator with a given index
 * (starting at zero, up to keyedHashFunctionCount() - 1), or NULL if the index
 * was out of bounds.
 *
 * keyedHashFunctionFind returns the keyed hash function with the given name,
 * or NULL if no keyed hash function exists with that name.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int							keyedHashFunctionCount();
BEEDLLAPI
const keyedHashFunction*	keyedHashFunctionGet(int);
BEEDLLAPI
const keyedHashFunction*	keyedHashFunctionFind(const char*);
BEEDLLAPI
const keyedHashFunction*	keyedHashFunctionDefault();

#ifdef __cplusplus
}
#endif

/*
 * The struct 'keyedHashFunctionContext' is used to contain both the functional
 * part (the keyedHashFunction), and its parameters.
 */

typedef struct
{
	const keyedHashFunction*	hash;
	keyedHashFunctionParam*		param;
} keyedHashFunctionContext;

/*
 * The following functions can be used to initialize and free a
 * keyedHashFunctionContext. Initializing will allocate a buffer of the size
 * required by the keyedHashFunction, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void keyedHashFunctionContextInit(keyedHashFunctionContext*, const keyedHashFunction*);
BEEDLLAPI
void keyedHashFunctionContextFree(keyedHashFunctionContext*);

#ifdef __cplusplus
}
#endif

/*
 * Block ciphers
 */

typedef enum
{
	ENCRYPT,
	DECRYPT
} cipherOperation;

typedef enum
{
	ECB,
	CBC
} cipherMode;

typedef void blockCipherParam;

typedef int (*blockModeEncrypt)(blockCipherParam*, int, uint32*, const uint32*, const uint32*);
typedef int (*blockModeDecrypt)(blockCipherParam*, int, uint32*, const uint32*, const uint32*);

typedef struct
{
	const blockModeEncrypt	encrypt;
	const blockModeDecrypt	decrypt;
} blockMode;

typedef int (*blockCipherSetup  )(blockCipherParam*, const uint32*, int, cipherOperation);
typedef int (*blockCipherEncrypt)(blockCipherParam*, uint32 *);
typedef int (*blockCipherDecrypt)(blockCipherParam*, uint32 *);

/*
 * The struct 'blockCipher' holds information and pointers to code specific
 * to each blockcipher. Specific block ciphers MAY be written to be
 * multithread-safe.
 *
 * The struct field 'keybitsmin' contains the minimum number of bits a key
 * must contains, 'keybitsmax' the maximum number of bits a key may contain,
 * 'keybitsinc', the increment in bits that may be used between min and max.
 *
 * The struct contains the following function(s):
 *
 * int (*setup)(blockCipherParam *param, const uint32* key, int keybits, cipherOperation);
 *
 * This function will setup the blockcipher parameters with the given secret
 * key for either encryption or decryption;
 * Return value is 0 on success, or -1 on failure.
 * NOTE: after use, it is recommended to wipe the parameters by calling setup
 * again with another (dummy) key.
 *
 * int (*encrypt)(blockCipherParam* param, uint32* block);
 *
 * This function will encrypt one block of data (with bit size equal to
 * 'blockbits')
 * Return value is 0 on success, or -1 on failure.
 * NOTE: this is raw encryption, without padding, etc.
 *
 * int (*decrypt)(blockCipherParam* param, uint32* block);
 *
 * This function will decrypt one block of data (with bit size equal to
 * 'blockbits')
 * Return value is 0 on success, or -1 on failure.
 * NOTE: this is raw decryption, without padding, etc.
 */
 
typedef struct
{
	const char*					name;
	const int					paramsize;	/* in bytes */
	const int					keybitsmin;	/* in bits */
	const int					keybitsmax;	/* in bits */
	const int					keybitsinc;	/* in bits */
	const int					blockbits;	/* in bits */
	const blockCipherSetup		setup;
	const blockCipherEncrypt	encrypt;
	const blockCipherDecrypt	decrypt;
	const blockMode*			mode;
} blockCipher;

/*
 * You can use the following functions to find blockciphers implemented by
 * the library:
 *
 * blockCipherCount returns the number of blockciphers available.
 *
 * blockCipherGet returns the blockcipher with a given index (starting
 * at zero, up to blockCipherCount() - 1), or NULL if the index was out of
 * bounds.
 *
 * blockCipherFind returns the blockcipher with the given name, or
 * NULL if no hash function exists with that name.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int						blockCipherCount();
BEEDLLAPI
const blockCipher*		blockCipherGet(int);
BEEDLLAPI
const blockCipher*		blockCipherFind(const char*);
BEEDLLAPI
const blockCipher*		blockCipherDefault();

#ifdef __cplusplus
}
#endif

/*
 * The struct 'blockCipherContext' is used to contain both the functional
 * part (the blockCipher), and its parameters.
 */

typedef struct
{
	const blockCipher* ciph;
	blockCipherParam* param;
} blockCipherContext;

/*
 * The following functions can be used to initialize and free a
 * blockCipherContext. Initializing will allocate a buffer of the size
 * required by the blockCipher, freeing will deallocate that buffer.
 */

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void blockCipherContextInit(blockCipherContext*, const blockCipher*);
BEEDLLAPI
void blockCipherContextFree(blockCipherContext*);

#ifdef __cplusplus
}
#endif

#endif
