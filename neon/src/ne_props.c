/* 
   WebDAV Properties manipulation
   Copyright (C) 2000-2001, Joe Orton <joe@light.plus.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ne_alloc.h"
#include "ne_xml.h"
#include "ne_props.h"
#include "ne_basic.h"

struct ne_propfind_handler_s {
    ne_session *sess;
    ne_request *request;

    int has_props; /* whether we've already written some
		    * props to the body. */
    ne_buffer *body;
    
    ne_207_parser *parser207;
    ne_xml_parser *parser;

    /* Callback to create the private structure. */
    ne_props_create_complex private_creator;
    void *private_userdata;
    
    /* Current propset. */
    ne_prop_result_set *current;

    ne_props_result callback;
    void *userdata;
};

#define ELM_namedprop (NE_ELM_207_UNUSED)

static const struct ne_xml_elm flat_elms[] = {
    { "", "", NE_ELM_unknown, NE_XML_COLLECT },
    { NULL }
};

/* We build up the results of one 'response' element in memory. */
struct prop {
    char *name, *nspace, *value, *lang;
    /* Store a ne_propname here too, for convienience.  pname.name =
     * name, pname.nspace = nspace, but they are const'ed in pname. */
    ne_propname pname;
};

struct propstat {
    struct prop *props;
    int numprops;
    ne_status status;
} propstat;

/* Results set. */
struct ne_prop_result_set_s {
    struct propstat *pstats;
    int numpstats;
    void *private;
    char *href;
};


static int 
startelm(void *userdata, const struct ne_xml_elm *elm, 
	 const char **atts);
static int 
endelm(void *userdata, const struct ne_xml_elm *elm, const char *cdata);
static int check_context(ne_xml_elmid parent, ne_xml_elmid child);

ne_xml_parser *ne_propfind_get_parser(ne_propfind_handler *handler)
{
    return handler->parser;
}

ne_request *ne_propfind_get_request(ne_propfind_handler *handler)
{
    return handler->request;
}

static int propfind(ne_propfind_handler *handler, 
		    ne_props_result results, void *userdata)
{
    int ret;
    ne_request *req = handler->request;

    /* Register the flat property handler to catch any properties 
     * which the user isn't handling as 'complex'. */
    ne_xml_push_handler(handler->parser, flat_elms, 
			 check_context, startelm, endelm, handler);

    /* Register the catch-all handler to ignore any cruft the
     * server returns. */
    ne_207_ignore_unknown(handler->parser207);
    
    handler->callback = results;
    handler->userdata = userdata;

    ne_set_request_body_buffer(req, handler->body->data,
			       ne_buffer_size(handler->body));

    ne_add_request_header(req, "Content-Type", "text/xml"); /* TODO: UTF-8? */
    
    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, 
				  handler->parser);

    ret = ne_request_dispatch(req);

    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    } else if (!ne_xml_valid(handler->parser)) {
	ne_set_error(handler->sess, ne_xml_get_error(handler->parser));
	ret = NE_ERROR;
    }

    return ret;
}

static void set_body(ne_propfind_handler *hdl, const ne_propname *names)
{
    ne_buffer *body = hdl->body;
    int n;
    
    if (!hdl->has_props) {
	ne_buffer_zappend(body, "<prop>" EOL);
	hdl->has_props = 1;
    }

    for (n = 0; names[n].name != NULL; n++) {
	ne_buffer_concat(body, "<", names[n].name, " xmlns=\"", 
		       names[n].nspace, "\"/>" EOL, NULL);
    }

}

int ne_propfind_allprop(ne_propfind_handler *handler, 
			 ne_props_result results, void *userdata)
{
    ne_buffer_zappend(handler->body, "<allprop/></propfind>" EOL);
    return propfind(handler, results, userdata);
}

int ne_propfind_named(ne_propfind_handler *handler, const ne_propname *props,
		       ne_props_result results, void *userdata)
{
    set_body(handler, props);
    ne_buffer_zappend(handler->body, "</prop></propfind>" EOL);
    return propfind(handler, results, userdata);
}


