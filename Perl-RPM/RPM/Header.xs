#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Header.xs,v 1.17 2000/10/05 04:48:59 rjray Exp $";
static int scalar_tag(pTHX_ SV *, int);

/*
  Use this define for deriving the saved Header struct, rather than coding
  it a dozen places. Note that the hv_fetch call is the no-magic one defined
  in RPM.h
*/
#define header_from_object_ret(s_ptr, header, object, err_ret) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (header) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Header *)SvIV(*(s_ptr)) : NULL; \
    if (! (header)) \
        return (err_ret);
/* And a no-return version: */
#define header_from_object(s_ptr, header, object) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (header) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Header *)SvIV(*(s_ptr)) : NULL;


/* Some simple functions to manage key-to-SV* transactions, since these
   gets used frequently. */
const char* sv2key(pTHX_ SV* key)
{
    const char* new_key;

    /* De-reference key, if it is a reference */
    if (SvROK(key))
        key = SvRV(key);
    new_key = SvPV(key, PL_na);

    return new_key;
}

SV* key2sv(pTHX_ const char* key)
{
    return (sv_2mortal(newSVpv((char *)key, PL_na)));
}

static SV* ikey2sv(pTHX_ int key)
{
    return (sv_2mortal(newSViv(key)));
}

/* This creates a header data-field from the passed-in data */
static SV* rpmhdr_create(pTHX_ const char* data, int type, int size,
                         int scalar)
{
    char urk[2];
    AV* new_list;
    SV* new_item;
    int idx;

    new_list = newAV();

    /*
      Bad enough to have to duplicate the loop for all the case branches, I
      can at least bitch out on two of them:
    */
    if (type == RPM_NULL_TYPE)
    {
        return newSVsv(&PL_sv_undef);
    }
    else if (type == RPM_BIN_TYPE)
    {
        /* This differs from other types in that here size is the length of
           the binary chunk itself */
        av_store(new_list, 0, newSVpv((char *)data, size));
    }
    else
    {
        /* There will be at least this many items: */
        av_extend(new_list, size);

        switch (type)
        {
          case RPM_CHAR_TYPE:
            {
                char* loop;

                for (loop = (char *)data, idx = 0; idx < size; idx++, loop++)
                {
                    urk[0] = *data;
                    urk[1] = '\0';
                    new_item = newSVpv((char *)urk, 1);
                    av_store(new_list, idx, sv_2mortal(new_item));
                    SvREFCNT_inc(new_item);
                }

                break;
            }
          case RPM_INT8_TYPE:
            {
                I8* loop;

                for (loop = (I8 *)data, idx = 0; idx < size; idx++, loop++)
                {
                    /* Note that the rpm lib also uses masks for INT8 */
                    new_item = newSViv((I32)(*((I8 *)loop) & 0xff));
                    av_store(new_list, idx, sv_2mortal(new_item));
                    SvREFCNT_inc(new_item);
                }

                break;
            }
          case RPM_INT16_TYPE:
            {
                I16* loop;

                for (loop = (I16 *)data, idx = 0; idx < size; idx++, loop++)
                {
                    /* Note that the rpm lib also uses masks for INT16 */
                    new_item = newSViv((I32)(*((I16 *)loop) & 0xffff));
                    av_store(new_list, idx, sv_2mortal(new_item));
                    SvREFCNT_inc(new_item);
                }

                break;
            }
          case RPM_INT32_TYPE:
            {
                I32* loop;

                for (loop = (I32 *)data, idx = 0; idx < size; idx++, loop++)
                {
                    new_item = newSViv((I32)*((I32 *)loop));
                    av_store(new_list, idx, sv_2mortal(new_item));
                    SvREFCNT_inc(new_item);
                }

                break;
            }
          case RPM_STRING_TYPE:
          case RPM_I18NSTRING_TYPE:
          case RPM_STRING_ARRAY_TYPE:
            {
                char** loop;

                /* Special case for exactly one RPM_STRING_TYPE */
                if (type == RPM_STRING_TYPE && size == 1)
                {
                    new_item = newSVsv(&PL_sv_undef);
                    sv_setpvn(new_item, (char *)data, strlen((char *)data));
                    av_store(new_list, 0, sv_2mortal(new_item));
                    SvREFCNT_inc(new_item);
                }
                else
                {
                    for (loop = (char **)data, idx = 0;
                         idx < size;
                         idx++, loop++)
                    {
                        new_item = newSVsv(&PL_sv_undef);
                        sv_setpvn(new_item, *loop, strlen(*loop));
                        av_store(new_list, idx, sv_2mortal(new_item));
                        SvREFCNT_inc(new_item);
                    }
                }

                /* Only for STRING_ARRAY_TYPE do we have to call free() */
                if (type == RPM_STRING_ARRAY_TYPE) Safefree(data);
                break;
            }
          default:
            rpm_error(aTHX_ RPMERR_BADARG, "Unimplemented tag type");
            break;
        }
    }

    if (scalar)
        new_item = newSVsv(*(av_fetch(new_list, 0, FALSE)));
    else
        new_item = newRV((SV *)new_list);

    return new_item;
}

