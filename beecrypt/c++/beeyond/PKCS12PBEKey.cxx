#define BEECRYPT_CXX_DLL_EXPORT

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/beeyond/PKCS12PBEKey.h"

#include <iostream>
using std::cout;
using std::endl;

using namespace beecrypt::beeyond;

PKCS12PBEKey::PKCS12PBEKey(const array<javachar>& password, const bytearray* salt, size_t iterationCount) : _pswd(password)
{
	if (salt)
		_salt = new bytearray(*salt);
	else
		_salt = 0;
	_iter = iterationCount;
	_enc = 0;
}

PKCS12PBEKey::~PKCS12PBEKey()
{
	if (_salt)
		delete _salt;
}

PKCS12PBEKey* PKCS12PBEKey::clone() const
{
	return new PKCS12PBEKey(_pswd, _salt, _iter);
}

bytearray* PKCS12PBEKey::encode(const array<javachar>& password, const bytearray* salt, size_t iterationCount)
{
	size_t i;

	bytearray* result = new bytearray((password.size() + 1) * 2);

	for (i = 0; i < password.size(); i++)
	{
		(*result)[2*i  ] = (password[i] >> 8) & 0xff;
		(*result)[2*i+1] = (password[i]     ) & 0xff;
	}
	(*result)[2*i  ] = 0;
	(*result)[2*i+1] = 0;

	return result;
}

size_t PKCS12PBEKey::getIterationCount() const throw ()
{
	return _iter;
}

const array<javachar>& PKCS12PBEKey::getPassword() const throw ()
{
	return _pswd;
}

const bytearray* PKCS12PBEKey::getSalt() const throw ()
{
	return _salt;
}

const bytearray* PKCS12PBEKey::getEncoded() const
{
	if (!_enc)
		_enc = encode(_pswd, _salt, _iter);

	return _enc;
}

const String& PKCS12PBEKey::getAlgorithm() const throw ()
{
	static const String ALGORITHM = UNICODE_STRING_SIMPLE("PKCS#12/PBE");
	return ALGORITHM;
}

const String* PKCS12PBEKey::getFormat() const throw ()
{
	static const String FORMAT = UNICODE_STRING_SIMPLE("RAW");
	return &FORMAT;
}
