// standard c++ includes
#include <string>
#include <vector>

// rpm includes
#include <rpmlib.h>
#include <rpmbuild.h>

// type definitions
typedef vector<string> t_StrVector;

using namespace std;

bool getRPMHeader(Header header,
				  int_32 nTag,
				  string& rResult)
{
	if (headerIsEntry(header, nTag)) {
		int_32 nCount, nTagType;
		void* pBuffer;
		if (!headerGetEntry(header, nTag, &nTagType, &pBuffer, &nCount))
			return false;
		char szTemp[32];
		switch (nTagType) {
			case RPM_INT32_TYPE:
				sprintf(szTemp, "%d", *((int_32*)pBuffer));
				rResult.assign(szTemp);
			default:
				rResult.assign((char*)pBuffer);
				break;
		}
		headerFreeData(pBuffer, (rpmTagType_e)nTagType);
		return true;
	}
	else
		return false;
}

bool getRPMHeaderArray(Header header,
					   int_32 nTag,
					   t_StrVector& rvResult)
{
	rvResult.clear();
	if (headerIsEntry(header, nTag)) {
		int_32 nCount, nTagType;
		void* pBuffer;
		if (!headerGetEntry(header, nTag, &nTagType, &pBuffer, &nCount))
			return false;
		if (nTagType == RPM_STRING_ARRAY_TYPE || nTagType == RPM_I18NSTRING_TYPE) {
			for (int_32 i = 0; i < nCount; i++)
				rvResult.push_back(((char**)pBuffer)[i]);
		}
		else
			rvResult.push_back((char*)pBuffer);
		headerFreeData(pBuffer, (rpmTagType_e)nTagType);
		return true;
	}
	else
		return false;
}

bool getRPMMacro(const char* szMacro,
				 string& rResult)
{
	char* szValue = rpmExpand(szMacro, NULL);
	if (szValue) {
		rResult.assign(szValue);
		return true;
	}
	else
		return false;
}
