#ifndef _BEECRYPT_API_H
#define _BEECRYPT_API_H

#if defined(_WIN32) && !defined(WIN32)
# define WIN32 1
#endif

#if WIN32 && !__CYGWIN32__
# include "beecrypt.win.h"
# ifdef BEECRYPT_DLL_EXPORT
#  define BEECRYPTAPI __declspec(dllexport)
# else
#  define BEECRYPTAPI __declspec(dllimport)
# endif
/*typedef UINT8_TYPE    byte;*/
#else
# include "beecrypt.gnu.h"
# define BEECRYPTAPI
typedef UINT8_TYPE  byte;
#endif

#ifndef ROTL32
# define ROTL32(x, s) (((x) << (s)) | ((x) >> (32 - (s))))
#endif
#ifndef ROTR32
# define ROTR32(x, s) (((x) >> (s)) | ((x) << (32 - (s))))
#endif

typedef INT8_TYPE   int8;
typedef INT16_TYPE  int16;
typedef INT32_TYPE  int32;
typedef INT64_TYPE  int64;

typedef UINT8_TYPE  uint8;
typedef UINT16_TYPE uint16;
typedef UINT32_TYPE uint32;
typedef UINT64_TYPE uint64;

typedef INT8_TYPE   javabyte;
typedef INT16_TYPE  javashort;
typedef INT32_TYPE  javaint;
typedef INT64_TYPE  javalong;

typedef UINT16_TYPE javachar;

typedef FLOAT4_TYPE     javafloat;
typedef DOUBLE8_TYPE    javadouble;

#endif
