/* 
   nbrowse, dummy WebDAV browsing in GNOME
   Copyright (C) 2000, Joe Orton <joe@orton.demon.co.uk>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Id: main.c,v 1.3 2000/07/16 16:15:44 joe Exp 
*/

/* 
  Aims to demonstrate that a blocking-IO based HTTP library can be
  easily integrated with a GUI polling loop.
   
  TODO: 
  - Stop it crashing all the time
  - Allow one PROPFIND at a time, i.e., ignore clicks once
    already loading a tree
  - Fetch lockdiscovery and display "locked" icons
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <signal.h>

#include <pthread.h>

#include <gnome.h>

#include <gtk/gtktree.h>


#include <neon_config.h>

#include <dav_props.h>
#include <dav_basic.h>
#include <uri.h>
#include <xalloc.h>

#include "basename.h"
#include "interface.h"
#include "support.h"

#define ELM_getcontentlength (1001)
#define ELM_resourcetype (1002)
#define ELM_getlastmodified (1003)
#define ELM_executable (1004)
#define ELM_collection (1005)
#define ELM_displayname (1006)

enum resource_type {
    res_normal,
    res_collection,
    res_reference,
};

http_session *sess;
/* since we let >1 thread spawn at a time, 
 * enforce mutual exclusion over access to the session state
 */
pthread_mutex_t sess_mutex = PTHREAD_MUTEX_INITIALIZER;

struct browser {
    const char *uri;
    GtkTree *tree;
    GnomeIconList *list;
    unsigned int thread_running:1;
    pthread_t thread;
};

struct resource {
    char *uri;
    char *displayname;
    enum resource_type type;
    size_t size;
    time_t modtime;
    int is_executable;
    struct resource *next;
};

static int start_propfind( struct browser *b );

static int check_context( hip_xml_elmid child, hip_xml_elmid parent )
{
    /* FIXME */
    return 0;    
}

/* Does a PROPFIND to load URI at tree */
static void load_tree( GtkWidget *tree, const char *uri ) 
{
    struct browser *b = xmalloc(sizeof(struct browser));

    /* Create the tree */
    b->tree = GTK_TREE(tree);
    b->list = GNOME_ICON_LIST(iconlist);
    b->uri = uri;

    start_propfind(b);
}

static void select_node_cb( GtkTreeItem *item )
{
    char *uri = gtk_object_get_data( GTK_OBJECT(item), "uri" );
    static char *loaded = "l";

    printf( "selected: %s\n", uri );

#if 0
    if( gtk_object_get_data( GTK_OBJECT(item), "is_loaded" ) == NULL ) {
	gtk_object_set_data( GTK_OBJECT(item), "is_loaded", loaded );
    } else {
	printf( "already loaded this node.\n" );
	return;
    }
#endif

    gnome_icon_list_clear(GNOME_ICON_LIST(iconlist));
    gtk_tree_remove_items (GTK_TREE(item->subtree), GTK_TREE(item->subtree)->children);
    load_tree( item->subtree, uri );
}

static int end_element( void *userdata, const struct hip_xml_elm *elm, const char *cdata )
{
    struct resource *r;
    r = dav_propfind_get_current_resource( userdata );
    if( r == NULL ) {
	return 0;
    }
    DEBUG( DEBUG_HTTP, "In resource %s\n", r->uri );
    DEBUG( DEBUG_HTTP, "Property [%d], %s@@%s = %s\n", elm->id,
	   elm->nspace, elm->name, cdata?cdata:"undefined" );
    switch( elm->id ) {
    case ELM_collection:
	r->type = res_collection;
	break;
    case ELM_getcontentlength:
	if( cdata ) {
	    r->size = atoi(cdata);
	}
	break;
    case ELM_displayname:
	if( cdata ) {
	    r->displayname = xstrdup(cdata);
	}
	break;
    case ELM_getlastmodified:
	if( cdata ) {
	    r->modtime = http_dateparse(cdata);
	    /* will be -1 on error */
	}
	break;
    case ELM_executable:
	if( cdata ) {
	    if( strcasecmp( cdata, "T" ) == 0 ) {
		r->is_executable = 1;
	    } else {
		r->is_executable = 0;
	    }
	}
	break;
    }
    return 0;
}

static void *start_resource( void *userdata, const char *href )
{
    struct resource *res = xmalloc(sizeof(struct resource));
    memset(res,0,sizeof(struct resource));
    res->uri = xstrdup(href);
    return res;
}

