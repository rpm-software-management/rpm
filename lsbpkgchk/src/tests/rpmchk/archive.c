#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>
#include <cpio.h>
#include "rpmchk.h"
#include "md5.h"
#include "tagfuncs.h"
#include "../tetj/tetj.h"

MD5_CTX	md5ctx;
unsigned char	fbuf[1024];

void
checkRpmArchive(RpmFile *file1, struct tetj_handle *journal)
{
#define TMP_STRING_SIZE (400)
char tmp_string[TMP_STRING_SIZE+1];
unsigned char	md5sum[17],md5str[33];
unsigned char	*fmd5=filemd5s,*flinktos=filelinktos;
gzFile	*zfile;
RpmArchiveHeader ahdr;
int	startoffset,endoffset;
int	fileindex=0;
int	filesizesum=0;

file1->archive=(caddr_t)file1->nexthdr;

/*
fprintf(stderr,"checkRpmArchive() archive=%x\n", (int)file1->archive );
*/

/* Check the RpmHeader magic value */
tetj_tp_count++;
tetj_purpose_start(journal, tetj_activity_count, tetj_tp_count, "Check magic value");
if( !( file1->archive[0]==(char)0x1f
    && file1->archive[1]==(char)0x8b) ) {
        snprintf( tmp_string, TMP_STRING_SIZE,
    "checkRpmArchive: magic isn't expected value 0x1f8b, found %x%x instead\n",
	     (unsigned int)file1->archive[0], (unsigned int)file1->archive[1]); 
        fprintf(stderr, "%s\n", tmp_string);
        tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count,
							0, 0, 0, tmp_string);
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL);
} else {
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_PASS); 
}
tetj_purpose_end(journal, tetj_activity_count, tetj_tp_count); 

/*
 * Now we need to set up zlib so that we can read/decompres the archive.
 */

if( lseek(file1->fd, (file1->archive-file1->addr), SEEK_SET) < 0 ) {
        snprintf( tmp_string, TMP_STRING_SIZE,
    "checkRpmArchive: Unable to seek to start of archive\n");
        fprintf(stderr, "%s\n", tmp_string);
        tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count,
							0, 0, 0, tmp_string);
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL);
	return;
	}

if( (zfile=gzdopen(file1->fd,"r")) == NULL ) {
        snprintf( tmp_string, TMP_STRING_SIZE,
    "checkRpmArchive: Unable to open compressed archive\n");
        fprintf(stderr, "%s\n", tmp_string);
        tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count,
							0, 0, 0, tmp_string);
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL);
	return;
	}

startoffset=gztell(zfile);
/*
 * The archive is really a cpio format file, so start reading records
 * and examining them.
 */

