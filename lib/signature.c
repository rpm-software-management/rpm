/* signature.c - RPM signature functions */

/* NOTES
 *
 * Things have been cleaned up wrt PGP.  We can now handle
 * signatures of any length (which means you can use any
 * size key you like).  We also honor PGPPATH finally.
 *
 * We are aligning the sig section on disk to 8 bytes.
 * The lead is already 8 byte aligned.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>

#include "signature.h"
#include "md5.h"
#include "rpmlib.h"
#include "rpmerr.h"

static int makeMD5Signature(char *file, int ofd);
static int makePGPSignature(char *file, int ofd, char *passPhrase);
static int auxVerifyPGPSig(char *datafile, char *sigfile, char *result);
static int verifyMD5Signature(char *datafile, void *sigfile, char *result);
static int verifyOLDPGPSignature(int fd, void *sig, char *result);
static int verifyMD5PGPSignature(int fd, unsigned char *sig,
				 char *result, int pgp);
static int checkPassPhrase(char *passPhrase);

unsigned short sigLookupType(void)
{
    char *name;

    /* Now we always generate at least a RPMSIG_MD5 */
    
    if (! (name = getVar(RPMVAR_SIGTYPE))) {
	return RPMSIG_MD5;
    }

    if (!strcasecmp(name, "none")) {
	return RPMSIG_MD5;
    } else if (!strcasecmp(name, "pgp")) {
	return RPMSIG_MD5_PGP;
    } else {
	return RPMSIG_BAD;
    }
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

/* We need to be careful here to do 8 byte alignment */
/* when reading any of the "new" style signatures.   */

int readSignature(int fd, short sig_type, void **sig)
{
    unsigned char buf[2048];
    int length;
    
    switch (sig_type) {
    case RPMSIG_NONE:
	if (sig) {
	    *sig = NULL;
	}
	break;
    case RPMSIG_PGP262_1024:
	/* These are always 256 bytes */
	if (read(fd, buf, 256) != 256) {
	   return 0;
	}
	if (sig) {
	    *sig = malloc(152);
	    memcpy(*sig, buf, 152);
	}
	break;
    case RPMSIG_MD5:
	/* 8 byte aligned */
	if (read(fd, buf, 16) != 16) {
	    return 0;
	}
	if (sig) {
	    *sig = malloc(16);
	    memcpy(*sig, buf, 16);
	}
	break;
    case RPMSIG_MD5_PGP:
	/* 8 byte aligned */
	if (read(fd, buf, 18) != 18) {
	    return 0;
	}
	length = buf[16] * 256 + buf[17];
	if (read(fd, buf + 18, length) != length) {
	    return 0;
	}
	if (sig) {
	    *sig = malloc(18 + length);
	    memcpy(*sig, buf, 18 + length);
	}
	/* The is the align magic */
	length = (8 - ((length + 18) % 8)) % 8;
	if (length) {
	    if (read(fd, buf, length) != length) {
		return 0;
	    }
	}
	break;
    default:
	return 0;
    }

    return 1;
}

int makeSignature(char *file, short sig_type, int ofd, char *passPhrase)
{
    switch (sig_type) {
    case RPMSIG_PGP262_1024:
	/* We no longer generate these */
#if 0	
	if (! getVar(RPMVAR_PGP_NAME)) {
	    error(RPMERR_SIGGEN, "You must set \"pgp_name:\" in /etc/rpmrc\n");
	    return RPMERR_SIGGEN;
	}
	if ((res = makePGPSignature(file, ofd, passPhrase)) == -1) {
	    /* This is the 151 byte sig hack */
	    return makePGPSignature(file, ofd, passPhrase);
	}
        return res;
#endif
	error(RPMERR_SIGGEN, "Internal error! RPMSIG_PGP262_1024\n");
	return RPMERR_SIGGEN;
	break;
    case RPMSIG_MD5:
	if (makeMD5Signature(file, ofd)) {
	    error(RPMERR_SIGGEN, "Unable to generate MD5 signature\n");
	    return RPMERR_SIGGEN;
	}
	break;
    case RPMSIG_MD5_PGP:
	if (! getVar(RPMVAR_PGP_NAME)) {
	    error(RPMERR_SIGGEN, "You must set \"pgp_name:\" in /etc/rpmrc\n");
	    return RPMERR_SIGGEN;
	}
	if (makeMD5Signature(file, ofd)) {
	    error(RPMERR_SIGGEN, "Unable to generate MD5 signature\n");
	    return RPMERR_SIGGEN;
	}
	/* makePGPSignature() takes care of 8 byte alignment */
	if (makePGPSignature(file, ofd, passPhrase)) {
	    error(RPMERR_SIGGEN, "Unable to generate PGP signature\n");
	    return RPMERR_SIGGEN;
	}
	break;
    case RPMSIG_NONE:
    }

    return 0;
}

static int makeMD5Signature(char *file, int ofd)
{
    unsigned char sig[16];

    mdbinfile(file, sig);
    if (write(ofd, sig, 16) != 16) {
	return 1;
    }
    
    return 0;
}

/* Be sure to handle 8 byte alignment! */

static int makePGPSignature(char *file, int ofd, char *passPhrase)
{
    unsigned char length[2];
    char name[1024];
    char sigfile[1024];
    int pid, status, len;
    int fd, inpipe[2];
    unsigned char sigbuf[2048];   /* 1024bit sig is ~152 bytes */
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

    /* Fill in the length, data, and then 8 byte alignment magic */
    len = statbuf.st_size;
    length[0] = len / 256;
    length[1] = len % 256;
    write(ofd, length, 2);
    
    fd = open(sigfile, O_RDONLY);
    if (read(fd, sigbuf, len) != len) {
	unlink(sigfile);
	close(fd);
	error(RPMERR_SIGGEN, "unable to read the signature");
	return 1;
    }
    if (write(ofd, sigbuf, len) != len) {
	unlink(sigfile);
	close(fd);
	error(RPMERR_SIGGEN, "unable to write the signature");
	return 1;
    }
    close(fd);
    unlink(sigfile);

    message(MESS_DEBUG, "Wrote %d bytes of PGP sig\n", len);
    
    /* Now the alignment */
    len = (8 - ((len + 18) % 8)) % 8;
    if (len) {
	memset(sigbuf, 0xff, len);
	if (write(ofd, sigbuf, len) != len) {
	    close(fd);
	    error(RPMERR_SIGGEN, "unable to write the alignment cruft");
	    return 1;
	}
	message(MESS_DEBUG, "Wrote %d bytes of alignment cruft\n", len);
    }
    
    return 0;
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
