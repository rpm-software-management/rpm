#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "rpmchk.h"

RpmFile	*OpenRpmFile( char *name )
{
RpmFile	*efile;
struct stat	stat;

if( (efile=(RpmFile *)calloc(1,sizeof(RpmFile))) == NULL ) {
	fprintf(stderr, "Unable to alloc efile memory for %s\n", name );
	return NULL;
	}

if( (efile->fd=open(name, O_RDONLY, 0666)) < 0 ) {
	perror( "unable to open() file" );
	free(efile);
	return NULL;
	}

if( fstat(efile->fd, &stat ) < 0 ) {
	perror( "unable to stat() file" );
	close(efile->fd);
	free(efile);
	return NULL;
	}

efile->size=stat.st_size;

if( efile->size == 0 ) {
	fprintf( stderr, "Zero length file\n" );
	close(efile->fd);
	free(efile);
	exit(-1);	/* Silently exit */
	}

if( (efile->addr=mmap(0, efile->size, PROT_READ|PROT_WRITE, MAP_PRIVATE, efile->fd, 0)) == (caddr_t)-1 ) {
	perror( "unable to mmap() file" );
	close(efile->fd);
	free(efile);
	return NULL;
	}

/*
fprintf(stderr,"%d bytes at %x", efile->size, efile->addr );
fprintf(stderr,"to %x\n", efile->addr+efile->size );
*/

if( memcmp(efile->addr, RPMMAG, SRPMMAG) ) {
	fprintf( stderr, "file not RPM\n" );
	close(efile->fd);
	free(efile);
	exit(-1);	/* Silently exit */
	}

return efile;
}
