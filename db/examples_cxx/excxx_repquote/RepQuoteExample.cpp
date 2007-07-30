/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001,2007 Oracle.  All rights reserved.
 *
 * $Id: RepQuoteExample.cpp,v 1.15 2007/05/17 15:15:31 bostic Exp $
 */

/*
 * In this application, we specify all communication via the command line.  In
 * a real application, we would expect that information about the other sites
 * in the system would be maintained in some sort of configuration file.  The
 * critical part of this interface is that we assume at startup that we can
 * find out
 * 	1) what host/port we wish to listen on for connections,
 * 	2) a (possibly empty) list of other sites we should attempt to connect
 * 	to; and
 * 	3) what our Berkeley DB home environment is.
 *
 * These pieces of information are expressed by the following flags.
 * -m host:port (required; m stands for me)
 * -o host:port (optional; o stands for other; any number of these may be
 *	specified)
 * -h home directory
 * -n nsites (optional; number of sites in replication group; defaults to 0
 *	in which case we try to dynamically compute the number of sites in
 *	the replication group)
 * -p priority (optional: defaults to 100)
 */

#include <iostream>

#include <db_cxx.h>
#include "RepConfigInfo.h"

using std::cout;
using std::cin;
using std::cerr;
using std::endl;
using std::flush;

#define	CACHESIZE	(10 * 1024 * 1024)
#define	DATABASE	"quote.db"
#define	SLEEPTIME	3

const char *progname = "excxx_repquote";

#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define	sleep(s)		Sleep(1000 * (s))

extern "C" {
  extern int getopt(int, char * const *, const char *);
  extern char *optarg;
}
#else
#include <errno.h>
#endif

// Struct used to store information in Db app_private field.
typedef struct {
	int is_master;
} APP_DATA;

class RepQuoteExample
{
public:
	RepQuoteExample();
	int init(RepConfigInfo* config);
	int doloop();
	int terminate();

	static void event_callback(DbEnv * dbenv, u_int32_t which, void *info);

private:
	// disable copy constructor.
	RepQuoteExample(const RepQuoteExample &);
	void operator = (const RepQuoteExample &);

	// internal data members.
	APP_DATA        app_data;
	RepConfigInfo   *app_config;
	DbEnv           cur_env;

	// private methods.
	static int print_stocks(Db *dbp);
};

static void usage()
{
	cerr << "usage: " << progname << endl
	    << "[-h home][-o host:port][-m host:port][-f host:port]"
		<< "[-n nsites][-p priority]" << endl;

	cerr << "\t -m host:port (required; m stands for me)" << endl
		<< "\t -o host:port (optional; o stands for other; any "
		<< "number of these may be specified)" << endl
		<< "\t -h home directory" << endl
		<< "\t -n nsites (optional; number of sites in replication "
		<< "group; defaults to 0" << endl
		<< "\t	in which case we try to dynamically compute the "
		<< "number of sites in" << endl
		<< "\t	the replication group)" << endl
		<< "\t -p priority (optional: defaults to 100)" << endl;

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	RepConfigInfo config;
	char ch, *portstr, *tmphost;
	int tmpport;
	bool tmppeer;
	int ret;

	// Extract the command line parameters
	while ((ch = getopt(argc, argv, "Cf:h:Mm:n:o:p:v")) != EOF) {
		tmppeer = false;
		switch (ch) {
		case 'C':
			config.start_policy = DB_REP_CLIENT;
			break;
		case 'h':
			config.home = optarg;
			break;
		case 'M':
			config.start_policy = DB_REP_MASTER;
			break;
		case 'm':
			config.this_host.host = strtok(optarg, ":");
			if ((portstr = strtok(NULL, ":")) == NULL) {
				cerr << "Bad host specification." << endl;
				usage();
			}
			config.this_host.port = (unsigned short)atoi(portstr);
			config.got_listen_address = true;
			break;
		case 'n':
			config.totalsites = atoi(optarg);
			break;
		case 'f':
			tmppeer = true; // FALLTHROUGH
		case 'o':
			tmphost = strtok(optarg, ":");
			if ((portstr = strtok(NULL, ":")) == NULL) {
				cerr << "Bad host specification." << endl;
				usage();
			}
			tmpport = (unsigned short)atoi(portstr);

			config.addOtherHost(tmphost, tmpport, tmppeer);

			break;
		case 'p':
			config.priority = atoi(optarg);
			break;
		case 'v':
			config.verbose = true;
			break;
		case '?':
		default:
			usage();
		}
	}

	// Error check command line.
	if ((!config.got_listen_address) || config.home == NULL)
		usage();

	RepQuoteExample runner;
	try {
		if((ret = runner.init(&config)) != 0)
			goto err;
		if((ret = runner.doloop()) != 0)
			goto err;
	} catch (DbException dbe) {
		cerr << "Caught an exception during initialization or"
		    << " processing: " << dbe.what() << endl;
	}
err:
	runner.terminate();
	return 0;
}

