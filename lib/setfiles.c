/* 
 * setfiles
 *
 * AUTHOR:  Stephen Smalley <sds@epoch.ncsc.mil>
 * This program was derived in part from the setfiles.pl script
 * developed by Secure Computing Corporation.
 *
 * PURPOSE:
 * This program reads a set of file security context specifications
 * based on pathname regular expressions and labels files
 * accordingly, traversing a set of file systems specified by
 * the user.  The program does not cross file system boundaries.
 *
 * USAGE:
 * setfiles [-dnpqsvW] spec_file pathname...
 * 
 * -d   Show what specification matched each file.
 * -n	Do not change any file labels.
 * -q   Be quiet (suppress non-error output).
 * -r   Use an alternate root path
 * -s   Use stdin for a list of files instead of searching a partition.
 * -v	Show changes in file labels.  
 * -W   Warn about entries that have no matching file.
 *
 * spec_file	The specification file.
 * pathname...	The file systems to label (omit if using -s).	
 *
 * EXAMPLE USAGE:
 * ./setfiles -v file_contexts `mount | awk '/ext3/{print $3}'`
 *
 * SPECIFICATION FILE:
 * Each specification has the form:
 *       regexp [ -type ] ( context | <<none>> )
 *
 * By default, the regexp is an anchored match on both ends (i.e. a 
 * caret (^) is prepended and a dollar sign ($) is appended automatically).
 * This default may be overridden by using .* at the beginning and/or
 * end of the regular expression.  
 *
 * The optional type field specifies the file type as shown in the mode
 * field by ls, e.g. use -d to match only directories or -- to match only
 * regular files.
 * 
 * The value of <<none> may be used to indicate that matching files
 * should not be relabeled.
 *
 * The last matching specification is used.
 *
 * If there are multiple hard links to a file that match 
 * different specifications and those specifications indicate
 * different security contexts, then a warning is displayed
 * but the file is still labeled based on the last matching
 * specification other than <<none>>.
 */

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <regex.h>
#include <sys/vfs.h>
#define __USE_XOPEN_EXTENDED 1	/* nftw */
#include <ftw.h>
#include <limits.h>
#include <selinux/selinux.h>

static int add_assoc = 1;

/*
 * Command-line options.
 */
static int debug = 0;
static int change = 1;
static int quiet = 0;
#define QPRINTF(args...) do { if (!quiet) printf(args); } while (0)
static int use_stdin = 0;
static int verbose = 0;
static int warn_no_match = 0;
static char *rootpath = NULL;
static int rootpathlen = 0;

/*
 * Program name and error message buffer.
 */
static char *progname;
static char errbuf[255 + 1];

/*
 * A file security context specification.
 */
typedef struct spec {
	char *regex_str;	/* regular expession string for diagnostic messages */
	char *type_str;		/* type string for diagnostic messages */
	char *context;		/* context string */
	regex_t regex;		/* compiled regular expression */
	mode_t mode;		/* mode format value */
	int matches;		/* number of matching pathnames */
	int hasMetaChars; 	/* indicates whether the RE has 
				   any meta characters.  
				   0 = no meta chars 
				   1 = has one or more meta chars */
	int stem_id;		/* indicates which of the stem-compression
				 * items it matches */
} spec_t;

typedef struct stem {
	char *buf;
	int len;
} stem_t;

stem_t *stem_arr = NULL;
int num_stems = 0;
int alloc_stems = 0;

const char * const regex_chars = ".^$?*+|[({";

/* Return the length of the text that can be considered the stem, returns 0
 * if there is no identifiable stem */
static int get_stem_from_spec(const char * const buf)
{
	const char *tmp = strchr(buf + 1, '/');
	const char *ind;

	if(!tmp)
		return 0;

	for(ind = buf + 1; ind < tmp; ind++)
	{
		if(strchr(regex_chars, (int)*ind))
			return 0;
	}
	return tmp - buf;
}