while( !gzeof(zfile) ) {
	char	*fptr,*fmt,filename[PATH_MAX+1],tagfilename[PATH_MAX+1];
	char	num[9];
	int	size,mode,devmaj,devmin,flink,fino;
	time_t	ftime;

	gzread(zfile, &ahdr, sizeof(ahdr) );
/*
	printf("***************************\n");
	printf("Magic: %6.6s\n", ahdr.c_magic );
	printf("ino: %8.8s\n", ahdr.c_ino );
	printf("Mode: %8.8s\n", ahdr.c_mode );
	printf("Rdev: %8.8s,%8.8s\n", ahdr.c_rdevmajor,ahdr.c_rdevminor );
	printf("mtime: %8.8s\n", ahdr.c_mtime );
	printf("nlink: %8.8s\n", ahdr.c_nlink );
	printf("filesize: %8.8s\n", ahdr.c_filesize );
	printf("namesize: %8.8s\n", ahdr.c_namesize );
	printf("UID: %8.8s\n", ahdr.c_uid );
	printf("GID: %8.8s\n", ahdr.c_gid );
*/
	if( !(strncmp(ahdr.c_magic,"070701",6) == 0) ) {
        	snprintf( tmp_string, TMP_STRING_SIZE,
    		"checkRpmArchive: Archive record has wrong magic %6.6s instead of 070701",
		ahdr.c_magic);
        	fprintf(stderr, "%s\n", tmp_string);
        	tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count,
							0, 0, 0, tmp_string);
        	tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL);
	}
	/* Read in the filename */
	memcpy(num,ahdr.c_namesize,8);
	num[8]=0; /* NULL terminate the namesize */
	size=strtol(num,NULL,16);
	gzread(zfile, filename, size );
	/*
	 * Check/fix padding here - the amount of space used for the header
	 * is rounded up to the long-word (32 its), so 1-3 bytes of padding
	 * may need to be skipped.
	 */
	size=gztell(zfile);
	size%=4;
	size=4-size;
	size%=4;
	//printf("padding %d\n", size);
	gzseek(zfile,size,SEEK_CUR);

	/*
	 * Check for the end of the archive
	 */
	if( strcmp(filename,"TRAILER!!!") == 0 ) {
		/* End of archive */
		break;
		}

	/* Skip the file contents */
	memcpy(num,ahdr.c_filesize,8);
	num[8]=0;
	size=strtol(num,NULL,16);

	/* Get the mode so we can idendify directories */
	memcpy(num,ahdr.c_mode,8);
	num[8]=0;
	mode=strtol(num,NULL,16);

	/*
	 * Check the file name against the RPMTAG_DIRNAME, RPMTAG_DIRINDEXES,
	 * RPMTAG_BASENAME values.
	 */

	if(hasPayloadFilesHavePrefix) {
		fmt=".%s%s";
	} else {
		fmt="%s%s";
	}
	if( dirindicies[fileindex] <= numdirnames ) {
		/*
		fprintf(stderr,"dirindex: %x\n", dirindicies[fileindex]);
		fprintf(stderr,"dirname: %s\n",
					dirnames[dirindicies[fileindex]]);
		fprintf(stderr,"basename: %s\n", basenames[fileindex]);
		*/
		sprintf(tagfilename,fmt,
			dirnames[dirindicies[fileindex]],basenames[fileindex]);

		if( strcmp(filename,tagfilename) != 0 ) {
			fprintf(stderr,
		    "Payload filename %s doesn't match RPMTAG based name %s\n",
			filename, tagfilename);
		}
	} else {
		fprintf(stderr,"dirindex out of range!!!\n");
	}

	/*
	 * Check the file size against the RPMTAG_FILESIZES value
	 */

	/* Directories have no size, but RPMTAG_FILESIZES sez 1024 */
	if( S_ISREG(mode) && (size != filesizes[fileindex]) ) {
		fprintf(stderr,"Filesize (%d) for %s not that same a specified in RPMTAG_FILESIZES (%d)\n", size, filename, filesizes[fileindex] );
	}
	filesizesum+=size;

	/*
	 * Check the file modes against the RPMTAG_FILEMODES value
	 */

	if( (mode != filemodes[fileindex]) ) {
		fprintf(stderr,"Filemode  (%o) for %s not that same a specified in RPMTAG_FILEMODES (%o)\n", mode, filename, filemodes[fileindex] );
	}

	/*
	 * Check the file dev against the RPMTAG_FILEDEVICES value
	 */

	memcpy(num,ahdr.c_devmajor,8);
	num[8]=0;
	devmaj=strtol(num,NULL,16);

	memcpy(num,ahdr.c_devminor,8);
	num[8]=0;
	devmin=strtol(num,NULL,16);

	if( (makedev(devmaj,devmin) != filedevs[fileindex]) ) {
		fprintf(stderr,"File dev (%x) for %s not that same a specified in RPMTAG_FILEDEVICES (%x)\n", makedev(devmaj,devmin), filename, filedevs[fileindex] );
	}

	/*
	 * Check the file rdev against the RPMTAG_FILERDEVS value
	 */

	memcpy(num,ahdr.c_rdevmajor,8);
	num[8]=0;
	devmaj=strtol(num,NULL,16);

	memcpy(num,ahdr.c_rdevminor,8);
	num[8]=0;
	devmin=strtol(num,NULL,16);

	if( (makedev(devmaj,devmin) != filerdevs[fileindex]) ) {
		fprintf(stderr,"File rdev (%x) for %s not that same a specified in RPMTAG_FILERDEVS (%x)\n", makedev(devmaj,devmin), filename, filerdevs[fileindex] );
	}

	/*
	 * Check the file modes against the RPMTAG_FILEMTIMES value
	 */

	memcpy(num,ahdr.c_mtime,8);
	num[8]=0;
	ftime=strtol(num,NULL,16);

	if( (ftime != filetimes[fileindex]) ) {
		fprintf(stderr,"File time  (%x) for %s not that same a specified in RPMTAG_FILEMTIMES (%x)\n", (unsigned int)ftime, filename, filetimes[fileindex] );
	}

	/*
	 * Check the file modes against the RPMTAG_FILEINODES value
	 */

	memcpy(num,ahdr.c_ino,8);
	num[8]=0;
	fino=strtol(num,NULL,16);

	if( (fino != fileinodes[fileindex]) ) {
		fprintf(stderr,"File inode  (%x) for %s not that same a specified in RPMTAG_FILEINODES (%x)\n", (unsigned int)fino, filename, fileinodes[fileindex] );
	}

	/*
	 * Check the file modes against the RPMTAG_FILEMD5S value
	 */

	if( S_ISREG(mode) ) {
		MD5Init(&md5ctx);
		while ( size>0 ) {
			gzread(zfile,fbuf,size>1024?1024:size);
			MD5Update(&md5ctx,fbuf,size>1024?1024:size);
			size-=1024;
			}
		MD5Final(md5sum,&md5ctx);
		sprintf(md5str,"%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
			md5sum[0], md5sum[1], md5sum[2], md5sum[3],
			md5sum[4], md5sum[5], md5sum[6], md5sum[7],
			md5sum[8], md5sum[9], md5sum[10], md5sum[11],
			md5sum[12], md5sum[13], md5sum[14], md5sum[15] );

		if( strncmp(fmd5,md5str,16) != 0 ) {
			fprintf(stderr,"File MD5 (%s) for %s does not match value in RPMTAG_FILEMD5S (%s)\n", md5str, filename, fmd5 );
			}
	}
	fmd5+=strlen(fmd5)+1;

	/*
	 * Check the file modes against the RPMTAG_FILELINKTOS value
	 */

	memcpy(num,ahdr.c_nlink,8);
	num[8]=0;
	flink=strtol(num,NULL,16);

	if( S_ISREG(mode) && flink>1 && !*flinktos ) {
		fprintf(stderr,"File link expected, but no FILELINKTOS entry\n");
	}
	if( S_ISREG(mode) && flink==1 && *flinktos ) {
		fprintf(stderr,"File link not expected, but FILELINKTOS present\n");
	}
	filelinktos+=strlen(filelinktos)+1;


	/*
	 * Check/fix padding here - the amount of space used for the file
	 * is rounded up to the long-word (32 its), so 1-3 bytes of padding
	 * may need to be skipped.
	 */
	size=gztell(zfile);
	//printf("offset %x\n", size);
	size%=4;
	size=4-size;
	size%=4;
	//printf("padding %d\n", size);
	gzseek(zfile,size,SEEK_CUR);


	/* Now, check the filename */
	if( filename[0] == '.' && filename[1] == '/' )
		fptr=&filename[1];
	else
		fptr=&filename[0];
	checkRpmArchiveFilename(fptr, journal);

	fileindex++;
	}

endoffset=gztell(zfile);

/*
fprintf(stderr,"%d bytes in uncompressed archive\n", endoffset-startoffset);
*/
if( endoffset-startoffset != archivesize ) {
		fprintf(stderr,"Archive size (%d) does ",endoffset-startoffset);
		fprintf(stderr,"not match the value in RPMTAG_ARCHIVESIZE (%d)\n",
							archivesize );
	}

/*
fprintf(stderr,"%d bytes in archive files\n", filesizesum);
*/
if( filesizesum != rpmtagsize ) {
		fprintf(stderr,"Sum of file sizes (%d) does ",filesizesum);
		fprintf(stderr,"not match the value in RPMTAG_SIZE (%d)\n",
							rpmtagsize );
	}
}
