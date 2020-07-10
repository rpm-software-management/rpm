from __future__ import absolute_import

from . import _rpm

__all__ = []

_enumClass = None
_flagEnumClass = None
try:
	from enum import IntEnum as _enumClass
except ImportError:
	try:
		from enum import Enum as _enumClass
	except ImportError:

		def _identity(self, x):
			return x

		def _enumClass(name, props):
			props["__slots__"] = ()
			props["__call__"] = _identity
			return type(name, (object,), props)()


try:
	from enum import IntFlag as _flagEnumClass
except ImportError:
	_flagEnumClass = _enumClass


def _populateEnums():
	class EnumConversionSpec:
		__slots__ = ("enumClass", "prefix", "name")

		def __init__(self, enumClass, prefix, name):
			self.enumClass = enumClass
			self.prefix = prefix
			self.name = name

	spec = [
		EnumConversionSpec(_enumClass, "RPMTAG_", "RPMTag"),
		EnumConversionSpec(_enumClass, "RPMRC_", "RPMResultCode"),
		EnumConversionSpec(_enumClass, "RPMPROB_", "RPMProb"),
		EnumConversionSpec(_enumClass, "RPMMIRE_", "RPMMire"),
		EnumConversionSpec(_enumClass, "RPMLOG_", "RPMLog"),
		EnumConversionSpec(_enumClass, "RPMFILE_STATE_", "RPMFileState"),
		EnumConversionSpec(_enumClass, "RPMDBI_", "RPMDBI"),
		EnumConversionSpec(_flagEnumClass, "RPMBUILD_", "RPMBuild"),
		EnumConversionSpec(_flagEnumClass, "HEADERCONV_", "HeaderConv"),
		EnumConversionSpec(_flagEnumClass, "RPMSENSE_", "RPMSense"),
		EnumConversionSpec(_flagEnumClass, "RPMTRANS_FLAG_", "RPMTransFlag"),
		EnumConversionSpec(_flagEnumClass, "RPMVERIFY_", "RPMVerify"),
		EnumConversionSpec(_flagEnumClass, "RPMVSF_", "RPMVSF"),
		EnumConversionSpec(_flagEnumClass, "RPMFILE_", "RPMFile"),
		EnumConversionSpec(_flagEnumClass, "RPMCALLBACK_", "RPMCallback"),
	]

	dicts = {e.name: [] for e in spec}

	for k in dir(_rpm):
		for el in spec:
			if k.startswith(el.prefix):
				dicts[el.name].append((k[len(el.prefix) :], getattr(_rpm, k)))
				break

	for e in spec:
		globals()[e.name] = e.enumClass(e.name, dict(sorted(dicts[e.name], key=lambda x: (x[1], x[0]))))
		__all__.append(e.name)


_populateEnums()
del _populateEnums

__all__ = tuple(__all__)
