/* signature.c - RPM signature functions */

/* NOTES
 *
 * Things have been cleaned up wrt PGP.  We can now handle
 * signatures of any length (which means you can use any
 * size key you like).  We also honor PGPPATH finally.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <asm/byteorder.h>
#include <fcntl.h>
#include <strings.h>

#include "signature.h"
#include "md5.h"
#include "rpmlib.h"
#include "rpmlead.h"
#include "rpmerr.h"

static int makePGPSignature(char *file, void **sig, int_32 *size,
			    char *passPhrase);
static int checkSize(int fd, int size, int sigsize);
static int auxVerifyPGPSig(char *datafile, char *sigfile, char *result);
static int verifyMD5Signature(char *datafile, void *sigfile, char *result);
static int verifyOLDPGPSignature(int fd, void *sig, char *result);
static int verifyMD5PGPSignature(int fd, unsigned char *sig,
				 char *result, int pgp);
static int checkPassPhrase(char *passPhrase);

int sigLookupType(void)
{
    char *name;

    if (! (name = getVar(RPMVAR_SIGTYPE))) {
	return 0;
    }

    if (!strcasecmp(name, "none")) {
	return 0;
    } else if (!strcasecmp(name, "pgp")) {
	return SIGTAG_PGP;
    } else {
	return -1;
    }
}

/* readSignature() emulates the new style signatures if it finds an */
/* old-style one.  It also immediately verifies the header+archive  */
/* size and returns an error if it doesn't match.                   */

int readSignature(int fd, Header *header, short sig_type)
{
    unsigned char buf[2048];
    int sigSize, length, pad;
    int_32 size[2], type, count;
    int_32 *archSize;
    Header h;

    if (header) {
	*header = NULL;
    }
    
    switch (sig_type) {
      case RPMSIG_NONE:
	message(MESS_DEBUG, "No signature\n");
	break;
      case RPMSIG_PGP262_1024:
	message(MESS_DEBUG, "Old PGP signature\n");
	/* These are always 256 bytes */
	if (read(fd, buf, 256) != 256) {
	    return 1;
	}
	if (header) {
	    *header = newHeader();
	    addEntry(*header, SIGTAG_PGP, BIN_TYPE, buf, 152);
	}
	break;
      case RPMSIG_MD5:
	message(MESS_DEBUG, "Old-new MD5 signature\n");
	/* 8 byte aligned */
	if (read(fd, size, 8) != 8) {
	    return 1;
	}
	size[0] = ntohl(size[0]);
	if (checkSize(fd, size[0], 8 + 16)) {
	    return 1;
	}
	if (read(fd, buf, 16) != 16) {
	    return 1;
	}
	if (header) {
	    *header = newHeader();
	    addEntry(*header, SIGTAG_SIZE, INT32_TYPE, size, 1);
	    addEntry(*header, SIGTAG_MD5, BIN_TYPE, buf, 16);
	}
	break;
      case RPMSIG_MD5_PGP:
	message(MESS_DEBUG, "Old-new MD5 and PGP signature\n");
	/* 8 byte aligned */
	/* Size of header+archive */
	if (read(fd, size, 8) != 8) {
	    return 1;
	}
	size[0] = ntohl(size[0]);
	/* Read MD5 sum and size of PGP signature */
	if (read(fd, buf, 18) != 18) {
	    return 1;
	}
	length = buf[16] * 256 + buf[17];  /* length of PGP sig */
	pad = (8 - ((length + 18) % 8)) % 8;
	if (checkSize(fd, size[0], 8 + 16 + 2 + length + pad)) {
	    return 1;
	}
	/* Read PGP signature */
	if (read(fd, buf + 18, length) != length) {
	    return 1;
	}
	if (header) {
	    *header = newHeader();
	    addEntry(*header, SIGTAG_SIZE, INT32_TYPE, size, 1);
	    addEntry(*header, SIGTAG_MD5, BIN_TYPE, buf, 16);
	    addEntry(*header, SIGTAG_PGP, BIN_TYPE, buf+18, length);
	}
	/* The is the align magic */
	if (pad) {
	    if (read(fd, buf, pad) != pad) {
		if (header) {
		    freeHeader(*header);
		    *header = NULL;
		}
		return 1;
	    }
	}
	break;
      case RPMSIG_HEADERSIG:
	message(MESS_DEBUG, "New Header signature\n");
	/* This is a new style signature */
	h = readHeader(fd, HEADER_MAGIC);
	if (! h) {
	    return 1;
	}
	sigSize = sizeofHeader(h, HEADER_MAGIC);
	pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	message(MESS_DEBUG, "Signature size: %d\n", sigSize);
	message(MESS_DEBUG, "Signature pad : %d\n", pad);
	if (! getEntry(h, SIGTAG_SIZE, &type, (void **)&archSize, &count)) {
	    freeHeader(h);
	    return 1;
	}
	if (checkSize(fd, *archSize, sigSize + pad)) {
	    freeHeader(h);
	    return 1;
	}
	if (pad) {
	    if (read(fd, buf, pad) != pad) {
		freeHeader(h);
		return 1;
	    }
	}
	if (header) {
	    *header = h;
	} else {
	    freeHeader(h);
	}
	break;
      default:
	return 1;
    }

    return 0;
}