/* These three are for reading the header data from external sources */
static int new_from_fd_t(FD_t fd, RPM_Header* new_hdr)
{
    if (rpmReadPackageHeader(fd, &new_hdr->hdr, &new_hdr->isSource,
                             &new_hdr->major, &new_hdr->minor))
    {
        /* Some cases of this failing, rpmError was already called. But not
           all cases, unfortunately. So check the IV part of rpm_errSV */
        if (! SvIV(rpm_errSV))
            rpm_error(aTHX_ RPMERR_READERROR, "Error reading package header");
        return 0;
    }

    return 1;
}

static int new_from_fd(int fd, RPM_Header* new_hdr)
{
    FD_t FD = fdDup(fd);

    return(new_from_fd_t(FD, new_hdr));
}

static int new_from_fname(pTHX_ const char* source, RPM_Header* new_hdr)
{
    FD_t fd;
    int retval;

    if (! (fd = Fopen(source, "r")))
    {
        char errmsg[256];

        snprintf(errmsg, 256, "Unable to open file %s", source);
        rpm_error(aTHX_ RPMERR_BADFILENAME, errmsg);
        return 0;
    }

    if (retval = new_from_fd_t(fd, new_hdr))
    {
        Fclose(fd);
        new_hdr->source_name = safemalloc(strlen(source) + 1);
        strcpy(new_hdr->source_name, source);
    }

    return retval;
}

RPM__Header rpmhdr_TIEHASH(pTHX_ SV* class, SV* source, int flags)
{
    char* fname;
    int fname_len;
    SV* val;
    RPM__Header TIEHASH;
    RPM_Header* hdr_struct; /* Use this to store the actual C-level data */

    hdr_struct = safemalloc(sizeof(RPM_Header));
    Zero(hdr_struct, 1, RPM_Header);
    TIEHASH = Null(RPM__Header);

    if (! source)
        hdr_struct->hdr = headerNew();
    else if (! (flags & RPM_HEADER_FROM_REF))
    {
        /* If we aren't starting out with a pointer to a Header
           struct, figure out how to get there from here */

        /* If it is a string value, assume it to be a file name */
        if (SvPOK(source))
        {
            fname = SvPV(source, fname_len);
            if (! new_from_fname(aTHX_ fname, hdr_struct))
            {
                return TIEHASH;
            }
        }
        else if (IoIFP(sv_2io(source)))
        {
            if (! new_from_fd(PerlIO_fileno(IoIFP(sv_2io(source))),
                              hdr_struct))
            {
                return TIEHASH;
            }
        }
        else
        {
            rpm_error(aTHX_ RPMERR_BADARG,
                      "Argument 2 must be filename or GLOB");
            return TIEHASH;
        }
    }
    else
    {
        hdr_struct->hdr = (Header)SvRV(source);
        /* We simply don't know these three settings at this point */
        hdr_struct->isSource = 0;
        hdr_struct->major = 0;
        hdr_struct->minor = 0;
    }

    /* These three are likely to be most of the data requests, anyway */
    headerNVR(hdr_struct->hdr,
              &hdr_struct->name, &hdr_struct->version, &hdr_struct->release);
    /* This defaults to false, but RPM::Database will set it true */
    hdr_struct->read_only = flags & RPM_HEADER_READONLY;
    
    hdr_struct->iterator = (HeaderIterator)NULL;

    new_RPM__Header(TIEHASH);
    /* With the actual HV*, store the type-keys for the three cached values: */
    hv_store_nomg(TIEHASH, "NAME_t", 7, newSViv(RPM_STRING_TYPE), FALSE);
    hv_store_nomg(TIEHASH, "VERSION_t", 10, newSViv(RPM_STRING_TYPE), FALSE);
    hv_store_nomg(TIEHASH, "RELEASE_t", 10, newSViv(RPM_STRING_TYPE), FALSE);
    hv_store_nomg(TIEHASH,
                  STRUCT_KEY, STRUCT_KEY_LEN,
                  newSViv((unsigned)hdr_struct), FALSE);
    return TIEHASH;
}