/* return the length of the text that is the stem of a file name */
static int get_stem_from_file_name(const char * const buf)
{
	const char *tmp = strchr(buf + 1, '/');

	if(!tmp)
		return 0;
	return tmp - buf;
}

/* find the stem of a file spec, returns the index into stem_arr for a new
 * or existing stem, (or -1 if there is no possible stem - IE for a file in
 * the root directory or a regex that is too complex for us).  Makes buf
 * point to the text AFTER the stem. */
static int find_stem_from_spec(const char **buf)
{
	int i;
	int stem_len = get_stem_from_spec(*buf);

	if(!stem_len)
		return -1;
	for(i = 0; i < num_stems; i++)
	{
		if(stem_len == stem_arr[i].len && !strncmp(*buf, stem_arr[i].buf, stem_len))
		{
			*buf += stem_len;
			return i;
		}
	}
	if(num_stems == alloc_stems)
	{
		alloc_stems = alloc_stems * 2 + 16;
		stem_arr = realloc(stem_arr, sizeof(stem_t) * alloc_stems);
		if(!stem_arr) {
			fprintf(stderr, "Unable to grow stem array to %d entries:  out of memory.\n", alloc_stems);
			exit(1);
		}
	}
	stem_arr[num_stems].len = stem_len;
	stem_arr[num_stems].buf = malloc(stem_len + 1);
	if(!stem_arr[num_stems].buf) {
		fprintf(stderr, "Unable to allocate new stem of length %d:  out of memory.\n", stem_len+1);
		exit(1);
	}
	memcpy(stem_arr[num_stems].buf, *buf, stem_len);
	stem_arr[num_stems].buf[stem_len] = '\0';
	num_stems++;
	*buf += stem_len;
	return num_stems - 1;
}

/* find the stem of a file name, returns the index into stem_arr (or -1 if
 * there is no match - IE for a file in the root directory or a regex that is
 * too complex for us).  Makes buf point to the text AFTER the stem. */
static int find_stem_from_file(const char **buf)
{
	int i;
	int stem_len = get_stem_from_file_name(*buf);

	if(!stem_len)
		return -1;
	for(i = 0; i < num_stems; i++)
	{
		if(stem_len == stem_arr[i].len && !strncmp(*buf, stem_arr[i].buf, stem_len))
		{
			*buf += stem_len;
			return i;
		}
	}
	return -1;
}

/*
 * The array of specifications, initially in the
 * same order as in the specification file.
 * Sorting occurs based on hasMetaChars
 */
static spec_t *spec_arr;
static int nspec;

/*
 * An association between an inode and a 
 * specification.  
 */
typedef struct file_spec {
	ino_t ino;		/* inode number */
	int specind;		/* index of specification in spec */
	char *file;		/* full pathname for diagnostic messages about conflicts */
	struct file_spec *next;	/* next association in hash bucket chain */
} file_spec_t;

/*
 * The hash table of associations, hashed by inode number.
 * Chaining is used for collisions, with elements ordered
 * by inode number in each bucket.  Each hash bucket has a dummy 
 * header.
 */
#define HASH_BITS 16
#define HASH_BUCKETS (1 << HASH_BITS)
#define HASH_MASK (HASH_BUCKETS-1)
static file_spec_t fl_head[HASH_BUCKETS];

/*
 * Try to add an association between an inode and
 * a specification.  If there is already an association
 * for the inode and it conflicts with this specification,
 * then use the specification that occurs later in the
 * specification array.
 */