/* The easy one... PROPPATCH */
int ne_proppatch(ne_session *sess, const char *uri, 
		  const ne_proppatch_operation *items)
{
    ne_request *req = ne_request_create(sess, "PROPPATCH", uri);
    ne_buffer *body = ne_buffer_create();
    int n, ret;
    
    /* Create the request body */
    ne_buffer_zappend(body, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" EOL
		     "<propertyupdate xmlns=\"DAV:\">");

    for (n = 0; items[n].name != NULL; n++) {
	switch (items[n].type) {
	case ne_propset:
	    /* <set><prop><prop-name>value</prop-name></prop></set> */
	    ne_buffer_concat(body, "<set><prop>"
			   "<", items[n].name->name, " xmlns=\"",
			   items[n].name->nspace, "\">", items[n].value,
			   "</", items[n].name->name, "></prop></set>" EOL, 
			   NULL);
	    break;

	case ne_propremove:
	    /* <remove><prop><prop-name/></prop></remove> */
	    ne_buffer_concat(body, 
			   "<remove><prop><", items[n].name->name, " xmlns=\"",
			   items[n].name->nspace, "\"/></prop></remove>" EOL, 
			   NULL);
	    break;
	}
    }	

    ne_buffer_zappend(body, "</propertyupdate>" EOL);

    ne_set_request_body_buffer(req, body->data, ne_buffer_size(body));
    ne_add_request_header(req, "Content-Type", "text/xml"); /* TODO: UTF-8? */
    
    ret = ne_simple_request(sess, req);
    
    ne_buffer_destroy(body);

    return ret;
}

/* Compare two property names. */
static int pnamecmp(const ne_propname *pn1, const ne_propname *pn2)
{
    return (strcasecmp(pn1->nspace, pn2->nspace) ||
	    strcasecmp(pn1->name, pn2->name));
}

/* Find property in 'set' with name 'pname'.  If found, set pstat_ret
 * to the containing propstat, likewise prop_ret, and returns zero.
 * If not found, returns non-zero.  */
static int findprop(const ne_prop_result_set *set, const ne_propname *pname,
		    struct propstat **pstat_ret, struct prop **prop_ret)
{
    
    int ps, p;

    for (ps = 0; ps < set->numpstats; ps++) {
	for (p = 0; p < set->pstats[ps].numprops; p++) {
	    struct prop *prop = &set->pstats[ps].props[p];

	    if (pnamecmp(&prop->pname, pname) == 0) {
		if (pstat_ret != NULL)
		    *pstat_ret = &set->pstats[ps];
		if (prop_ret != NULL)
		    *prop_ret = prop;
		return 0;
	    }
	}
    }

    return -1;
}

const char *ne_propset_value(const ne_prop_result_set *set,
			      const ne_propname *pname)
{
    struct prop *prop;
    
    if (findprop(set, pname, NULL, &prop)) {
	return NULL;
    } else {
	return prop->value;
    }
}

const char *ne_propset_lang(const ne_prop_result_set *set,
			     const ne_propname *pname)
{
    struct prop *prop;

    if (findprop(set, pname, NULL, &prop)) {
	return NULL;
    } else {
	return prop->lang;
    }
}

void *ne_propfind_current_private(ne_propfind_handler *handler)
{
    return handler->current->private;
}

void *ne_propset_private(const ne_prop_result_set *set)
{
    return set->private;
}

int ne_propset_iterate(const ne_prop_result_set *set,
			ne_propset_iterator iterator, void *userdata)
{
    int ps, p;

    for (ps = 0; ps < set->numpstats; ps++) {
	for (p = 0; p < set->pstats[ps].numprops; p++) {
	    struct prop *prop = &set->pstats[ps].props[p];
	    int ret = iterator(userdata, &prop->pname, prop->value, 
			       &set->pstats[ps].status);
	    if (ret)
		return ret;

	}
    }

    return 0;
}