int writeSignature(int fd, Header header)
{
    int sigSize, pad;
    unsigned char buf[8];
    
    writeHeader(fd, header, HEADER_MAGIC);
    sigSize = sizeofHeader(header, HEADER_MAGIC);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
	message(MESS_DEBUG, "Signature size: %d\n", sigSize);
	message(MESS_DEBUG, "Signature pad : %d\n", pad);
	memset(buf, 0, pad);
	write(fd, buf, pad);
    }
    return 0;
}

Header newSignature(void)
{
    return newHeader();
}

void freeSignature(Header h)
{
    freeHeader(h);
}

int addSignature(Header header, char *file, int_32 sigTag, char *passPhrase)
{
    struct stat statbuf;
    int_32 size;
    unsigned char buf[16];
    void *sig;
    
    switch (sigTag) {
      case SIGTAG_SIZE:
	stat(file, &statbuf);
	size = statbuf.st_size;
	addEntry(header, SIGTAG_SIZE, INT32_TYPE, &size, 1);
	break;
      case SIGTAG_MD5:
	mdbinfile(file, buf);
	addEntry(header, sigTag, BIN_TYPE, buf, 16);
	break;
      case SIGTAG_PGP:
	makePGPSignature(file, &sig, &size, passPhrase);
	addEntry(header, sigTag, BIN_TYPE, sig, size);
	break;
    }

    return 0;
}

static int makePGPSignature(char *file, void **sig, int_32 *size,
			    char *passPhrase)
{
    char name[1024];
    char sigfile[1024];
    int pid, status;
    int fd, inpipe[2];
    FILE *fpipe;
    struct stat statbuf;

    sprintf(name, "+myname=\"%s\"", getVar(RPMVAR_PGP_NAME));

    sprintf(sigfile, "%s.sig", file);

    pipe(inpipe);
    
    if (!(pid = fork())) {
	close(0);
	dup2(inpipe[0], 3);
	close(inpipe[1]);
	setenv("PGPPASSFD", "3", 1);
	if (getVar(RPMVAR_PGP_PATH)) {
	    setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	}
	/* setenv("PGPPASS", passPhrase, 1); */
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0", "+armor=off",
	       name, "-sb", file, sigfile,
	       NULL);
	error(RPMERR_EXEC, "Couldn't exec pgp");
	exit(RPMERR_EXEC);
    }

    fpipe = fdopen(inpipe[1], "w");
    close(inpipe[0]);
    fprintf(fpipe, "%s\n", passPhrase);
    fclose(fpipe);

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_SIGGEN, "pgp failed");
	return 1;
    }

    if (stat(sigfile, &statbuf)) {
	/* PGP failed to write signature */
	unlink(sigfile);  /* Just in case */
	error(RPMERR_SIGGEN, "pgp failed to write signature");
	return 1;
    }

    *size = statbuf.st_size;
    message(MESS_DEBUG, "PGP sig size: %d\n", *size);
    *sig = malloc(*size);
    
    fd = open(sigfile, O_RDONLY);
    if (read(fd, *sig, *size) != *size) {
	unlink(sigfile);
	close(fd);
	free(*sig);
	error(RPMERR_SIGGEN, "unable to read the signature");
	return 1;
    }
    close(fd);
    unlink(sigfile);

    message(MESS_DEBUG, "Got %d bytes of PGP sig\n", *size);
    
    return 0;
}

static int checkSize(int fd, int size, int sigsize)
{
    int headerArchiveSize;
    struct stat statbuf;

    fstat(fd, &statbuf);
    headerArchiveSize = statbuf.st_size - sizeof(struct rpmlead) - sigsize;

    message(MESS_DEBUG, "sigsize         : %d\n", sigsize);
    message(MESS_DEBUG, "Header + Archive: %d\n", headerArchiveSize);
    message(MESS_DEBUG, "expected size   : %d\n", size);

    return size - headerArchiveSize;
}

int verifySignature(int fd, short sig_type, void *sig, char *result, int pgp)
{
    int res = RPMSIG_SIGOK;

    switch (sig_type) {
      case RPMSIG_NONE:
	strcpy(result, "No signature information available\n");
	return (RPMSIG_BADSIG | RPMSIG_NOSIG);
	break;
      case RPMSIG_PGP262_1024:
	if ((res = verifyOLDPGPSignature(fd, sig, result))) {
	    return (RPMSIG_BADSIG | res);
	}
	break;
      case RPMSIG_MD5:
	if ((res = verifyMD5PGPSignature(fd, sig, result, 0))) {
	    return (RPMSIG_BADSIG | res);
	}
	break;
      case RPMSIG_MD5_PGP:
	if ((res = verifyMD5PGPSignature(fd, sig, result, pgp))) {
	    return (RPMSIG_BADSIG | res);
	}
	break;
      default:
	sprintf(result, "Unimplemented signature type\n");
	return (RPMSIG_BADSIG | RPMSIG_UNKNOWNSIG);
	break;
    }

    return RPMSIG_SIGOK;
}

