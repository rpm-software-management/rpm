from . import RPMTAG_PAYLOADCOMPRESSOR, fd, files

"""This file contains high-level simple wrappers."""

__all__ = ("RPMPackageUnpackerContext",)


class RPMPackageUnpackerContext:
	"""This class allows you to iterate files and unpack them from a package."""

	__slots__ = ("path", "ts", "hdr", "files", "rpmFd", "compressedFd", "archive")

	def __init__(self, path, transactionSet=None):
		self.path = path
		self.ts = transactionSet
		self.rpmFd = None
		self.hdr = None
		self.files = None

	@property
	def fileNo(self):
		"""Returns a file number"""
		return self.rpmFd.fileno()

	def __enter__(self):
		self.rpmFd = fd(str(self.path), "r")  # RPM-specific file descriptor
		self.hdr = self.ts.hdrFromFdno(self.rpmFd)  # should it be explicitly freed anyhow?
		self.files = files(self.hdr)  # should it be explicitly freed anyhow?
		self.compressedFd = fd(self.rpmFd, "r", flags=self.hdr[RPMTAG_PAYLOADCOMPRESSOR])  # RPM-specific compressed file descriptor
		self.archive = self.files.archive(self.compressedFd, False)
		return self

	def iterFiles(self):  # type: typing.Callable[[], typing.Iterable[typing.Tuple["rpm.file", "rpm.archive"]]]
		for f in self.archive:
			# f is rpm.file
			yield f, self.archive

	def __exit__(self, *args, **kwargs):
		self.archive.close()
		self.compressedFd.close()
		self.rpmFd.close()