static file_spec_t *file_spec_add(ino_t ino, int specind, const char *file)
{
	file_spec_t *prevfl, *fl;
	int h, no_conflict, ret;
	struct stat sb;

	h = (ino + (ino >> HASH_BITS)) & HASH_MASK;
	for (prevfl = &fl_head[h], fl = fl_head[h].next; fl;
	     prevfl = fl, fl = fl->next) {
		if (ino == fl->ino) {
			ret = lstat(fl->file, &sb);
			if (ret < 0 || sb.st_ino != ino) {
				fl->specind = specind;
				free(fl->file);
				fl->file = malloc(strlen(file) + 1);
				if (!fl->file) {
					fprintf(stderr,
						"%s:  insufficient memory for file label entry for %s\n",
						progname, file);
					return NULL;
				}
				strcpy(fl->file, file);
				return fl;

			}

			no_conflict = (strcmp(spec_arr[fl->specind].context,spec_arr[specind].context) == 0);
			if (no_conflict)
				return fl;

			fprintf(stderr,
				"%s:  conflicting specifications for %s and %s, using %s.\n",
				progname, file, fl->file,
				((specind > fl->specind) ? spec_arr[specind].
				 context : spec_arr[fl->specind].context));
			fl->specind =
			    (specind >
			     fl->specind) ? specind : fl->specind;
			free(fl->file);
			fl->file = malloc(strlen(file) + 1);
			if (!fl->file) {
				fprintf(stderr,
					"%s:  insufficient memory for file label entry for %s\n",
					progname, file);
				return NULL;
			}
			strcpy(fl->file, file);
			return fl;
		}

		if (ino > fl->ino)
			break;
	}

	fl = malloc(sizeof(file_spec_t));
	if (!fl) {
		fprintf(stderr,
			"%s:  insufficient memory for file label entry for %s\n",
			progname, file);
		return NULL;
	}
	fl->ino = ino;
	fl->specind = specind;
	fl->file = malloc(strlen(file) + 1);
	if (!fl->file) {
		fprintf(stderr,
			"%s:  insufficient memory for file label entry for %s\n",
			progname, file);
		return NULL;
	}
	strcpy(fl->file, file);
	fl->next = prevfl->next;
	prevfl->next = fl;
	return fl;
}

/*
 * Evaluate the association hash table distribution.
 */
static void file_spec_eval(void)
{
	file_spec_t *fl;
	int h, used, nel, len, longest;

	used = 0;
	longest = 0;
	nel = 0;
	for (h = 0; h < HASH_BUCKETS; h++) {
		len = 0;
		for (fl = fl_head[h].next; fl; fl = fl->next) {
			len++;
		}
		if (len)
			used++;
		if (len > longest)
			longest = len;
		nel += len;
	}

	QPRINTF
	    ("%s:  hash table stats: %d elements, %d/%d buckets used, longest chain length %d\n",
	     progname, nel, used, HASH_BUCKETS, longest);
}


/*
 * Destroy the association hash table.
 */
static void file_spec_destroy(void)
{
	file_spec_t *fl, *tmp;
	int h;

	for (h = 0; h < HASH_BUCKETS; h++) {
		fl = fl_head[h].next;
		while (fl) {
			tmp = fl;
			fl = fl->next;
			free(tmp->file);
			free(tmp);
		}
		fl_head[h].next = NULL;
	}
}


static int match(const char *name, struct stat *sb)
{
	int i, ret, file_stem;
	const char *fullname = name;
	const char *buf = name;

	/* fullname will be the real file that gets labeled
	 * name will be what is matched in the policy */
	if (NULL != rootpath) {
		if (0 != strncmp(rootpath, name, rootpathlen)) {
			fprintf(stderr, "%s:  %s is not located in %s\n", 
				progname, name, rootpath);
			return -1;
		}
		name += rootpathlen;
		buf += rootpathlen;
	}

	ret = lstat(fullname, sb);
	if (ret) {
		fprintf(stderr, "%s:  unable to stat file %s\n", progname,
			fullname);
		return -1;
	}

	file_stem = find_stem_from_file(&buf);

	/* 
	 * Check for matching specifications in reverse order, so that
	 * the last matching specification is used.
	 */
	for (i = nspec - 1; i >= 0; i--)
	{
		/* if the spec in question matches no stem or has the same
		 * stem as the file AND if the spec in question has no mode
		 * specified or if the mode matches the file mode then we do
		 * a regex check	*/
		if( (spec_arr[i].stem_id == -1 || spec_arr[i].stem_id == file_stem)
		  && (!spec_arr[i].mode || ( (sb->st_mode & S_IFMT) == spec_arr[i].mode ) ) )
		{
			if(spec_arr[i].stem_id == -1)
				ret = regexec(&spec_arr[i].regex, name, 0, NULL, 0);
			else
				ret = regexec(&spec_arr[i].regex, buf, 0, NULL, 0);
			if (ret == 0)
				break;

			if (ret == REG_NOMATCH)
				continue;
			/* else it's an error */
			regerror(ret, &spec_arr[i].regex, errbuf, sizeof errbuf);
			fprintf(stderr,
				"%s:  unable to match %s against %s:  %s\n",
				progname, name, spec_arr[i].regex_str, errbuf);
			return -1;
		}
	}

	if (i < 0)
		/* No matching specification. */
		return -1;

	spec_arr[i].matches++;

	return i;
}