RepQuoteExample::RepQuoteExample() : app_config(0), cur_env(0)
{
	app_data.is_master = 0; // assume I start out as client
}

int RepQuoteExample::init(RepConfigInfo *config)
{
	int ret = 0;

	app_config = config;

	cur_env.set_app_private(&app_data);
	cur_env.set_errfile(stderr);
	cur_env.set_errpfx(progname);
	cur_env.set_event_notify(event_callback);
	cur_env.repmgr_set_ack_policy(DB_REPMGR_ACKS_ALL);

	if ((ret = cur_env.repmgr_set_local_site(app_config->this_host.host,
		app_config->this_host.port, 0)) != 0) {
		// should throw an exception anyway.
		cerr << "Could not set listen address to host:port "
		    << app_config->this_host.host << ":"
		    << app_config->this_host.port
		    << "error: " << ret << endl;
		cerr << "WARNING: This should have been an exception." << endl;
	}

	for ( REP_HOST_INFO *cur = app_config->other_hosts; cur != NULL;
		cur = cur->next) {
		if ((ret = cur_env.repmgr_add_remote_site(cur->host, cur->port,
			NULL, cur->peer ? DB_REPMGR_PEER : 0)) != 0) {
				// should have resulted in an exception.
				cerr << "could not add site." << endl
				    << "WARNING: This should have been an exception." << endl;
		}
	}

	if (app_config->totalsites > 0) {
		try {
			if ((ret = cur_env.rep_set_nsites(app_config->totalsites)) != 0)
				cur_env.err(ret, "set_nsites");
		} catch (DbException dbe) {
			cerr << "rep_set_nsites call failed. Continuing." << endl;
			// non-fatal to the test app.
		}
	}

	cur_env.rep_set_priority(app_config->priority);

	/*
	 * We can now open our environment, although we're not ready to
	 * begin replicating.  However, we want to have a dbenv around
	 * so that we can send it into any of our message handlers.
	 */
	cur_env.set_cachesize(0, CACHESIZE, 0);
	cur_env.set_flags(DB_TXN_NOSYNC, 1);

	try {
		cur_env.open(app_config->home, DB_CREATE | DB_RECOVER |
		    DB_THREAD | DB_INIT_REP | DB_INIT_LOCK | DB_INIT_LOG |
		    DB_INIT_MPOOL | DB_INIT_TXN, 0);
	} catch(DbException dbe) {
		cerr << "Caught an exception during DB environment open." << endl
		    << "Ensure that the home directory is created prior to starting"
		    << " the application." << endl;
		ret = ENOENT;
		goto err;
	}
	if (app_config->verbose &&
	    (ret = cur_env.set_verbose(DB_VERB_REPLICATION, 1)) != 0)
		goto err;

	if ((ret = cur_env.repmgr_start(3, app_config->start_policy)) != 0)
		goto err;

err:
	return ret;
}

int RepQuoteExample::terminate()
{
	try {
		cur_env.close(0);
	} catch (DbException dbe) {
		cout << "error closing environment: " << dbe.what() << endl;
	}
	return 0;
}