SV* rpmhdr_FETCH(pTHX_ RPM__Header self, SV* key,
                 const char* data_in, int type_in, int size_in)
{
    const char* name;  /* For the actual name out of (SV *)key */
    int namelen;       /* Arg for SvPV(..., len)               */
    char* uc_name;     /* UC'd version of name                 */
    RPM_Header* hdr;   /* Pointer to C-level struct            */
    SV** svp;
    SV* FETCH;
    int i, tag_by_num;
    char errmsg[256];

    FETCH = newSVsv(&PL_sv_undef);

    header_from_object_ret(svp, hdr, self, FETCH);

    name = sv2key(aTHX_ key);
    if (! (name && (namelen = strlen(name))))
        return FETCH;

    uc_name = safemalloc(namelen + 3);
    for (i = 0; i < namelen; i++)
        uc_name[i] = toUPPER(name[i]);
    uc_name[i] = '\0';

    /* Get the #define value for the tag from the hash made at boot */
    if (! (tag_by_num = tag2num(aTHX_ uc_name)))
    {
        snprintf(errmsg, 256, "RPM::Header::FETCH: unknown tag '%s'", uc_name);
        rpm_error(aTHX_ RPMERR_BADARG, errmsg);
        Safefree(uc_name);
        return FETCH;
    }

    /* Check the three keys that are cached directly on the struct itself: */
    if (! strcmp(uc_name, "NAME"))
        FETCH = newSVpv((char *)hdr->name, 0);
    else if (! strcmp(uc_name, "VERSION"))
        FETCH = newSVpv((char *)hdr->version, 0);
    else if (! strcmp(uc_name, "RELEASE"))
        FETCH = newSVpv((char *)hdr->release, 0);
    else
    {
        /* If it wasn't one of those three, then we have to explicitly fetch
           it, either from the store in cache or via the headerGetEntry call */
        hv_fetch_nomg(svp, self, uc_name, namelen, FALSE);
        if (svp && SvOK(*svp))
        {
            FETCH = newSVsv(*svp);
            if (SvROK(FETCH))
                FETCH = SvRV(FETCH);
        }
        else if (data_in)
        {
            /* In some cases (particarly the iterators) we could be called
               with the data already available, but not hashed just yet. */
            SV* new_item = rpmhdr_create(aTHX_ data_in, type_in, size_in,
                                         scalar_tag(aTHX_ Nullsv, tag_by_num));

            hv_store_nomg(self, uc_name, namelen, newRV((SV *)new_item),
                          FALSE);
            hv_store_nomg(self, strcat(uc_name, "_t"), (namelen + 2),
                          newSViv(type_in), FALSE);

            FETCH = new_item;
        }
        else
        {
            SV* new_item;
            char* new_item_p;
            int new_item_type;
            int size;
            char urk[2];

            /* Pull the tag by the int value we now have */
            if (! headerGetEntry(hdr->hdr, tag_by_num,
                                 &new_item_type, (void **)&new_item_p, &size))
            {
                snprintf(errmsg, 256,
                         "RPM::Header::FETCH: no tag '%s' in header", uc_name);
                rpm_error(aTHX_ RPMERR_BADARG, errmsg);
                Safefree(uc_name);
                return FETCH;
            }
            new_item = rpmhdr_create(aTHX_ new_item_p, new_item_type, size,
                                     scalar_tag(aTHX_ Nullsv, tag_by_num));

            hv_store_nomg(self, uc_name, namelen, newRV((SV *)new_item),
                          FALSE);
            hv_store_nomg(self, strcat(uc_name, "_t"), (namelen + 2),
                          newSViv(new_item_type), FALSE);
            FETCH = new_item;
        }
    }

    Safefree(uc_name);
    return FETCH;
}