/* Used with qsort to sort specs from lowest to highest hasMetaChars value */
static int spec_compare(const void* specA, const void* specB)
{
	return(
		((const struct spec *)specB)->hasMetaChars -
		((const struct spec *)specA)->hasMetaChars
		); 
}

/*
 * Check for duplicate specifications. If a duplicate specification is found 
 * and the context is the same, give a warning to the user. If a duplicate 
 * specification is found and the context is different, give a warning
 * to the user (This could be changed to error). Return of non-zero is an error.
 */
static int nodups_specs(void)
{
	int ii, jj;
	struct spec *curr_spec;

	for (ii = 0; ii < nspec; ii++) {
		curr_spec = &spec_arr[ii];
		for (jj = ii + 1; jj < nspec; jj++) { 
				/* Check if same RE string */
			if ((!strcmp(spec_arr[jj].regex_str, curr_spec->regex_str))
									&&
				(!spec_arr[jj].mode || !curr_spec->mode 
				 || spec_arr[jj].mode == curr_spec->mode)) {
				/* Same RE string found */
				if (strcmp(spec_arr[jj].context, curr_spec->context)) {
					/* If different contexts, give warning */
					fprintf(stderr,
					"ERROR: Multiple different specifications for %s  (%s and %s).\n",
						curr_spec->regex_str, 
						spec_arr[jj].context,
						curr_spec->context);
				}
				else {
					/* If same contexts give warning */
					fprintf(stderr,
					"WARNING: Multiple same specifications for %s.\n",
						curr_spec->regex_str);
				}
			}
		}
	}
	return 0;
}

static void usage(const char * const name)
{
	fprintf(stderr,
		"usage:  %s [-dnqvW] spec_file pathname...\n"
		"usage:  %s -s [-dnqvW] spec_file\n", name, name);
	exit(1);
}

/* Determine if the regular expression specification has any meta characters. */
static void spec_hasMetaChars(struct spec *spec)
{
	char *c;
	int len;
	char *end;

	c = spec->regex_str;
	len = strlen(spec->regex_str);
	end = c + len;

	spec->hasMetaChars = 0; 

	/* Look at each character in the RE specification string for a 
	 * meta character. Return when any meta character reached. */
	while (c != end) {
		switch(*c) {
			case '.':
			case '^':
			case '$':
			case '?':
			case '*':
			case '+':
			case '|':
			case '[':
			case '(':
			case '{':
				spec->hasMetaChars = 1;
				return;
			case '\\':		/* skip the next character */
				c++;
				break;
			default:
				break;

		}
		c++;
	}
	return;
}

#define SZ 255

/*
 * Apply the last matching specification to a file.
 * This function is called by nftw on each file during
 * the directory traversal.
 */