static int auxVerifyPGPSig(char *datafile, char *sigfile, char *result)
{
    int pid, status, outpipe[2];
    unsigned char buf[8192];
    FILE *file;

    /* Now run PGP */
    pipe(outpipe);

    if (!(pid = fork())) {
	close(1);
	close(outpipe[0]);
	dup2(outpipe[1], 1);
	if (getVar(RPMVAR_PGP_PATH)) {
	    setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	}
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0",
	       sigfile, datafile,
	       NULL);
	printf("exec failed!\n");
	error(RPMERR_EXEC, "Could not run pgp.  Use --nopgp to verify MD5 checksums only.");
	exit(RPMERR_EXEC);
    }

    close(outpipe[1]);
    file = fdopen(outpipe[0], "r");
    result[0] = '\0';
    while (fgets(buf, 1024, file)) {
	if (strncmp("File '", buf, 6) && strncmp("Text is assu", buf, 12)) {
	    strcat(result, buf);
	}
    }
    fclose(file);

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	return(RPMSIG_BADPGP);
    }
    
    return(RPMSIG_SIGOK);
}

static int verifyOLDPGPSignature(int fd, void *sig, char *result)
{
    char *sigfile;
    char *datafile;
    int count, sfd, ret;
    unsigned char buf[8192];

    /* Write out the signature */
    sigfile = tempnam("/var/tmp", "rpmsig");
    sfd = open(sigfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    write(sfd, sig, 152);
    close(sfd);

    /* Write out the data */
    datafile = tempnam("/var/tmp", "rpmsig");
    sfd = open(datafile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    while((count = read(fd, buf, 8192)) > 0) {
	write(sfd, buf, count);
    }
    close(sfd);

    ret = auxVerifyPGPSig(datafile, sigfile, result);
    
    unlink(datafile);
    unlink(sigfile);
    
    return(ret);
}

static int verifyMD5Signature(char *datafile, void *sig, char *result)
{
    unsigned char md5sum[16];

    mdbinfile(datafile, md5sum);
    if (memcmp(md5sum, sig, 16)) {
	strcpy(result, "MD5 sum mismatch");
	return RPMSIG_BADMD5;
    }

    return RPMSIG_SIGOK;
}

static int verifyMD5PGPSignature(int fd, unsigned char *sig,
				 char *result, int pgp)
{
    char *datafile;
    char *sigfile;
    int count, sfd;
    unsigned char buf[8192];

    /* Write out the data */
    datafile = tempnam("/var/tmp", "rpmsig");
    sfd = open(datafile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    while((count = read(fd, buf, 8192)) > 0) {
	write(sfd, buf, count);
    }
    close(sfd);

    if (verifyMD5Signature(datafile, sig, result)) {
	unlink(datafile);
	return RPMSIG_BADMD5;
    }

    if (! pgp) {
   	unlink(datafile);
	return 0;
    }
    
    /* Write out the signature */
    sig += 16;
    count = sig[0] * 256 + sig[1];
    sigfile = tempnam("/var/tmp", "rpmsig");
    sfd = open(sigfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    sig += 2;
    write(sfd, sig, count);
    close(sfd);

    if (auxVerifyPGPSig(datafile, sigfile, result)) {
	unlink(datafile);
	unlink(sigfile);
	return RPMSIG_BADPGP;
    }

    unlink(datafile);
    unlink(sigfile);
    return RPMSIG_SIGOK;
}

char *getPassPhrase(char *prompt)
{
    char *pass;

    if (! getVar(RPMVAR_PGP_NAME)) {
	error(RPMERR_SIGGEN,
	      "You must set \"pgp_name:\" in your rpmrc file");
	return NULL;
    }

    if (prompt) {
        pass = getpass(prompt);
    } else {
        pass = getpass("");
    }

    if (checkPassPhrase(pass)) {
	return NULL;
    }

    return pass;
}

static int checkPassPhrase(char *passPhrase)
{
    char name[1024];
    int passPhrasePipe[2];
    FILE *fpipe;
    int pid, status;
    int fd;

    sprintf(name, "+myname=\"%s\"", getVar(RPMVAR_PGP_NAME));

    pipe(passPhrasePipe);
    if (!(pid = fork())) {
	close(0);
	close(1);
	if (! isVerbose()) {
	    close(2);
	}
	if ((fd = open("/dev/null", O_RDONLY)) != 0) {
	    dup2(fd, 0);
	}
	if ((fd = open("/dev/null", O_WRONLY)) != 1) {
	    dup2(fd, 1);
	}
	dup2(passPhrasePipe[0], 3);
	setenv("PGPPASSFD", "3", 1);
	if (getVar(RPMVAR_PGP_PATH)) {
	    setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	}
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0",
	       name, "-sf",
	       NULL);
	error(RPMERR_EXEC, "Couldn't exec pgp");
	exit(RPMERR_EXEC);
    }

    fpipe = fdopen(passPhrasePipe[1], "w");
    close(passPhrasePipe[0]);
    fprintf(fpipe, "%s\n", passPhrase);
    fclose(fpipe);

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	return 1;
    }

    /* passPhrase is good */
    return 0;
}