/*
  Store the data in "value" both in the header and in the hash associated
  with "self".
*/
int rpmhdr_STORE(pTHX_ RPM__Header self, SV* key, SV* value)
{
    SV** svp;
    const char* name;
    char* uc_name;
    char errmsg[256];
    STRLEN namelen;
    int size, i, is_scalar;
    I32 num_ent, data_type, data_key;
    void* data;
    AV* a_value = Nullav;
    RPM_Header* hdr;

    header_from_object_ret(svp, hdr, self, 0);
    if (hdr->read_only)
        return 0;

    name = sv2key(aTHX_ key);
    if (! (name && (namelen = strlen(name))))
        return 0;

    uc_name = safemalloc(namelen + 3);
    for (i = 0; i < namelen; i++)
        uc_name[i] = toUPPER(name[i]);
    uc_name[i] = '\0';

    /* Get the numerical tag value for this name. If none exists, this means
       that there is no such tag, which is an error in this case */
    if (! (num_ent = tag2num(aTHX_ uc_name)))
    {
        snprintf(errmsg, 256, "RPM::Header::STORE: No such tag '%s'", uc_name);
        rpm_error(aTHX_ RPMERR_BADARG, errmsg);
        return 0;
    }
    is_scalar = scalar_tag(Nullsv, num_ent);
    if (SvROK(value))
    {
        /*
          This is complicated. We have to allow for straight-in AV*, or a
          single-pair HV* that provides the type indexing the data. Then
          we get to decide if the data part needs to be promoted to an AV*.
        */
        if (SvTYPE(SvRV(value)) == SVt_PVHV)
        {
            HE* iter;
            SV* key;
            SV* new_value;
            HV* hv_value = (HV *)SvRV(value);

            /* There should be exactly one key */
            if (hv_iterinit(hv_value) != 1)
            {
                snprintf(errmsg, 256,
                         "RPM::Header::STORE: Hash reference passed in for "
                         "tag '%s' has invalid content", uc_name);
                rpm_error(aTHX_ RPMERR_BADARG, errmsg);
                return 0;
            }
            iter = hv_iternext(hv_value);
            key = HeSVKEY(iter);
            new_value = HeVAL(iter);
            if (! (SvIOK(key) && (data_type = SvIV(key))))
            {
                snprintf(errmsg, 256,
                         "RPM::Header::STORE: Hash reference key passed in "
                         "for tag '%s' is invalid", uc_name);
                rpm_error(aTHX_ RPMERR_BADARG, errmsg);
                return 0;
            }
            /* Clear this for later sanity-check */
            value = Nullsv;
            /* Now let's look at new_value */
            if (SvROK(new_value))
            {
                if (SvTYPE(SvRV(new_value)) == SVt_PVAV)
                    a_value = (AV *)SvRV(new_value);
                else
                    /* Hope for the best... */
                    value = SvRV(new_value);
            }
            else
                value = new_value;
        }
        else if (SvTYPE(SvRV(value)) == SVt_PVAV)
        {
            /*
              If they passed a straight-through AV*, de-ref it and mark type
              to be filled in later
            */
            a_value = (AV *)SvRV(value);
            data_type = -1;
            value = Nullsv;
        }
        else
        {
            /* De-reference it and hope it passes muster as a scalar */
            value = SvRV(value);
        }
    }

    /* The only way value will still be set is if nothing else matched */
    if (value != Nullsv)
    {
        /*
          The case-block below is already set up to handle data in a manner
          transparent to the quantity or type. We can fake this with a_value
          and not worry again until actually storing on the hash table for
          self.
        */
        a_value = newAV();
        av_store(a_value, 0, value);
        /* Mark type for later setting */
        data_type = -1;
    }
    size = av_len(a_value) + 1;

    /*
      Setting/STORE-ing means do the following:

      1. Confirm that data adheres to type (mostly check against int types)
      2. Create the blob in **data (based on is_scalar)
      3. Store to the header struct
      4. Store the SV* on the hash
    */

    if (data_type == -1)
    {
        /* This will permanently concat "_t" to uc_name. But we'll craftily
           manipulate that later on with namelen. */
        hv_fetch_nomg(svp, self, strcat(uc_name, "_t"), (namelen + 2), FALSE);
        if (! (svp && SvOK(*svp)))
        {
            /*
              If NAME_t does not exist, then this has not been fetched
              previously, and worse, we don't really know what the type is
              supposed to be. So we state in the docs that the default is
              RPM_STRING_TYPE.
            */
            data_type = RPM_STRING_TYPE;
        }
        else
        {
            data_type = SvIV(*svp);
            SvREFCNT_dec(*svp);
        }
    }

    if (data_type == RPM_INT8_TYPE ||
        data_type == RPM_INT16_TYPE ||
        data_type == RPM_INT32_TYPE)
    {
        /* Cycle over the array and verify that all elements are valid IVs */
        for (i = 0; i < size; i++)
        {
            svp = av_fetch(a_value, i, FALSE);
            if (! (SvOK(*svp) && SvIOK(*svp)))
            {
                rpm_error(aTHX_ RPMERR_BADARG,
                          "RPM::Header::STORE: Non-integer value passed for "
                          "integer-type tag");
                return 0;
            }
        }
    }

    /*
      This is more like the rpmhdr_create case block, where we have to
      discern based on data-type, so that the pointers are properly
      allocated and assigned.
    */
    switch (data_type)
    {
      case RPM_NULL_TYPE:
        size = 1;
        data = NULL;
        break;
      case RPM_BIN_TYPE:
        {
            char* data_p;

            svp = av_fetch(a_value, 0, FALSE);
            if (svp && SvPOK(*svp))
                data_p = SvPV(*svp, size);
            else
            {
                size = 0;
                data_p = Nullch;
            }

            data = (void *)data_p;
            break;
        }
      case RPM_CHAR_TYPE:
        {
            char* data_p;
            char* str_sv;
            STRLEN len;

            Newz(TRUE, data_p, size, char);
            for (i = 0; i < size; i++)
            {
                /* Having stored the chars in separate SVs wasn't the most
                   efficient way, but it made the rest of things a lot
                   cleaner. To be safe, only take the initial character from
                   each SV. */
                svp = av_fetch(a_value, i, FALSE);
                if (svp && SvPOK(*svp))
                {
                    str_sv = SvPV(*svp, len);
                    data_p[i] = str_sv[0];
                }
                else
                    data_p[i] = '\0';
            }

            data = (void *)data_p;
            break;
        }
      case RPM_INT8_TYPE:
        {
            I8** data_p;

            Newz(TRUE, data_p, size, I8*);

            for (i = 0; i < size; i++)
            {
                svp = av_fetch(a_value, i, FALSE);
                if (svp && SvIOK(*svp))
                    *(data_p[i]) = (I8)SvIV(*svp);
                else
                    *(data_p[i]) = (I8)0;
            }

            data = (void *)data_p;
            break;
        }
      case RPM_INT16_TYPE:
        {
            I16** data_p;

            Newz(TRUE, data_p, size, I16*);

            for (i = 0; i < size; i++)
            {
                svp = av_fetch(a_value, i, FALSE);
                if (svp && SvIOK(*svp))
                    *(data_p[i]) = (I16)SvIV(*svp);
                else
                    *(data_p[i]) = (I16)0;
            }

            data = (void *)data_p;
            break;
        }
      case RPM_INT32_TYPE:
        {
            I32** data_p;

            Newz(TRUE, data_p, size, I32*);

            for (i = 0; i < size; i++)
            {
                svp = av_fetch(a_value, i, FALSE);
                if (svp && SvIOK(*svp))
                    *(data_p[i]) = SvIV(*svp);
                else
                    *(data_p[i]) = 0;
            }

            data = (void *)data_p;
            break;
        }
      case RPM_STRING_TYPE:
      case RPM_I18NSTRING_TYPE:
      case RPM_STRING_ARRAY_TYPE:
        {
            char** data_p;
            char* str_sv;
            char* str_new;
            SV* cloned;
            STRLEN len;

            if (data_type == RPM_STRING_TYPE && size == 1)
            {
                /* Special case for exactly one RPM_STRING_TYPE */
                svp = av_fetch(a_value, 0, FALSE);
                if (svp)
                {
                    if (SvPOK(*svp))
                        cloned = *svp;
                    else
                        cloned = sv_mortalcopy(*svp);
                    str_sv = SvPV(cloned, len);
                    str_new = safemalloc(len + 1);
                    strncpy(str_new, str_sv, len + 1);
                }
                else
                    str_new = Nullch;

                data = (void **)str_new;
            }
            else
            {
                Newz(TRUE, data_p, size, char*);

                for (i = 0; i < size; i++)
                {
                    svp = av_fetch(a_value, i, FALSE);
                    if (svp)
                    {
                        if (SvPOK(*svp))
                            cloned = *svp;
                        else
                            cloned = sv_mortalcopy(*svp);
                        str_sv = SvPV(*svp, len);
                        str_new = safemalloc(len + 1);
                        strncpy(str_new, str_sv, len + 1);
                        data_p[i] = str_new;
                    }
                    else
                        data_p[i] = Nullch;
                }

                data = (void *)data_p;
            }
            break;
        }
      default:
        rpm_error(aTHX_ RPMERR_BADARG, "Unimplemented tag type");
        break;
    }
    /* That was fun. I always enjoy delving into the black magic of void *. */

    /* Remove any pre-existing tag */
    headerRemoveEntry(hdr->hdr, num_ent); /* Don't care if it fails? */
    /* Store the new data */
    headerAddEntry(hdr->hdr, num_ent, data_type, data, size);
    /* Store on the hash */
    hv_store_nomg(self, uc_name, namelen,
                  (is_scalar) ? newSVsv(value) : newRV_noinc((SV *)a_value),
                  FALSE);

    return 1;
}