static int apply_spec(const char *file,
		      const struct stat *sb_unused, int flag, struct FTW *s_unused)
{
	const char *my_file;
	file_spec_t *fl;
	struct stat my_sb;
	int i, ret;
	char *context; 

	/* Skip the extra slash at the beginning, if present. */
	if (file[0] == '/' && file[1] == '/')
		my_file = &file[1];
	else
		my_file = file;

	if (flag == FTW_DNR) {
		fprintf(stderr, "%s:  unable to read directory %s\n",
			progname, my_file);
		return 0;
	}

	i = match(my_file, &my_sb);
	if (i < 0)
		/* No matching specification. */
		return 0;

	/*
	 * Try to add an association between this inode and
	 * this specification.  If there is already an association
	 * for this inode and it conflicts with this specification,
	 * then use the last matching specification.
	 */
	if (add_assoc) {
		fl = file_spec_add(my_sb.st_ino, i, my_file);
		if (!fl)
			/* Insufficient memory to proceed. */
			return 1;

		if (fl->specind != i)
			/* There was already an association and it took precedence. */
			return 0;
	}

	if (debug) {
		if (spec_arr[i].type_str) {
			printf("%s:  %s matched by (%s,%s,%s)\n", progname,
			       my_file, spec_arr[i].regex_str,
			       spec_arr[i].type_str, spec_arr[i].context);
		} else {
			printf("%s:  %s matched by (%s,%s)\n", progname,
			       my_file, spec_arr[i].regex_str,
			       spec_arr[i].context);
		}
	}

	/* Get the current context of the file. */
	ret = lgetfilecon(my_file, &context);
	if (ret < 0) {
		if (errno == ENODATA) {
			context = malloc(10);
			strcpy(context, "<<none>>");
		} else {
			perror(my_file);
			fprintf(stderr, "%s:  unable to obtain attribute for file %s\n", 
				progname, my_file);
			return -1;
		}
	}

	/*
	 * Do not relabel the file if the matching specification is 
	 * <<none>> or the file is already labeled according to the 
	 * specification.
	 */
	if ((strcmp(spec_arr[i].context, "<<none>>") == 0) || 
	    (strcmp(context,spec_arr[i].context) == 0)) {
		freecon(context);
		return 0;
	}

	if (verbose) {
		printf("%s:  relabeling %s from %s to %s\n", progname,
		       my_file, context, spec_arr[i].context);
	}

	freecon(context);

	/*
	 * Do not relabel the file if -n was used.
	 */
	if (!change)
		return 0;

	/*
	 * Relabel the file to the specified context.
	 */
	ret = lsetfilecon(my_file, spec_arr[i].context);
	if (ret) {
		perror(my_file);
		fprintf(stderr, "%s:  unable to relabel %s to %s\n",
			progname, my_file, spec_arr[i].context);
		return 1;
	}

	return 0;
}

static void set_rootpath(const char *arg)
{
	int len;

	rootpath = strdup(arg);
	if (NULL == rootpath) {
		fprintf(stderr, "%s:  insufficient memory for rootpath\n", 
			progname);
		exit(1);
	}

	/* trim trailing /, if present */
	len = strlen(rootpath);
	while ('/' == rootpath[len - 1])
		rootpath[--len] = 0;
	rootpathlen = len;
}

static int nerr = 0;

static void inc_err(void)
{
    nerr++;
    if(nerr > 9 && !debug) {
	fprintf(stderr, "Exiting after 10 errors.\n");
	exit(1);
    }
}

