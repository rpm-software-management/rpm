#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/beecrypt.h"
#include "beecrypt/blockmode.h"
#include "beecrypt/mpnumber.h"
#include "beecrypt/mpbarrett.h"

#if JAVAGLUE

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

#include "javaglue.h"

static const char* JAVA_OUT_OF_MEMORY_ERROR = "java/lang/OutOfMemoryError";
static const char* JAVA_PROVIDER_EXCEPTION = "java/security/ProviderException";
static const char* JAVA_INVALID_KEY_EXCEPTION = "java/security/InvalidKeyException";
static const char* MSG_OUT_OF_MEMORY = "out of memory";
static const char* MSG_NO_SUCH_ALGORITHM = "algorithm not available";
static const char* MSG_NO_ENTROPY_SOURCE = "no entropy source";
static const char* MSG_INVALID_KEY = "invalid key";

/* Utility functions */

static void jba_to_mpnumber(JNIEnv* env, jbyteArray input, mpnumber* n)
{
    jbyte* data = (*env)->GetByteArrayElements(env, input, (jboolean*) 0);
    if (data == (jbyte*) 0)
    {
        jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
        if (ex)
            (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
    }
	else
	{
		jsize len = (*env)->GetArrayLength(env, input);
		size_t size = MP_BYTES_TO_WORDS(len + MP_WBYTES - 1);

		mpnsetbin(n, data, len);
	}
	(*env)->ReleaseByteArrayElements(env, input, data, JNI_ABORT);
}

static void jba_to_mpbarrett(JNIEnv* env, jbyteArray input, mpbarrett* b)
{
    jbyte* data = (*env)->GetByteArrayElements(env, input, (jboolean*) 0);
    if (data == (jbyte*) 0)
    {
        jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
        if (ex)
            (*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
    }
	else
	{
		jsize len = (*env)->GetArrayLength(env, input);
		size_t size = MP_BYTES_TO_WORDS(len + MP_WBYTES - 1);

		mpbsetbin(b, data, len);
	}
	(*env)->ReleaseByteArrayElements(env, input, data, JNI_ABORT);
}

/* NativeMessageDigest */

jlong JNICALL Java_beecrypt_security_NativeMessageDigest_find(JNIEnv* env, jclass dummy, jstring algorithm)
{
	const char* name = (*env)->GetStringUTFChars(env, algorithm, (jboolean*) 0);
	const hashFunction* hash = hashFunctionFind(name);
	(*env)->ReleaseStringUTFChars(env, algorithm, name);
	if (hash == (hashFunction*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
		if (ex != (jclass) 0)
			(*env)->ThrowNew(env, ex, MSG_NO_SUCH_ALGORITHM);
	}
	return (jlong) hash;
}

jlong JNICALL Java_beecrypt_security_NativeMessageDigest_allocParam(JNIEnv* env, jclass dummy, jlong hash)
{
	void *param = malloc(((const hashFunction*) hash)->paramsize);
	if (param == (void*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex != (jclass) 0)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
	}
	return (jlong) param;
}

jlong JNICALL Java_beecrypt_security_NativeMessageDigest_cloneParam(JNIEnv* env, jclass dummy, jlong hash, jlong param)
{
	unsigned int paramsize = ((const hashFunction*) hash)->paramsize;
	void *clone = malloc(paramsize);
	memcpy(clone, (void*) param, paramsize);
	return (jlong) clone;
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_freeParam(JNIEnv* env, jclass dummy, jlong param)
{
	if (param)
		free((void*) param);
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_reset(JNIEnv* env, jclass dummy, jlong hash, jlong param)
{
	((const hashFunction*) hash)->reset((hashFunctionParam*) param);
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_update(JNIEnv* env, jclass dummy, jlong hash, jlong param, jbyte input)
{
	((const hashFunction*) hash)->update((hashFunctionParam*) param, (const byte*) &input, 1);
}

void JNICALL Java_beecrypt_security_NativeMessageDigest_updateBlock(JNIEnv* env, jclass dummy, jlong hash, jlong param, jbyteArray input, jint offset, jint len)
{
	jbyte* data = (*env)->GetByteArrayElements(env, input, (jboolean*) 0);
	if (data == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, input, data, JNI_ABORT);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	((const hashFunction*) hash)->update((hashFunctionParam*) param, (const byte*) data+offset, len);
	(*env)->ReleaseByteArrayElements(env, input, data, JNI_ABORT);
}

jbyteArray JNICALL Java_beecrypt_security_NativeMessageDigest_digest(JNIEnv* env, jclass dummy, jlong hash, jlong param)
{
	jbyteArray digestArray;
	jbyte* digest;

	int digestsize = (jsize) ((const hashFunction*) hash)->digestsize;

	digestArray = (*env)->NewByteArray(env, digestsize);
	digest = (*env)->GetByteArrayElements(env, digestArray, (jboolean*) 0);

	if (digest == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return (jbyteArray) 0;
	}

	((const hashFunction*) hash)->digest((hashFunctionParam*) param, (byte*) digest);

	(*env)->ReleaseByteArrayElements(env, digestArray, digest, 0);

	return digestArray;
}

jint JNICALL Java_beecrypt_security_NativeMessageDigest_digestLength(JNIEnv* env, jclass dummy, jlong hash)
{
	return (jint) ((const hashFunction*) hash)->digestsize;
}

/* NativeSecureRandom */

jlong JNICALL Java_beecrypt_security_NativeSecureRandom_find(JNIEnv* env, jclass dummy, jstring algorithm)
{
	const char* name = (*env)->GetStringUTFChars(env, algorithm, (jboolean*) 0);
	const randomGenerator* prng = randomGeneratorFind(name);
	(*env)->ReleaseStringUTFChars(env, algorithm, name);
	if (prng == (randomGenerator*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_NO_SUCH_ALGORITHM);
	}
	return (jlong) prng;
}

jlong JNICALL Java_beecrypt_security_NativeSecureRandom_allocParam(JNIEnv* env, jclass dummy, jlong prng)
{
	void *param = malloc(((const randomGenerator*) prng)->paramsize);
	if (param == (void*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
	}
	return (jlong) param;
}

jlong JNICALL Java_beecrypt_security_NativeSecureRandom_cloneParam(JNIEnv* env, jclass dummy, jlong prng, jlong param)
{
	unsigned int paramsize = ((const randomGenerator*) prng)->paramsize;
	void *clone = malloc(paramsize);
	memcpy(clone, (void*) param, paramsize);
	return (jlong) clone;
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_freeParam(JNIEnv* env, jclass dummy, jlong param)
{
	if (param)
		free((void*) param);
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_setup(JNIEnv* env, jclass dummy, jlong prng, jlong param)
{
	((const randomGenerator*) prng)->setup((randomGeneratorParam*) param);
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_setSeed(JNIEnv* env, jclass dummy, jlong prng, jlong param, jbyteArray seedArray)
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
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}

		((const randomGenerator*) prng)->seed((randomGeneratorParam*) param, (byte*) seed, seedSize);

		(*env)->ReleaseByteArrayElements(env, seedArray, seed, JNI_ABORT);
	}
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_nextBytes(JNIEnv* env, jclass dummy, jlong prng, jlong param, jbyteArray bytesArray)
{
	jsize bytesSize = (*env)->GetArrayLength(env, bytesArray);
	if (bytesSize)
	{
		jbyte* bytes = (*env)->GetByteArrayElements(env, bytesArray, (jboolean*) 0);
		if (bytes == (jbyte*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}

		((const randomGenerator*) prng)->next((randomGeneratorParam*) param, (byte*) bytes, bytesSize);

		(*env)->ReleaseByteArrayElements(env, bytesArray, bytes, 0);
	}
}

void JNICALL Java_beecrypt_security_NativeSecureRandom_generateSeed(JNIEnv* env, jclass dummy, jbyteArray seedArray)
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
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}

		if (ents == (entropySource*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
			(*env)->ReleaseByteArrayElements(env, seedArray, seed, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_NO_ENTROPY_SOURCE);
			return;
		}

		ents->next((byte*) seed, seedSize);

		(*env)->ReleaseByteArrayElements(env, seedArray, seed, 0);
	}
}

/* NativeBlockCipher */

jlong JNICALL Java_beecrypt_crypto_NativeBlockCipher_find(JNIEnv* env, jclass dummy, jstring algorithm)
{
	const char* name = (*env)->GetStringUTFChars(env, algorithm, (jboolean*) 0);
	const blockCipher* ciph = blockCipherFind(name);
	(*env)->ReleaseStringUTFChars(env, algorithm, name);
	if (ciph == (blockCipher*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_PROVIDER_EXCEPTION);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_NO_SUCH_ALGORITHM);
	}
	return (jlong) ciph;
}

jlong JNICALL Java_beecrypt_crypto_NativeBlockCipher_allocParam(JNIEnv* env, jclass dummy, jlong ciph)
{
	void *param = malloc(((const blockCipher*) ciph)->paramsize);
	if (param == (void*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
	}
	return (jlong) param;
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_freeParam(JNIEnv* env, jclass dummy, jlong param)
{
	if (param)
		free((void*) param);
}

jint JNICALL Java_beecrypt_crypto_NativeBlockCipher_getBlockSize(JNIEnv* env, jclass dummy, jlong ciph)
{
	return ((const blockCipher*) ciph)->blocksize;
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_setup(JNIEnv* env, jclass dummy, jlong ciph, jlong param, jint mode, jbyteArray keyArray)
{
	/* BeeCrypt takes key in byte array with size in bits */
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
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}

		rc = ((const blockCipher*) ciph)->setup((blockCipherParam*) param, (const byte*) key, keysize << 3, nativeop);

		if (rc != 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_INVALID_KEY_EXCEPTION);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_INVALID_KEY);
		}

		(*env)->ReleaseByteArrayElements(env, keyArray, key, JNI_ABORT);
	}
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_setIV(JNIEnv* env, jclass dummy, jlong ciph, jlong param, jbyteArray ivArray)
{
	if (ivArray == (jbyteArray) 0)
	{
		((const blockCipher*) ciph)->setiv((blockCipherParam*) param, 0);
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
					(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
				return;
			}
		
			((const blockCipher*) ciph)->setiv((blockCipherParam*) param, (byte*) iv);

			(*env)->ReleaseByteArrayElements(env, ivArray, iv, JNI_ABORT);
		}
	}
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_encryptECB(JNIEnv* env, jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32_t* datain;
		uint32_t* dataout;

		datain = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (datain == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (dataout == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32_t));
		blockEncryptECB((const blockCipher*) ciph, (blockCipherParam*) param, dataout, datain, blocks);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32_t));
	}
	else
	{	/* aligned */
		blockEncryptECB((const blockCipher*) ciph, (blockCipherParam*) param, (uint32_t*)(output+outputOffset), (uint32_t*) (input+inputOffset), blocks);
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_decryptECB(JNIEnv* env, jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32_t* datain;
		uint32_t* dataout;

		datain = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (datain == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (dataout == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32_t));
		blockDecryptECB((const blockCipher*) ciph, (blockCipherParam*) param, dataout, datain, blocks);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32_t));
	}
	else
	{	/* aligned */
		blockDecryptECB((const blockCipher*) ciph, (blockCipherParam*) param, (uint32_t*)(output+outputOffset), (uint32_t*) (input+inputOffset), blocks);
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_encryptCBC(JNIEnv* env, jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32_t* datain;
		uint32_t* dataout;

		datain = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (datain == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (dataout == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32_t));
		blockEncryptCBC((const blockCipher*) ciph, (blockCipherParam*) param, dataout, datain, blocks);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32_t));
	}
	else
	{	/* aligned */
		blockEncryptCBC((const blockCipher*) ciph, (blockCipherParam*) param, (uint32_t*)(output+outputOffset), (uint32_t*) (input+inputOffset), blocks);
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

void JNICALL Java_beecrypt_crypto_NativeBlockCipher_decryptCBC(JNIEnv* env, jclass dummy, jlong ciph, jlong param, jbyteArray inputArray, jint inputOffset, jbyteArray outputArray, jint outputOffset, jint blocks)
{
	jbyte* input;
	jbyte* output;

	input = (*env)->GetByteArrayElements(env, inputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}
	output = (*env)->GetByteArrayElements(env, outputArray, (jboolean*) 0);
	if (input == (jbyte*) 0)
	{
		jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
		(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
		if (ex)
			(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
		return;
	}

	if (((long) (input+inputOffset) & 0x3) || ((long) (output+outputOffset) & 0x3))
	{	/* unaligned */
		uint32_t* datain;
		uint32_t* dataout;

		datain = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (datain == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		dataout = (uint32_t*) malloc(blocks * sizeof(uint32_t));
		if (dataout == (uint32_t*) 0)
		{
			jclass ex = (*env)->FindClass(env, JAVA_OUT_OF_MEMORY_ERROR);
			free(datain);
			(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
			(*env)->ReleaseByteArrayElements(env, outputArray, output, JNI_ABORT);
			if (ex)
				(*env)->ThrowNew(env, ex, MSG_OUT_OF_MEMORY);
			return;
		}
		memcpy(datain, input+inputOffset, blocks * sizeof(uint32_t));
		blockDecryptCBC((const blockCipher*) ciph, (blockCipherParam*) param, dataout, datain, blocks);
		memcpy(output+outputOffset, dataout, blocks * sizeof(uint32_t));
	}
	else
	{	/* aligned */
		blockDecryptCBC((const blockCipher*) ciph, (blockCipherParam*) param, (uint32_t*)(output+outputOffset), (uint32_t*) (input+inputOffset), blocks);
	}

	(*env)->ReleaseByteArrayElements(env, inputArray, input, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, outputArray, output, 0);
}

#endif
