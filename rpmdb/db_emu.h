/*
 * This file emulates the db3/4 structures
 * ...this is supposed to be compatable w/ the _real_ db.h!
 */

#ifndef __DB_EMU_H
#define __DB_EMU_H

struct __db;		typedef struct __db DB;
struct __db_dbt;	typedef struct __db_dbt DBT;
struct __db_env;	typedef struct __db_env DB_ENV;
struct __dbc;		typedef struct __dbc DBC;
struct __db_txn;	typedef struct __db_txn DB_TXN;
struct __db_h_stat;	typedef struct __db_h_stat DB_HASH_STAT;

/* Database handle */
struct __db {
  void		*app_private;
};

struct __db_dbt {
  u_int32_t	size;
  void		*data;

  #define DB_DBT_MALLOC 0x01   /* We malloc the memory and hand off a copy. */
  u_int32_t	flags;
};

struct __db_env {
  void		*app_private;
};

struct __dbc {
  DB		*dbp;
};

struct __db_txn {
  /* NULL */ ;
};

struct __db_h_stat {
  u_int32_t	hash_nkeys;
};

#define DB_FAST_STAT 11
#define DB_KEYLAST 19
#define DB_NEXT 21
#define DB_SET 32
#define DB_WRITECURSOR 39
#define DB_NOTFOUND (-30990)

#endif