int rpmhdr_DELETE(pTHX_ RPM__Header self, SV* key)
{
    const char* name;  /* For the actual name out of (SV *)key */
    int namelen;       /* Arg for SvPV(..., len)               */
    char* uc_name;     /* UC'd version of name                 */
    RPM_Header* hdr;   /* Pointer to C-level struct            */
    SV** svp;
    int retval, num, i;

    header_from_object_ret(svp, hdr, self, 0);
    if (hdr->read_only)
        return 0;

    name = sv2key(aTHX_ key);
    if (! (name && (namelen = strlen(name))))
        return 0;

    uc_name = safemalloc(namelen + 3);
    for (i = 0; i < namelen; i++)
        uc_name[i] = toUPPER(name[i]);
    uc_name[i] = '\0';

    /* Get the numerical tag value for this name. If none exists, this means
       that there is no such tag, which isn't really an error (so return 1). */
    if (! (num = tag2num(aTHX_ uc_name)))
    {
        retval = 1;
    }
    /*
      Deletion means three steps:

      1. rpmlib-level deletion
      2. Delete the key from self
      3. Delete the KEY_t from self
    */
    
    /* First off, if there were no entries of this tag, no need to do 2 or 3 */
    else if (headerRemoveEntry(hdr->hdr, num))
    {
        retval = 1;
    }
    else
    {
        /* Remove magic long enough to do two hv_delete() calls */
        SvMAGICAL_off((SV *)self);
        hv_delete(self, uc_name, namelen, G_DISCARD);
        hv_delete(self, strcat(uc_name, "_t"), namelen + 2, G_DISCARD);
        SvMAGICAL_on((SV *)self);
        retval = 1;
    }

    Safefree(uc_name);
    return retval;
}

int rpmhdr_EXISTS(pTHX_ RPM__Header self, SV* key)
{
    const char* name;
    char* uc_name;
    int namelen, tag_by_num, i;
    SV** svp;
    RPM_Header* hdr;

    header_from_object_ret(svp, hdr, self, 0);
    name = sv2key(aTHX_ key);
    if (! (name && (namelen = strlen(name))))
        return 0;

    /* Unlike FETCH, there will be no need for the KEY_t string */
    uc_name = safemalloc(namelen + 1);
    for (i = 0; i < namelen; i++)
        uc_name[i] = toUPPER(name[i]);
    uc_name[i] = '\0';

    /* Get the #define value for the tag from the hash made at boot-up */
    tag_by_num = tag2num(aTHX_ uc_name);
    Safefree(uc_name);
    if (! tag_by_num)
        /* Later we need to set some sort of error message */
        return 0;

    return (headerIsEntry(hdr->hdr, tag_by_num));
}