static void end_resource( void *userdata, void *resource,
			  const char *status_line, const http_status *status,
			  const char *description )
{
    struct browser *b = userdata;
    char *abspath;
    struct resource *res = resource;
    char *leaf;

    if( res == NULL ) {
	return;
    }	

    DEBUG( DEBUG_HTTP, "Href: %s\n", res->uri );
    /* char * cast is valid since res->uri is char * */
    abspath = (char *) uri_abspath( res->uri );

    if( uri_compare( abspath, b->uri ) == 0 ) {
	return;
    }

    if( uri_has_trailing_slash(abspath) ) {
	abspath[strlen(abspath)-1] = '\0';
    }

    leaf = base_name(abspath);

    gdk_threads_enter();

    if( res->type == res_collection ) {
	GtkWidget *sub, *item;

	item = gtk_tree_item_new_with_label( leaf );
	gtk_tree_append( b->tree, item );

	sub = gtk_tree_new();	
	gtk_tree_item_set_subtree( GTK_TREE_ITEM(item), sub );
	gtk_object_set_data_full( GTK_OBJECT(item), "uri", 
				  xstrdup(res->uri), free );
	gtk_signal_connect (GTK_OBJECT(item), "select",
			    GTK_SIGNAL_FUNC(select_node_cb), item);
	gtk_signal_connect (GTK_OBJECT(item), "expand",
			    GTK_SIGNAL_FUNC(select_node_cb), item);
	gtk_widget_show( item );

	gnome_icon_list_append( b->list, "/usr/share/pixmaps/mc/i-directory.png", leaf );

    } else {
	gnome_icon_list_append( b->list, "/usr/share/pixmaps/mc/i-regular.png", leaf );
    }

    gdk_threads_leave();

    free( res->uri);
    free( res );
}

/* Run in a separate thread */
static void *do_propfind( void *ptr )
{
    dav_propfind_handler *ph;
    static const dav_propname props[] = {
	{ "DAV:", "getcontentlength" },
	{ "DAV:", "resourcetype" },
	{ "DAV:", "getlastmodified" },
	{ "DAV:", "displayname" },
	{ "http://apache.org/dav/props/", "executable" },
	{ NULL }
    };
    static const struct hip_xml_elm elms[] = {
	{ "DAV:", "getcontentlength", ELM_getcontentlength, HIP_XML_CDATA },
	{ "DAV:", "resourcetype", ELM_resourcetype, 0 },
	{ "DAV:", "getlastmodified", ELM_getlastmodified, HIP_XML_CDATA },
	{ "http://apache.org/dav/props/", "executable", ELM_executable, HIP_XML_CDATA },
	{ "DAV:", "collection", ELM_collection, 0 },
	{ "DAV:", "displayname", ELM_displayname, HIP_XML_CDATA },
	{ NULL }
    };
    struct browser *b = ptr;

    pthread_detach(pthread_self());

    pthread_mutex_lock( &sess_mutex );
    
    ph = dav_propfind_create (sess, b->uri, DAV_DEPTH_ONE );

    dav_propfind_set_resource_handlers (ph, start_resource, end_resource);

    hip_xml_add_handler (dav_propfind_get_parser(ph), elms,
			 check_context, NULL, end_element, ph); 
    
    if (dav_propfind_named (ph, props, ptr) != HTTP_OK) {
	printf( "PROPFIND failed: %s\n", http_get_error(sess) );
    }

    pthread_mutex_unlock( &sess_mutex );
    
    free( b );

    return NULL;
}


static int start_propfind( struct browser *b )
{
    b->thread_running = 1;
    return pthread_create( &b->thread, NULL, do_propfind, b );
}

int
main (int argc, char **argv)
{
    GtkWidget *nbrowse;
    
    if( argc < 3 ) {
	printf("nbrowse: Usage 'nbrowse server.name.com /path/'.\n");
	return -1;
    }

#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
    textdomain (PACKAGE);
#endif
    
    /* Initialize threading */
    g_thread_init(NULL);
    
    signal( SIGPIPE, SIG_IGN );
   
    gnome_init ("nbrowse", NEON_VERSION, argc, argv);
    
    /* Create the main window */
    nbrowse = create_nbrowse ();
    gtk_widget_show (nbrowse);

    /* Create a new session. */
    sess = http_session_create();

    printf( "Using server: %s - path %s\n", argv[1], argv[2] );

    http_session_server( sess, argv[1], 80 );
    http_set_useragent( sess, "nbrowser/" NEON_VERSION );
    
    load_tree( tree, argv[2] );
    
    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();
    return 0;
}