const ne_status *ne_propset_status(const ne_prop_result_set *set,
				      const ne_propname *pname)
{
    struct propstat *pstat;
    
    if (findprop(set, pname, &pstat, NULL)) {
	/* TODO: it is tempting to return a dummy status object here
	 * rather than NULL, which says "Property result was not given
	 * by server."  but I'm not sure if this is best left to the
	 * client.  */
	return NULL;
    } else {
	return &pstat->status;
    }
}


/* Pick up all properties as flat properties. */
static int check_context(ne_xml_elmid parent, ne_xml_elmid child)
{
    if (child == NE_ELM_unknown && parent == NE_ELM_prop)
	return NE_XML_VALID;

    return NE_XML_DECLINE;
}

static void *start_response(void *userdata, const char *href)
{
    ne_prop_result_set *set = ne_calloc(sizeof(*set));
    ne_propfind_handler *hdl = userdata;

    set->href = ne_strdup(href);

    if (hdl->private_creator != NULL) {
	set->private = hdl->private_creator(hdl->private_userdata, href);
    }

    hdl->current = set;

    return set;
}

static void *start_propstat(void *userdata, void *response)
{
    ne_prop_result_set *set = response;
    int n;
    struct propstat *pstat;

    n = set->numpstats;
    set->pstats = realloc(set->pstats, sizeof(struct propstat) * (n+1));
    set->numpstats = n+1;

    pstat = &set->pstats[n];
    memset(pstat, 0, sizeof(*pstat));
    
    /* And return this as the new pstat. */
    return &set->pstats[n];
}

static int 
startelm(void *userdata, const struct ne_xml_elm *elm, 
	 const char **atts)
{
    ne_propfind_handler *hdl = userdata;
    struct propstat *pstat = ne_207_get_current_propstat(hdl->parser207);
    struct prop *prop;
    int n;
    const char *lang;

    /* Paranoia */
    if (pstat == NULL) {
	NE_DEBUG(NE_DBG_XML, "gp_startelm: No propstat found, or not my element.");
	return -1;
    }

    /* Add a property to this propstat */
    n = pstat->numprops;

    pstat->props = realloc(pstat->props, sizeof(struct prop) * (n + 1));
    pstat->numprops = n+1;

    /* Fill in the new property. */
    prop = &pstat->props[n];

    prop->pname.name = prop->name = ne_strdup(elm->name);
    prop->pname.nspace = prop->nspace = ne_strdup(elm->nspace);
    prop->value = NULL;

    NE_DEBUG(NE_DBG_XML, "Got property #%d: %s@@%s.\n", n, 
	  prop->nspace, prop->name);

    /* This is under discussion at time of writing (April '01), and it
     * looks like we need to retrieve the xml:lang property from any
     * element here or above.
     *
     * Also, I think we might need attribute namespace handling here.  */
    lang = ne_xml_get_attr(atts, "xml:lang");
    if (lang != NULL) {
	prop->lang = ne_strdup(lang);
	NE_DEBUG(NE_DBG_XML, "Property language is %s\n", prop->lang);
    } else {
	prop->lang = NULL;
    }

    return 0;
}

static int 
endelm(void *userdata, const struct ne_xml_elm *elm, const char *cdata)
{
    ne_propfind_handler *hdl = userdata;
    struct propstat *pstat = ne_207_get_current_propstat(hdl->parser207);
    int n;

    if (pstat == NULL) {
	NE_DEBUG(NE_DBG_XML, "gp_endelm: No propstat found, or not my element.");
	return -1;
    }

    n = pstat->numprops - 1;

    NE_DEBUG(NE_DBG_XML, "Value of property #%d is %s\n", n, cdata);
    
    pstat->props[n].value = ne_strdup(cdata);

    return 0;
}