int rpmhdr_FIRSTKEY(pTHX_ RPM__Header self, SV** key, SV** value)
{
    SV** svp;
    RPM_Header* hdr;
    int tag, type, size;
    char* ptr;
    const char* tagname;

    header_from_object_ret(svp, hdr, self, 0);
    /* If there is an existing iterator attached to the struct, free it */
    if (hdr->iterator)
        headerFreeIterator(hdr->iterator);

    /* The init function returns the iterator that is used in later calls */
    if (! (hdr->iterator = headerInitIterator(hdr->hdr)))
        /* need some error message? */
        return 0;

    /* Run once to get started */
    headerNextIterator(hdr->iterator,
                       Null(int *), Null(int *), Null(void **), Null(int *));
    /* Now run it once, to get the first header entry */
    if (! headerNextIterator(hdr->iterator, &tag, &type, (void **)&ptr, &size))
        return 0;

    tagname = num2tag(aTHX_ tag);
    *key = newSVpv((char *)tagname, strlen(tagname));
    *value = rpmhdr_FETCH(aTHX_ self, *key, ptr, type, size);

    return 1;
}

int rpmhdr_NEXTKEY(pTHX_ RPM__Header self, SV* key,
                   SV** nextkey, SV** nextvalue)
{
    SV** svp;
    RPM_Header* hdr;
    int tag, type, size;
    char* ptr;
    const char* tagname;

    header_from_object_ret(svp, hdr, self, 0);
    /* If there is not an existing iterator, we can't continue */
    if (! hdr->iterator)
        return 0;

    /* Iterate here, since there are internal tags that may be present for
       which we don't want to expose to the user. */
    while (1)
    {
        /* Run it once, to get the next header entry */
        if (! headerNextIterator(hdr->iterator, &tag, &type, (void **)&ptr,
                                 &size))
            /* Last tag. Inform perl that iteration is over. */
            return 0;

        tagname = num2tag(aTHX_ tag);
        /* This means that any time num2tag couldn't map it, we iterate */
        if (tagname != Nullch)
            break;
    }

    *nextkey = newSVpv((char *)tagname, strlen(tagname));
    *nextvalue = rpmhdr_FETCH(aTHX_ self, *nextkey, ptr, type, size);

    return 1;
}

void rpmhdr_DESTROY(pTHX_ RPM__Header self)
{
    SV** svp;
    RPM_Header* hdr;

    header_from_object(svp, hdr, self);
    if (! hdr) return;

    if (hdr->iterator)
        headerFreeIterator(hdr->iterator);
    if (hdr->hdr)
        headerFree(hdr->hdr);

    hv_undef(self);
}

unsigned int rpmhdr_size(pTHX_ RPM__Header self)
{
    SV** svp;
    RPM_Header* hdr;

    header_from_object_ret(svp, hdr, self, 0);

    if (! hdr->hdr)
        return 0;
    else
        return(headerSizeof(hdr->hdr, HEADER_MAGIC_YES));
}

int rpmhdr_tagtype(pTHX_ RPM__Header self, SV* key)
{
    STRLEN namelen;
    const char* name;
    char* uc_name;
    SV** svp;
    int i, retval;

    name = sv2key(aTHX_ key);
    if (! (name && (namelen = strlen(name))))
        return RPM_NULL_TYPE;

    uc_name = safemalloc(namelen + 3);
    for (i = 0; i < namelen; i++)
        uc_name[i] = toUPPER(name[i]);
    uc_name[i] = '\0';
    strcat(uc_name, "_t");

    retval = RPM_NULL_TYPE;

    hv_fetch_nomg(svp, self, uc_name, strlen(uc_name) + 1, FALSE);
    if (svp && SvOK(*svp))
    {
        /* The base tag has already been fetched and thus we have a type */
        retval =  SvIV(*svp);
    }
    else
    {
        /* We haven't had to fetch the tag itself yet. Until then, the special
           key that holds the type isn't available, either. */

        /* Do a plain fetch (that is, leave magic on) to populate the other */
        SV* sub_fetch = rpmhdr_FETCH(aTHX_ self, key, Nullch, 0, 0);

        if (sub_fetch)
        {
            hv_fetch_nomg(svp, self, uc_name, strlen(uc_name), FALSE);
            if (svp && SvOK(*svp))
            {
                /* The base tag has now been fetched */
                retval =  SvIV(*svp);
            }
        }
    }

    Safefree(uc_name);
    return retval;
}

int rpmhdr_write(pTHX_ RPM__Header self, SV* gv_in, int magicp)
{
    IO* io;
    PerlIO* fp;
    FD_t fd;
    RPM_Header* hdr;
    GV* gv;
    SV** svp;
    int written = 0;

    gv = (SvPOK(gv_in) && (SvTYPE(gv_in) == SVt_PVGV)) ?
        (GV *)SvRV(gv_in) : (GV *)gv_in;
    header_from_object_ret(svp, hdr, self, 0);

    if (!gv || !(io = GvIO(gv)) || !(fp = IoIFP(io)))
        return written;

    fd = fdDup(PerlIO_fileno(fp));
    headerWrite(fd, hdr->hdr, magicp);
    Fclose(fd);
    written = headerSizeof(hdr->hdr, magicp);

    return written;
}

