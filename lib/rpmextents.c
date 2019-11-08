
#include "system.h"

#include <rpm/rpmlog.h>
#include <rpm/rpmio.h>
#include <string.h>
#include <errno.h>


#include "lib/rpmextents_internal.h"


int extentsVerifySigs(FD_t fd, int print_content){
    rpm_loff_t current;
    int32_t rc;
    size_t len;
    uint64_t content_len;
    char *content = NULL;
    struct extents_footer_t footer;

    current = Ftell(fd);

    if(extentsFooterFromFD(fd, &footer) != RPMRC_OK) {
	rc = -1;
	goto exit;
    }
    if(Fseek(fd, footer.offsets.checksig_offset, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR, _("extentsVerifySigs: Failed to seek signature verification offset\n"));
	rc = -1;
	goto exit;
    }
    len = sizeof(rc);
    if (Fread(&rc, len, 1, fd) != len) {
	rpmlog(RPMLOG_ERR, _("extentsVerifySigs: Failed to read Signature Verification RC\n"));
	rc = -1;
	goto exit;
    }

    if(print_content) {
	len = sizeof(content_len);
	if (Fread(&content_len, len, 1, fd) != len) {
	    rpmlog(RPMLOG_ERR, _("extentsVerifySigs: Failed to read signature content length\n"));
	    goto exit;
	}

	content = rmalloc(content_len + 1);
	if(content == NULL) {
	    rpmlog(RPMLOG_ERR, _("extentsVerifySigs: Failed to allocate memory to read signature content\n"));
	    goto exit;
	}
	content[content_len] = 0;
	if (Fread(content, content_len, 1, fd) != content_len) {
	    rpmlog(RPMLOG_ERR, _("extentsVerifySigs: Failed to read signature content\n"));
	    goto exit;
	}

	rpmlog(RPMLOG_NOTICE, "%s", content);
    }
exit:
    if(content){
	rfree(content);
    }
    if (Fseek(fd, current, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR, _("extentsVerifySigs: unable to seek back to original location\n"));
    }
    return rc;

}

rpmRC extentsFooterFromFD(FD_t fd, struct extents_footer_t *footer) {

    rpmRC rc = RPMRC_NOTFOUND;
    rpm_loff_t current;
    size_t len;

    // If the file is not seekable, we cannot detect whether or not it is transcoded.
    if(Fseek(fd, 0, SEEK_CUR) < 0) {
        return RPMRC_FAIL;
    }
    current = Ftell(fd);

    len = sizeof(struct extents_footer_t);
    if(Fseek(fd, -len, SEEK_END) < 0) {
	rc = RPMRC_FAIL;
	goto exit;
    }
    if (Fread(footer, len, 1, fd) != len) {
	rpmlog(RPMLOG_ERR, _("isTranscodedRpm: unable to read footer\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }
    if (footer->magic != EXTENTS_MAGIC) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }
    rc = RPMRC_OK;
exit:
    if (Fseek(fd, current, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR, _("isTranscodedRpm: unable to seek back to original location\n"));
	rc = RPMRC_FAIL;
    }
    return rc;
}

rpmRC isTranscodedRpm(FD_t fd) {
    struct extents_footer_t footer;
    return extentsFooterFromFD(fd, &footer);
}