#define BUFSIZE 1024
int RepQuoteExample::doloop()
{
	Db *dbp;
	Dbt key, data;
	char buf[BUFSIZE], *rbuf;
	int ret, was_master;

	dbp = NULL;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	ret = was_master = 0;

	for (;;) {
		if (dbp == NULL) {
			dbp = new Db(&cur_env, 0);

			// Set page size small so page allocation is cheap.
			if ((ret = dbp->set_pagesize(512)) != 0)
				goto err;

			try {
				dbp->open(NULL, DATABASE, NULL, DB_BTREE,
			        app_data.is_master ? DB_CREATE | DB_AUTO_COMMIT :
			        DB_AUTO_COMMIT, 0);
			} catch(DbException dbe) {
				/* It is expected that the database open will fail
				 * when client sites first start up.
				 * It can take a while for the master site to be found
				 * and synced, and no DB is available until then.
				 */
				if (dbe.get_errno() == ENOENT) {
					cout << "No stock db available yet - retrying." << endl;
					try {
						dbp->close(0);
					} catch (DbException dbe2) {
						cout << "Unexpected error closing after failed" <<
							" open, message: " << dbe2.what() << endl;
						dbp = NULL;
						goto err;
					}
					dbp = NULL;
					sleep(SLEEPTIME);
					continue;
				} else {
					cur_env.err(ret, "DB->open");
					throw dbe;
				}
			}
		}

		cout << "QUOTESERVER" ;
		if (!app_data.is_master)
			cout << "(read-only)";
		cout << "> " << flush;

		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		if (strtok(&buf[0], " \t\n") == NULL) {
			switch ((ret = print_stocks(dbp))) {
			case 0:
				continue;
			case DB_REP_HANDLE_DEAD:
				(void)dbp->close(DB_NOSYNC);
				dbp = NULL;
			default:
				dbp->err(ret, "Error traversing data");
				goto err;
			}
		}
		rbuf = strtok(NULL, " \t\n");
		if (rbuf == NULL || rbuf[0] == '\0') {
			if (strncmp(buf, "exit", 4) == 0 ||
			    strncmp(buf, "quit", 4) == 0)
				break;
			cur_env.errx("Format: TICKER VALUE");
			continue;
		}

		if (!app_data.is_master) {
			cur_env.errx("Can't update at client");
			continue;
		}

		key.set_data(buf);
		key.set_size((u_int32_t)strlen(buf));

		data.set_data(rbuf);
		data.set_size((u_int32_t)strlen(rbuf));

		if ((ret = dbp->put(NULL, &key, &data, DB_AUTO_COMMIT)) != 0)
		{
			dbp->err(ret, "DB->put");
			if (ret != DB_KEYEXIST)
				goto err;
		}
	}

err:	if (dbp != NULL)
		(void)dbp->close(DB_NOSYNC);

	return (ret);
}

void RepQuoteExample::event_callback(DbEnv* dbenv, u_int32_t which, void *info)
{
	APP_DATA *app = (APP_DATA*)dbenv->get_app_private();

	info = NULL;				/* Currently unused. */

	switch (which) {
	case DB_EVENT_REP_MASTER:
		app->is_master = 1;
		break;

	case DB_EVENT_REP_CLIENT:
		app->is_master = 0;
		break;

	case DB_EVENT_REP_STARTUPDONE: /* FALLTHROUGH */
	case DB_EVENT_REP_NEWMASTER:
        case DB_EVENT_REP_PERM_FAILED:
		// I don't care about this one, for now.
		break;

	default:
		dbenv->errx("ignoring event %d", which);
	}
}

int RepQuoteExample::print_stocks(Db *dbp)
{
	Dbc *dbc;
	Dbt key, data;
#define	MAXKEYSIZE	10
#define	MAXDATASIZE	20
	char keybuf[MAXKEYSIZE + 1], databuf[MAXDATASIZE + 1];
	int ret, t_ret;
	u_int32_t keysize, datasize;

 	if ((ret = dbp->cursor(NULL, &dbc, 0)) != 0) {
		dbp->err(ret, "can't open cursor");
		return (ret);
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	cout << "\tSymbol\tPrice" << endl
	    << "\t======\t=====" << endl;

	for (ret = dbc->get(&key, &data, DB_FIRST);
	    ret == 0;
	    ret = dbc->get(&key, &data, DB_NEXT)) {
		keysize = key.get_size() > MAXKEYSIZE ? MAXKEYSIZE : key.get_size();
		memcpy(keybuf, key.get_data(), keysize);
		keybuf[keysize] = '\0';

		datasize = data.get_size() >=
		    MAXDATASIZE ? MAXDATASIZE : data.get_size();
		memcpy(databuf, data.get_data(), datasize);
		databuf[datasize] = '\0';

		cout << "\t" << keybuf << "\t" << databuf << endl;
	}
	cout << endl << flush;

	if ((t_ret = dbc->close()) != 0 && ret == 0)
		ret = t_ret;

	switch (ret) {
	case 0:
	case DB_NOTFOUND:
	case DB_LOCK_DEADLOCK:
		return (0);
	default:
		return (ret);
	}
}