/* T/F test whether the header references a SRPM */
int rpmhdr_is_source(pTHX_ RPM__Header self)
{
    SV** svp;
    RPM_Header* hdr;

    header_from_object_ret(svp, hdr, self, 0);

    if (! hdr->hdr)
        return 0;
    else
        return (hdr->isSource);
}

/*
  A classic-style comparison function for two headers, returns -1 if a < b,
  1 if a > b, and 0 if a == b. In terms of version/release, that is.
*/
int rpmhdr_cmpver(pTHX_ RPM__Header self, RPM__Header other)
{
    RPM_Header* one;
    RPM_Header* two;
    SV** svp;

    header_from_object(svp, one, self);
    if (! one)
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Header::rpmhdr_cmpver: Arg 1 has no header data");
        return 0;
    }
    header_from_object(svp, two, other);
    if (! two)
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Header::rpmhdr_cmpver: Arg 2 has no header data");
        return 0;
    }

    return rpmVersionCompare(one->hdr, two->hdr);
}

/*
  If the header object was read from a specific source (file, ftp, http), then
  a copy of that location was kept for future reference. Now reference it.
*/
char* rpmhdr_source_name(RPM__Header self)
{
    SV** svp;
    RPM_Header* hdr;

    header_from_object(svp, hdr, self);

    return hdr->source_name;
}

/*
  A matter-of-convenience function that tells whether the passed-in tag is
  one that returns a scalar (yields a true return value) or one that returns
  an array reference (yields a false value).
*/
static int scalar_tag(pTHX_ SV* self, int tag_value)
{
    /* self is passed in as SV*, and unused, because this is a class method */
    switch (tag_value)
    {
      case RPMTAG_ARCH:
      case RPMTAG_ARCHIVESIZE:
      case RPMTAG_BUILDHOST:
      case RPMTAG_BUILDROOT:
      case RPMTAG_BUILDTIME:
      case RPMTAG_COOKIE:
      case RPMTAG_DESCRIPTION:
      case RPMTAG_DISTRIBUTION:
      case RPMTAG_EPOCH:
      case RPMTAG_EXCLUDEARCH:
      case RPMTAG_EXCLUDEOS:
      case RPMTAG_EXCLUSIVEARCH:
      case RPMTAG_EXCLUSIVEOS:
      case RPMTAG_GIF:
      case RPMTAG_GROUP:
      case RPMTAG_ICON:
      case RPMTAG_INSTALLTIME:
      case RPMTAG_LICENSE:
      case RPMTAG_NAME:
      case RPMTAG_OS:
      case RPMTAG_PACKAGER:
      case RPMTAG_RELEASE:
      case RPMTAG_RPMVERSION:
      case RPMTAG_SIZE:
      case RPMTAG_SOURCERPM:
      case RPMTAG_SUMMARY:
      case RPMTAG_URL:
      case RPMTAG_VENDOR:
      case RPMTAG_VERSION:
      case RPMTAG_XPM:
        return 1;
        /* not reached */
        break;
      default:
        return 0;
        /* not reached */
        break;
    }
    /* not reached */
}


MODULE = RPM::Header    PACKAGE = RPM::Header           PREFIX = rpmhdr_


RPM::Header
rpmhdr_TIEHASH(class, source=NULL, flags=0)
    SV* class;
    SV* source;
    int flags;
    PROTOTYPE: $;$$
    CODE:
    RETVAL = rpmhdr_TIEHASH(aTHX_ class, source, flags);
    OUTPUT:
    RETVAL

SV*
rpmhdr_FETCH(self, key)
    RPM::Header self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmhdr_FETCH(aTHX_ self, key, Nullch, 0, 0);
    OUTPUT:
    RETVAL

int
rpmhdr_STORE(self, key, value)
    RPM::Header self;
    SV* key;
    SV* value;
    PROTOTYPE: $$$
    PREINIT:
    AV* avalue;
    CODE:
    RETVAL = rpmhdr_STORE(aTHX_ self, key, value);
    OUTPUT:
    RETVAL

int
rpmhdr_DELETE(self, key)
    RPM::Header self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmhdr_DELETE(aTHX_ self, key);
    OUTPUT:
    RETVAL

int
rpmhdr_CLEAR(self)
    RPM::Header self;
    PROTOTYPE: $
    CODE:
    {
        rpm_error(aTHX_ RPMERR_NOCREATEDB, "CLEAR: operation not permitted");
        RETVAL = 0;
    }
    OUTPUT:
    RETVAL

int
rpmhdr_EXISTS(self, key)
    RPM::Header self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmhdr_EXISTS(aTHX_ self, key);
    OUTPUT:
    RETVAL