static
int parseREContexts(const char *fn)
{
    FILE * fp;
    char line_buf[255 + 1];
    char * buf_p;
    char * regex;
    char * type;
    char * context;
    char * anchored_regex;
    int items;
    int len;
    int lineno;
    int pass;
    int regerr;

    if ((fp = fopen(fn, "r")) == NULL) {
	perror(fn);
	return -1;
    }

    /* 
     * Perform two passes over the specification file.
     * The first pass counts the number of specifications and
     * performs simple validation of the input.  At the end
     * of the first pass, the spec array is allocated.
     * The second pass performs detailed validation of the input
     * and fills in the spec array.
     */
    for (pass = 0; pass < 2; pass++) {
	lineno = 0;
	nspec = 0;
	while (fgets(line_buf, sizeof line_buf, fp)) {
	    lineno++;
	    len = strlen(line_buf);
	    if (line_buf[len - 1] != '\n') {
		fprintf(stderr,
			"%s:  no newline on line number %d (only read %s)\n",
			fn, lineno, line_buf);
		inc_err();
		continue;
	    }
	    line_buf[len - 1] = 0;
	    buf_p = line_buf;
	    while (isspace(*buf_p))
		buf_p++;
	    /* Skip comment lines and empty lines. */
	    if (*buf_p == '#' || *buf_p == 0)
		continue;
	    items = sscanf(line_buf, "%as %as %as", &regex, &type, &context);
	    if (items < 2) {
		fprintf(stderr,
			"%s:  line number %d is missing fields (only read %s)\n",
			fn, lineno, line_buf);
		inc_err();
		if (items == 1)
		    free(regex);
		continue;
	    } else if (items == 2) {
		/* The type field is optional. */
		free(context);
		context = type;
		type = 0;
	    }

	    if (pass == 1) {
		/* On the second pass, compile and store the specification in spec. */
		const char *reg_buf = regex;
		spec_arr[nspec].stem_id = find_stem_from_spec(&reg_buf);
		spec_arr[nspec].regex_str = regex;

		/* Anchor the regular expression. */
		len = strlen(reg_buf);
		anchored_regex = malloc(len + 3);
		if (!anchored_regex) {
		    fprintf(stderr,
			"%s:  insufficient memory for anchored regexp on line %d\n",
			fn, lineno);
		    return -1;
		}
		sprintf(anchored_regex, "^%s$", reg_buf);

		/* Compile the regular expression. */
		regerr = regcomp(&spec_arr[nspec].regex,
			    anchored_regex,
			    REG_EXTENDED | REG_NOSUB);
		if (regerr < 0) {
		    regerror(regerr, &spec_arr[nspec].regex,
				 errbuf, sizeof errbuf);
		    fprintf(stderr,
			"%s:  unable to compile regular expression %s on line number %d:  %s\n",
			fn, regex, lineno,
			errbuf);
		    inc_err();
		}
		free(anchored_regex);

		/* Convert the type string to a mode format */
		spec_arr[nspec].type_str = type;
		spec_arr[nspec].mode = 0;
		if (!type)
		    goto skip_type;
		len = strlen(type);
		if (type[0] != '-' || len != 2) {
		    fprintf(stderr,
			"%s:  invalid type specifier %s on line number %d\n",
			fn, type, lineno);
		    inc_err();
		    goto skip_type;
		}
		switch (type[1]) {
		case 'b':
		    spec_arr[nspec].mode = S_IFBLK;
		    break;
		case 'c':
		    spec_arr[nspec].mode = S_IFCHR;
		    break;
		case 'd':
		    spec_arr[nspec].mode = S_IFDIR;
		    break;
		case 'p':
		    spec_arr[nspec].mode = S_IFIFO;
		    break;
		case 'l':
		    spec_arr[nspec].mode = S_IFLNK;
		    break;
		case 's':
		    spec_arr[nspec].mode = S_IFSOCK;
		    break;
		case '-':
		    spec_arr[nspec].mode = S_IFREG;
		    break;
		default:
		    fprintf(stderr,
			"%s:  invalid type specifier %s on line number %d\n",
			fn, type, lineno);
		    inc_err();
		}

	      skip_type:

		spec_arr[nspec].context = context;

		if (strcmp(context, "<<none>>")) {
		    if (security_check_context(context) < 0 && errno != ENOENT) {
			fprintf(stderr,
				"%s:  invalid context %s on line number %d\n",
				fn, context, lineno);
			inc_err();
		    }
		}

		/* Determine if specification has 
		 * any meta characters in the RE */
		spec_hasMetaChars(&spec_arr[nspec]);
	    }

	    nspec++;
	    if (pass == 0) {
		free(regex);
		if (type)
		    free(type);
		free(context);
	    }
	}

	if (nerr)
	    return -1;

	if (pass == 0) {
	    QPRINTF("%s:  read %d specifications\n", fn, nspec);
	    if (nspec == 0)
		return 0;
	    if ((spec_arr = malloc(sizeof(spec_t) * nspec)) == NULL) {
		fprintf(stderr, "%s:  insufficient memory for specifications\n",
			fn);
		return -1;
	    }
	    memset(spec_arr, 0, sizeof(spec_t) * nspec);
	    rewind(fp);
	}
    }
    fclose(fp);

    /* Sort the specifications with most general first */
    qsort(spec_arr, nspec, sizeof(struct spec), spec_compare);

    /* Verify no exact duplicates */
    if (nodups_specs() != 0)
	return -1;
    return 0;
}

