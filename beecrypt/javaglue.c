/** \ingroup JAVA_m
 * \file javaglue.c
 */

#define BEECRYPT_DLL_EXPORT

#define	JNIEXPORT /*@unused@*/
#define	JNICALL

#include "system.h"
#include "beecrypt.h"
#include "blockmode.h"

#undef	JAVAGLUE
#define	JAVAGLUE	0	/* XXX disable for now */

#if JAVAGLUE

#include "javaglue.h"
#include "debug.h"

/* For now, I'm lazy ... */
/*@-nullpass -nullret -shiftsigned -usedef -temptrans -freshtrans @*/
/*@-noeffectuncon -globs -globnoglobs -modunconnomods -modnomods @*/
/*@-mustfree@*/

#ifndef WORDS_BIGENDIAN
# define WORDS_BIGENDIAN	0
#endif

/*@observer@*/
static const char* JAVA_OUT_OF_MEMORY_ERROR = "java/lang/OutOfMemoryError";
/*@observer@*/
static const char* JAVA_PROVIDER_EXCEPTION = "java/security/ProviderException";
/*@observer@*/
static const char* JAVA_INVALID_KEY_EXCEPTION = "java/security/InvalidKeyException";
/*@observer@*/
static const char* MSG_OUT_OF_MEMORY = "out of memory";
/*@observer@*/
static const char* MSG_NO_SUCH_ALGORITHM = "algorithm not available";
/*@observer@*/
static const char* MSG_NO_ENTROPY_SOURCE = "no entropy source";
/*@observer@*/
static const char* MSG_INVALID_KEY = "invalid key";

/* NativeMessageDigest */