void
rpmhdr_FIRSTKEY(self)
    RPM::Header self;
    PROTOTYPE: $
    PREINIT:
    SV* key;
    SV* value;
    int i;
    PPCODE:
    {
        if (! rpmhdr_FIRSTKEY(aTHX_ self, &key, &value))
        {
            key = newSVsv(&PL_sv_undef);
            value = newSVsv(&PL_sv_undef);
        }

        XPUSHs(sv_2mortal(newSVsv(value)));
        XPUSHs(sv_2mortal(newSVsv(key)));
    }

void
rpmhdr_NEXTKEY(self, key=NULL)
    RPM::Header self;
    SV* key;
    PROTOTYPE: $;$
    PREINIT:
    SV* nextkey;
    SV* nextvalue;
    int i;
    PPCODE:
    {
        if (! rpmhdr_NEXTKEY(aTHX_ self, key, &nextkey, &nextvalue))
        {
            nextkey = newSVsv(&PL_sv_undef);
            nextvalue = newSVsv(&PL_sv_undef);
        }

        XPUSHs(sv_2mortal(newSVsv(nextvalue)));
        XPUSHs(sv_2mortal(newSVsv(nextkey)));
    }

void
rpmhdr_DESTROY(self)
    RPM::Header self;
    PROTOTYPE: $
    CODE:
    rpmhdr_DESTROY(aTHX_ self);

unsigned int
rpmhdr_size(self)
    RPM::Header self;
    PROTOTYPE: $
    CODE:
    RETVAL = rpmhdr_size(aTHX_ self);
    OUTPUT:
    RETVAL

int
rpmhdr_tagtype(self, key)
    RPM::Header self;
    SV* key;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmhdr_tagtype(aTHX_ self, key);
    OUTPUT:
    RETVAL

int
rpmhdr_write(self, gv, magicp=0)
    RPM::Header self;
    SV* gv;
    SV* magicp;
    PROTOTYPE: $$;$
    CODE:
    {
        int flag;

        if (magicp && SvIOK(magicp))
            flag = SvIV(magicp);
        else
            flag = HEADER_MAGIC_YES;

        RETVAL = rpmhdr_write(aTHX_ self, gv, flag);
    }
    OUTPUT:
    RETVAL

int
rpmhdr_is_source(self)
    RPM::Header self;
    PROTOTYPE: $
    CODE:
    RETVAL = rpmhdr_is_source(aTHX_ self);
    OUTPUT:
    RETVAL

int
rpmhdr_cmpver(self, other)
    RPM::Header self;
    RPM::Header other;
    PROTOTYPE: $$
    CODE:
    RETVAL = rpmhdr_cmpver(aTHX_ self, other);
    OUTPUT:
    RETVAL

void
rpmhdr_NVR(self)
    RPM::Header self;
    PROTOTYPE: $
    PPCODE:
    {
        SV** svp;
        RPM_Header* hdr;

        header_from_object(svp, hdr, self);

        if (hdr->name)
        {
            XPUSHs(sv_2mortal(newSVpv((char *)hdr->name, 0)));
            XPUSHs(sv_2mortal(newSVpv((char *)hdr->version, 0)));
            XPUSHs(sv_2mortal(newSVpv((char *)hdr->release, 0)));
        }
    }

int
rpmhdr_scalar_tag(self, tag)
    SV* self;
    SV* tag;
    PROTOTYPE: $$
    CODE:
    int tag_value;

    if (SvPOK(tag))
    {
        const char* name;
        int i, namelen;
        char* uc_name;

        /*
          A matter of explanation:

          By doing this bit here, I can let the real scalar_tag take an
          integer argument directly. That means I can call it all over the
          place in here, without the overhead of the SV-to-int conversion
          below. Only use from outside of the XS code pays that penalty.
        */
        name = sv2key(aTHX_ tag);
        if (! (name && (namelen = strlen(name))))
            RETVAL = 0;
        else
        {
            uc_name = safemalloc(namelen + 1);
            for (i = 0; i < namelen; i++)
                uc_name[i] = toUPPER(name[i]);
            uc_name[i] = '\0';
            if (! (tag_value = tag2num(aTHX_ uc_name)))
            {
                char errmsg[256];

                snprintf(errmsg, 256,
                         "RPM::Header::scalar_tag: unknown tag %s", uc_name);
                rpm_error(aTHX_ RPMERR_BADARG, errmsg);
                Safefree(uc_name);
                RETVAL = 0;
            }

            RETVAL = scalar_tag(aTHX_ self, tag_value);
        }
    }
    else if (SvIOK(tag))
    {
        tag_value = SvIV(tag);
        RETVAL = scalar_tag(aTHX_ self, tag_value);
    }
    else
    {
        rpm_error(aTHX_ RPMERR_BADARG,
                  "RPM::Header::scalar_tag: argument must be string or int");
        RETVAL = 0;
    }
    OUTPUT:
    RETVAL

char*
rpmhdr_source_name(self)
    RPM::Header self;
    PROTOTYPE: $
    CODE:
    RETVAL = rpmhdr_source_name(self);
    OUTPUT:
    RETVAL
