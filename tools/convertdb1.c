#include "system.h"
#include <rpmio.h>
#include <rpmlib.h>
#include <rpmdb.h>
#include <rpmmacro.h>
#include "legacy.h"
#include "debug.h"

#define FA_MAGIC      0x02050920

struct faFileHeader{
    unsigned int magic;
    unsigned int firstFree;
};

struct faHeader {
    unsigned int size;				
    unsigned int freeNext; /* offset of the next free block, 0 if none */
    unsigned int freePrev; 
    unsigned int isFree;

    /* note that the u16's appear last for alignment/space reasons */
};


static int fadFileSize;

static ssize_t Pread(FD_t fd, void * buf, size_t count, off_t offset) {
    if (Fseek(fd, offset, SEEK_SET) < 0)
        return -1;
    return Fread(buf, sizeof(char), count, fd);
}

static FD_t fadOpen(const char * path)
{
    struct faFileHeader newHdr;
    FD_t fd;
    struct stat stb;

    fd = Fopen(path, "r.fdio");
    if (!fd || Ferror(fd))
	return NULL;

    if (fstat(Fileno(fd), &stb)) {
	Fclose(fd);
        return NULL;
    }
    fadFileSize = stb.st_size;

    /* is this file brand new? */
    if (fadFileSize == 0) {
	Fclose(fd);
	return NULL;
    }
    if (Pread(fd, &newHdr, sizeof(newHdr), 0) != sizeof(newHdr)) {
	Fclose(fd);
	return NULL;
    }
    if (newHdr.magic != FA_MAGIC) {
	Fclose(fd);
	return NULL;
    }
    /*@-refcounttrans@*/ return fd /*@=refcounttrans@*/ ;
}

static int fadNextOffset(FD_t fd, unsigned int lastOffset)
{
    struct faHeader header;
    int offset;

    offset = (lastOffset)
	? (lastOffset - sizeof(header))
	: sizeof(struct faFileHeader);

    if (offset >= fadFileSize)
	return 0;

    if (Pread(fd, &header, sizeof(header), offset) != sizeof(header))
	return 0;

    if (!lastOffset && !header.isFree)
	return (offset + sizeof(header));

    do {
	offset += header.size;

	if (Pread(fd, &header, sizeof(header), offset) != sizeof(header))
	    return 0;

	if (!header.isFree) break;
    } while (offset < fadFileSize && header.isFree);

    if (offset < fadFileSize) {
	/* Sanity check this to make sure we're not going in loops */
	offset += sizeof(header);

	if (offset <= lastOffset) return -1;

	return offset;
    } else
	return 0;
}

static int fadFirstOffset(FD_t fd)
{
    return fadNextOffset(fd, 0);
}

static rpmdb db;

int
main(argc, argv)
int argc;
char **argv;
{
  FD_t fd;
  int offset;
  Header h;
  const char *name;
  const char *version;
  const char *release;

  if (argc != 2)
    {
      fprintf(stderr, "usage: rpm3import <packages.rpm>\n");
      exit(1);
    }
  if ((fd = fadOpen(argv[1])) == 0)
    {
      fprintf(stderr, "could not open %s\n", argv[1]);
      exit(1);
    }
  rpmInitMacros(NULL, "/usr/lib/rpm/macros");
  if (rpmdbOpen("/", &db, O_RDWR, 0644)) {
    fprintf(stderr, "could not open rpm database\n");
    exit(1);
  }
   
  for (offset = fadFirstOffset(fd); offset; offset = fadNextOffset(fd, offset))
    {
      rpmdbMatchIterator mi;

      /* have to use lseek instead of Fseek because headerRead
       * uses low level IO
       */
      if (lseek(Fileno(fd), (off_t)offset, SEEK_SET) == -1)
	{
	  perror("lseek");
	  continue;
	}
      h = headerRead(fd, HEADER_MAGIC_NO);
      if (!h)
	continue;
      compressFilelist(h);
      headerNVR(h, &name, &version, &release);
      mi = rpmdbInitIterator(db, RPMTAG_NAME, name, 0);
      rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_DEFAULT, version);
      rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_DEFAULT, release);
      if (rpmdbNextIterator(mi))
        {
	  printf("%s-%s-%s is already in database\n", name, version, release);
	  rpmdbFreeIterator(mi);
	  headerFree(h);
	  continue;
        }
      rpmdbFreeIterator(mi);
      if (rpmdbAdd(db, -1, h, 0, 0))
	{
	  fprintf(stderr, "could not add %s-%s-%s!\n", name, version, release);
	}
      headerFree(h);
    }
  Fclose(fd);
  rpmdbClose(db);
  return 0;
}