jlong JNICALL Java_beecrypt_security_NativeMessageDigest_find(JNIEnv* env, /*@unused@*/ jclass dummy, jstring algorithm)
{
	const char* name = (*env)->GetStringUTFChars(env, algorithm, (jboolean*) 0);
	const hashFunction* hash = hashFunctionFind(name);
	(*env)->ReleaseStringUTFChars(env, algorithm, name);
	if (hash == (hashFunction*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
		if (ex != (jclass) 0)
			(void) (*env)->ThrowNew(env, ex, MSG_NO_SUCH_ALGORITHM);
	}
	return (jlong) hash;
}

jlong JNICALL Java_beecrypt_security_NativeMessageDigest_allocParam(JNIEnv* env, /*@unused@*/ jclass dummy, jlong hash)
{
	void *param = malloc(((const hashFunction*) hash)->paramsize);
	if (param == (void*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex != (jclass) 0)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
	}
	return (jlong) param;
}

jlong JNICALL Java_beecrypt_security_NativeMessageDigest_cloneParam(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong hash, jlong param)
{
	unsigned int paramsize = ((const hashFunction*) hash)->paramsize;
	void *clone = malloc(paramsize);
	memcpy(clone, (void*) param, paramsize);
	return (jlong) clone;
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_freeParam(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong param)
{
	if (param)
		free((void*) param);
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_reset(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong hash, jlong param)
{
	(void) ((const hashFunction*) hash)->reset((hashFunctionParam*) param);
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_update(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong hash, jlong param, jbyte input)
{
	(void) ((const hashFunction*) hash)->update((hashFunctionParam*) param, (const byte*) &input, 1);
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_updateBlock(JNIEnv* env, /*@unused@*/ jclass dummy, jlong hash, jlong param, jbyteArray input, jint offset, jint len)
{
	jbyte* data = (*env)->GetByteArrayElements(env, input, (jboolean*) 0);
	if (data == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, input, data, JNI_ABORT);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	(void) ((const hashFunction*) hash)->update((hashFunctionParam*) param, (const byte*) data+offset, len);
	(*env)->ReleaseByteArrayElements(env, input, data, JNI_ABORT);
}

jbyteArray JNICALL Java_beecrypt_security_NativeMessageDigest_digest(JNIEnv* env, /*@unused@*/ jclass dummy, jlong hash, jlong param)
{
	jbyteArray digestArray;
	jbyte* digest;

	int digestsize = (jsize) ((const hashFunction*) hash)->digestsize;
	int digestwords = digestsize >> 2;

	digestArray = (*env)->NewByteArray(env, digestsize);
	digest = (*env)->GetByteArrayElements(env, digestArray, (jboolean*) 0);

	if (digest == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return (jbyteArray) 0;
	}

	if (!WORDS_BIGENDIAN || (int) digest & 0x3)
	{	/* unaligned, or swap necessary */
		uint32* data = (uint32*) malloc(digestwords * sizeof(uint32));

		if (data == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, digestArray, digest, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return (jbyteArray) 0;
		}

		(void) ((const hashFunction*) hash)->digest((hashFunctionParam*) param, data);
		(void) encodeInts((const javaint*) data, digest, digestwords);
		free(data);
	}
	else
	{	/* aligned */
		(void) ((const hashFunction*) hash)->digest((hashFunctionParam*) param, (uint32*) digest);
	}

	(*env)->ReleaseByteArrayElements(env, digestArray, digest, 0);

	return digestArray;
}

jint JNICALL Java_beecrypt_security_NativeMessageDigest_digestLength(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong hash)
{
	return (jint) ((const hashFunction*) hash)->digestsize;
}

/* NativeSecureRandom */

jlong JNICALL Java_beecrypt_security_NativeSecureRandom_find(JNIEnv* env, /*@unused@*/ jclass dummy, jstring algorithm)
{
	const char* name = (*env)->GetStringUTFChars(env, algorithm, (jboolean*) 0);
	const randomGenerator* prng = randomGeneratorFind(name);
	(*env)->ReleaseStringUTFChars(env, algorithm, name);
	if (prng == (randomGenerator*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_NO_SUCH_ALGORITHM);
	}
	return (jlong) prng;
}

jlong JNICALL Java_beecrypt_security_NativeSecureRandom_allocParam(JNIEnv* env, /*@unused@*/ jclass dummy, jlong prng)
{
	void *param = malloc(((const randomGenerator*) prng)->paramsize);
	if (param == (void*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
	}
	return (jlong) param;
}

jlong JNICALL Java_beecrypt_security_NativeSecureRandom_cloneParam(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong prng, jlong param)
{
	unsigned int paramsize = ((const randomGenerator*) prng)->paramsize;
	void *clone = malloc(paramsize);
	memcpy(clone, (void*) param, paramsize);
	return (jlong) clone;
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_freeParam(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong param)
{
	if (param)
		free((void*) param);
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_setup(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong prng, jlong param)
{
	(void) ((const randomGenerator*) prng)->setup((randomGeneratorParam*) param);
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_setSeed(JNIEnv* env, /*@unused@*/ jclass dummy, jlong prng, jlong param, jbyteArray seedArray)
{
	/* BeeCrypt takes size in words */
	jsize seedSize = (*env)->GetArrayLength(env, seedArray);
	if (seedSize)
	{
		jbyte* seed = (*env)->GetByteArrayElements(env, seedArray, (jboolean*) 0);
		if (seed == (jbyte*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		/* check memory alignment of seed and seedSize */
		if (((int) seed & 0x3) || (seedSize & 0x3))
		{	/* unaligned */
			int size = (seedSize+3) >> 2;
			uint32* data = (uint32*) malloc(size * sizeof(uint32));

			if (data == (uint32*) 0)
			{
				jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
				(*env)->ReleaseByteArrayElements(env, seedArray, seed, JNI_ABORT);
				if (ex)
					(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
				return;
			}
			(void) decodeIntsPartial(data, seed, seedSize);
			(void) ((const randomGenerator*) prng)->seed((randomGeneratorParam*) param, data, size);
			free(data);
		}
		else
		{	/* aligned and properly sized */
			(void) ((const randomGenerator*) prng)->seed((randomGeneratorParam*) param, (uint32*) seed, seedSize >> 2);
		}

		(*env)->ReleaseByteArrayElements(env, seedArray, seed, JNI_ABORT);
	}
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_nextBytes(JNIEnv* env, /*@unused@*/ jclass dummy, jlong prng, jlong param, jbyteArray bytesArray)
{
	/* BeeCrypt takes size in words */
	jsize bytesSize = (*env)->GetArrayLength(env, bytesArray);
	if (bytesSize)
	{
		jbyte* bytes = (*env)->GetByteArrayElements(env, bytesArray, (jboolean*) 0);
		if (bytes == (jbyte*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}

		/* check memory alignment of bytes and bytesSize */
		if (((int) bytes & 0x3) || (bytesSize & 0x3))
		{	/* unaligned */
			int size = (bytesSize+3) >> 2;
			uint32* data = (uint32*) malloc(size * sizeof(uint32));

			if (data == (uint32*) 0)
			{
				jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
				(*env)->ReleaseByteArrayElements(env, bytesArray, bytes, JNI_ABORT);
				if (ex)
					(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
				return;
			}

			(void) ((const randomGenerator*) prng)->next((randomGeneratorParam*) param, data, size);
			memcpy(bytes, data, bytesSize);
			free(data);
		}
		else
		{	/* aligned and properly sized */
			(void) ((const randomGenerator*) prng)->next((randomGeneratorParam*) param, (uint32*) bytes, bytesSize >> 2);
		}

		(*env)->ReleaseByteArrayElements(env, bytesArray, bytes, 0);
	}
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_generateSeed(JNIEnv* env, /*@unused@*/ jclass dummy, jbyteArray seedArray)
{
	/* BeeCrypt takes size in words */
	jsize seedSize = (*env)->GetArrayLength(env, seedArray);

	if (seedSize)
	{
		jbyte* seed = (*env)->GetByteArrayElements(env, seedArray, (jboolean*) 0);

		const entropySource* ents = entropySourceDefault();
	
		if (seed == (jbyte*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}

		if (ents == (entropySource*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
			(*env)->ReleaseByteArrayElements(env, seedArray, seed, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_NO_ENTROPY_SOURCE);
			return;
		}

		/* check memory alignment of seed and seedSize */
		if (((int) seed & 0x3) || (seedSize & 0x3))
		{	/* unaligned */
			int size = (seedSize+3) >> 2;
			uint32* data = (uint32*) malloc(size * sizeof(uint32));
			(void) ents->next(data, size);
			memcpy(seed, data, seedSize);
			free(data);
		}
		else
		{	/* aligned */
			(void) ents->next((uint32*) seed, seedSize >> 2);
		}

		(*env)->ReleaseByteArrayElements(env, seedArray, seed, 0);
	}
}

/* NativeBlockCipher */

jlong JNICALL Java_beecrypt_crypto_NativeBlockCipher_find(JNIEnv* env, /*@unused@*/ jclass dummy, jstring algorithm)
{
	const char* name = (*env)->GetStringUTFChars(env, algorithm, (jboolean*) 0);
	const blockCipher* ciph = blockCipherFind(name);
	(*env)->ReleaseStringUTFChars(env, algorithm, name);
	if (ciph == (blockCipher*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_NO_SUCH_ALGORITHM);
	}
	return (jlong) ciph;
}

jlong JNICALL Java_beecrypt_crypto_NativeBlockCipher_allocParam(JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph)
{
	void *param = malloc(((const blockCipher*) ciph)->paramsize);
	if (param == (void*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
	}
	return (jlong) param;
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_freeParam(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong param)
{
	if (param)
		free((void*) param);
}

jint JNICALL Java_beecrypt_crypto_NativeBlockCipher_getBlockSize(/*@unused@*/ JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph)
{
	return ((const blockCipher*) ciph)->blocksize;
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_setup(JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph, jlong param, jint mode, jbyteArray keyArray)
{
	/* BeeCrypt takes key in 32 bit words with size in bits */
	jsize keysize = (*env)->GetArrayLength(env, keyArray);

	if (keysize)
	{
		int rc;
		cipherOperation nativeop;
		jbyte* key;

		switch (mode)
		{
		case javax_crypto_Cipher_ENCRYPT_MODE:
			nativeop = ENCRYPT;
			break;
		case javax_crypto_Cipher_DECRYPT_MODE:
			nativeop = DECRYPT;
			break;
		}
		
		key = (*env)->GetByteArrayElements(env, keyArray, (jboolean*) 0);
		if (key == (jbyte*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}

		if (!WORDS_BIGENDIAN || ((int) key & 0x3) || (keysize & 0x3))
		{	/* unaligned */
			int size = (keysize + 3) >> 2;
			uint32* data = (uint32*) malloc(size * sizeof(uint32));

			if (data == (uint32*) 0)
			{
				jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
				(*env)->ReleaseByteArrayElements(env, keyArray, key, JNI_ABORT);
				if (ex)
					(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
				return;
			}

			(void) decodeIntsPartial(data, key, keysize);
			rc = ((const blockCipher*) ciph)->setup((blockCipherParam*) param, data, keysize << 3, nativeop);
			free(data);
		}
		else
		{	/* aligned and properly sized */
			rc = ((const blockCipher*) ciph)->setup((blockCipherParam*) param, (const uint32*) key, keysize << 3, nativeop);
		}

		if (rc != 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_INVALID_KEY_EXCEPTION);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_INVALID_KEY);
		}

		(*env)->ReleaseByteArrayElements(env, keyArray, key, JNI_ABORT);
	}
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_setIV(JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph, jlong param, jbyteArray ivArray)
{
	if (ivArray == (jbyteArray) 0)
	{
		(void) ((const blockCipher*) ciph)->setiv((blockCipherParam*) param, 0);
	}
	else
	{
		jsize ivsize = (*env)->GetArrayLength(env, ivArray);

		if (ivsize > 0)
		{
			jbyte* iv = (*env)->GetByteArrayElements(env, ivArray, (jboolean*) 0);
		
			if (iv == (jbyte*) 0)
			{
				jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
				if (ex)
					(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
				return;
			}
		
			if (!WORDS_BIGENDIAN || ((int) iv & 0x3) || (ivsize & 0x3))
			{	/* unaligned */
				int size = (ivsize + 3) >> 2;
				uint32* data = (uint32*) malloc(size * sizeof(uint32));

				if (data == (uint32*) 0)
				{
					jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
					(*env)->ReleaseByteArrayElements(env, ivArray, iv, JNI_ABORT);
					if (ex)
						(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
					return;
				}

				(void) decodeIntsPartial(data, iv, ivsize);
				(void) ((const blockCipher*) ciph)->setiv((blockCipherParam*) param, data);
				free(data);
			}
			else
			{	/* aligned */
				(void) ((const blockCipher*) ciph)->setiv((blockCipherParam*) param, (uint32*) iv);
			}
			(*env)->ReleaseByteArrayElements(env, ivArray, iv, JNI_ABORT);
		}
	}
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_encryptECB(JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32* datain;
		uint32* dataout;

		datain = (uint32*) malloc(blocks * sizeof(uint32));
		if (datain == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32*) malloc(blocks * sizeof(uint32));
		if (dataout == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32));
		(void) blockEncrypt((const blockCipher*) ciph, (blockCipherParam*) param, ECB, blocks, dataout, datain);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32));
	}
	else
	{	/* aligned */
		(void) blockEncrypt((const blockCipher*) ciph, (blockCipherParam*) param, ECB, blocks, (uint32*)(output+outputOffset), (uint32*) (input+inputOffset));
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_decryptECB(JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32* datain;
		uint32* dataout;

		datain = (uint32*) malloc(blocks * sizeof(uint32));
		if (datain == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32*) malloc(blocks * sizeof(uint32));
		if (dataout == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32));
		(void) blockDecrypt((const blockCipher*) ciph, (blockCipherParam*) param, ECB, blocks, dataout, datain);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32));
	}
	else
	{	/* aligned */
		(void) blockDecrypt((const blockCipher*) ciph, (blockCipherParam*) param, ECB, blocks, (uint32*)(output+outputOffset), (uint32*) (input+inputOffset));
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_encryptCBC(JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32* datain;
		uint32* dataout;

		datain = (uint32*) malloc(blocks * sizeof(uint32));
		if (datain == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32*) malloc(blocks * sizeof(uint32));
		if (dataout == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32));
		(void) blockEncrypt((const blockCipher*) ciph, (blockCipherParam*) param, CBC, blocks, dataout, datain);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32));
	}
	else
	{	/* aligned */
		(void) blockEncrypt((const blockCipher*) ciph, (blockCipherParam*) param, CBC, blocks, (uint32*)(output+outputOffset), (uint32*) (input+inputOffset));
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_decryptCBC(JNIEnv* env, /*@unused@*/ jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(void) (void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32* datain;
		uint32* dataout;

		datain = (uint32*) malloc(blocks * sizeof(uint32));
		if (datain == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32*) malloc(blocks * sizeof(uint32));
		if (dataout == (uint32*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(void) (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32));
		(void) blockDecrypt((const blockCipher*) ciph, (blockCipherParam*) param, CBC, blocks, dataout, datain);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32));
	}
	else
	{	/* aligned */
		(void) blockDecrypt((const blockCipher*) ciph, (blockCipherParam*) param, CBC, blocks, (uint32*)(output+outputOffset), (uint32*) (input+inputOffset));
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

/*@=mustfree@*/
/*@=noeffectuncon =globs =globnoglobs =modunconnomods =modnomods @*/
/*@=nullpass =nullret =shiftsigned =usedef =temptrans =freshtrans @*/

#endif