int main(int argc, char **argv)
{
	int opt, i;

	/* Process any options. */
	while ((opt = getopt(argc, argv, "dnqrsvW")) > 0) {
		switch (opt) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			change = 0;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			if (optind + 1 >= argc) {
				fprintf(stderr, "usage:  %s -r rootpath\n", 
					argv[0]);
				exit(1);
			}
			if (NULL != rootpath) {
				fprintf(stderr, 
					"%s: only one -r can be specified\n", 
					argv[0]);
				exit(1);
			}
			set_rootpath(argv[optind++]);
			break;
		case 's':
			use_stdin = 1;
			add_assoc = 0;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'W':
			warn_no_match = 1;
			break;
		case '?':
			usage(argv[0]);
		}
	}

	if (use_stdin) {
		if (optind != (argc - 1)) {
			/* Cannot mix with pathname arguments. */
			usage(argv[0]);
		}
	} else {
		if (optind > (argc - 2))
			usage(argv[0]);
	}

	/* Parse the specification file. */
	if (parseREContexts(argv[optind]) != 0) {
		perror(argv[optind]);
		exit(1);
	}
	optind++;

	/*
	 * Apply the specifications to the file systems.
	 */
	progname = argv[0];
	if(use_stdin)
	{
		char buf[PATH_MAX];
		while(fgets(buf, sizeof(buf), stdin))
		{
			struct stat sb;
			strtok(buf, "\n");
			if(buf[0] != '\n')
			{
				if(stat(buf, &sb))
					fprintf(stderr, "File \"%s\" not found.\n", buf);
				else
				{
					int flag;
					switch(sb.st_mode)
					{
					case S_IFDIR:
						flag = FTW_D;
					break;
					case S_IFLNK:
						flag = FTW_SL;
					break;
					default:
						flag = FTW_F;
					}
					apply_spec(buf, &sb, flag, NULL);
				}
			}
		}
	}
	else for (; optind < argc; optind++)
	{
		if (NULL != rootpath) {
			QPRINTF("%s:  labeling files, pretending %s is /\n",
				argv[0], rootpath);
		}

		QPRINTF("%s:  labeling files under %s\n", argv[0],
			argv[optind]);
		
		/* Walk the file tree, calling apply_spec on each file. */
		if (nftw
		    (argv[optind], apply_spec, 1024,
		     FTW_PHYS | FTW_MOUNT)) {
			fprintf(stderr,
				"%s:  error while labeling files under %s\n",
				argv[0], argv[optind]);
			exit(1);
		}

		/*
		 * Evaluate the association hash table distribution for the
		 * directory tree just traversed.
		 */
		file_spec_eval();

		/* Reset the association hash table for the next directory tree. */
		file_spec_destroy();
	}

	if (warn_no_match) {
		for (i = 0; i < nspec; i++) {
			if (spec_arr[i].matches == 0) {
				if (spec_arr[i].type_str) {
					printf
						("%s:  Warning!  No matches for (%s, %s, %s)\n",
						 argv[0], spec_arr[i].regex_str,
						 spec_arr[i].type_str, spec_arr[i].context);
				} else {
					printf
						("%s:  Warning!  No matches for (%s, %s)\n",
						 argv[0], spec_arr[i].regex_str,
						 spec_arr[i].context);
				}
			}
		}
	}

	QPRINTF("%s:  Done.\n", argv[0]);

	exit(0);
}