static void end_propstat(void *userdata, void *pstat_v, 
			 const char *status_line, const ne_status *status,
			 const char *description)
{
    struct propstat *pstat = pstat_v;

    /* If we get a non-2xx response back here, we wipe the value for
     * each of the properties in this propstat, so the caller knows to
     * look at the status instead. It's annoying, since for each prop
     * we will have done an unnecessary strdup("") above, but there is
     * no easy way round that given the fact that we don't know
     * whether we've got an error or not till after we get the
     * property element.
     *
     * Interestingly IIS breaks the 2518 DTD and puts the status
     * element first in the propstat. This is useful since then we
     * *do* know whether each subsequent empty prop element means, but
     * we can't rely on that here. */
    if (status->klass != 2) {
	int n;
	
	for (n = 0; n < pstat->numprops; n++) {
	    free(pstat->props[n].value);
	    pstat->props[n].value = NULL;
	}
    }

    pstat->status = *status;
}

/* Frees up a results set */
static void free_propset(ne_prop_result_set *set)
{
    int n;
    
    for (n = 0; n < set->numpstats; n++) {
	int m;
	struct propstat *p = &set->pstats[n];

	for (m = 0; m < p->numprops; m++) {
	    free(p->props[m].nspace);
	    free(p->props[m].name);
	    NE_FREE(p->props[m].lang);
	    NE_FREE(p->props[m].value);
	}

	free(set->pstats[n].props);
    }

    free(set->pstats);
    free(set);	 
}

static void end_response(void *userdata, void *resource,
			 const char *status_line,
			 const ne_status *status,
			 const char *description)
{
    ne_propfind_handler *handler = userdata;
    ne_prop_result_set *set = resource;
    
    /* TODO: Handle status here too? The status element is mandatory
     * inside each propstat, so, not much point probably. */

    /* Pass back the results for this resource. */
    if (handler->callback != NULL) {
	handler->callback(handler->userdata, set->href, set);
    }

    free(set->href);

    /* Clean up the propset tree we've just built. */
    free_propset(set);
}

ne_propfind_handler *
ne_propfind_create(ne_session *sess, const char *uri, int depth)
{
    ne_propfind_handler *ret = ne_calloc(sizeof(ne_propfind_handler));

    ret->parser = ne_xml_create();
    ret->parser207 = ne_207_create(ret->parser, ret);
    ret->sess = sess;
    ret->body = ne_buffer_create();
    ret->request = ne_request_create(sess, "PROPFIND", uri);

    ne_add_depth_header(ret->request, depth);

    ne_207_set_response_handlers(ret->parser207, 
				  start_response, end_response);

    ne_207_set_propstat_handlers(ret->parser207, start_propstat,
				  end_propstat);

    /* The start of the request body is fixed: */
    ne_buffer_concat(ret->body, 
		    "<?xml version=\"1.0\" encoding=\"utf-8\"?>" EOL 
		    "<propfind xmlns=\"DAV:\">", NULL);

    return ret;
}

/* Destroy a propfind handler */
void ne_propfind_destroy(ne_propfind_handler *handler)
{
    ne_207_destroy(handler->parser207);
    ne_xml_destroy(handler->parser);
    ne_buffer_destroy(handler->body);
    ne_request_destroy(handler->request);
    free(handler);    
}

int ne_simple_propfind(ne_session *sess, const char *href, int depth,
			const ne_propname *props,
			ne_props_result results, void *userdata)
{
    ne_propfind_handler *hdl;
    int ret;

    hdl = ne_propfind_create(sess, href, depth);
    if (props != NULL) {
	ret = ne_propfind_named(hdl, props, results, userdata);
    } else {
	ret = ne_propfind_allprop(hdl, results, userdata);
    }
	
    ne_propfind_destroy(hdl);
    
    return ret;
}

int ne_propnames(ne_session *sess, const char *href, int depth,
		  ne_props_result results, void *userdata)
{
    ne_propfind_handler *hdl;
    int ret;

    hdl = ne_propfind_create(sess, href, depth);

    ne_buffer_zappend(hdl->body, "<propname/></propfind>");

    ret = propfind(hdl, results, userdata);

    ne_propfind_destroy(hdl);

    return ret;
}

void ne_propfind_set_private(ne_propfind_handler *hdl,
			      ne_props_create_complex creator,
			      void *userdata)
{
    hdl->private_creator = creator;
    hdl->private_userdata = userdata;
}
