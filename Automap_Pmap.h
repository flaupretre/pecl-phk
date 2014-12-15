/*
  +----------------------------------------------------------------------+
  | Automap extension <http://automap.tekwire.net>                       |
  +----------------------------------------------------------------------+
  | Copyright (c) 2005-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Francois Laupretre <francois@tekwire.net>                    |
  +----------------------------------------------------------------------+
*/

#ifndef __AUTOMAP_PMAP_H
#define __AUTOMAP_PMAP_H

/*---------------------------------------------------------------*/

#define AUTOMAP_MAGIC "AUTOMAP  M\024\010\6\3"

/*---------------------------------------------------------------*/

typedef struct {				/* Persistent */
	char stype;					/* Symbol type */
	zval zsname;				/* Symbol name (case sensitive) */
	char ftype;					/* Target type */
	zval zfpath;				/* Target path (as in map) */
	zval zfapath;				/* Absolute target path */
} Automap_Pmap_Entry;

typedef struct {				/* Persistent */
	zval *zufid;				/* (String zval *) */
	ulong ufid_hash;
	zval *zmin_version;			/* Map requires at least this runtime version (String) */
	zval *zversion;				/* Map was created with this creator version (String) */
	char map_major_version;
	zval *zoptions;				/* Array */
	zval *zsymbols;				/* Array of Automap_Pmap_Entry structures */
} Automap_Pmap;

/*---------------------------------------------------------------*/
/* Persistent data */

static HashTable pmap_array;	/* Key=UFID, Value=(Automap_ptab *) */

StaticMutexDeclare(pmap_array);

/*============================================================================*/

static Automap_Pmap *Automap_Pmap_get(zval *zufidp, ulong hash TSRMLS_DC);

static int Automap_Pmap_create_entry(zval **zpp
#if PHP_API_VERSION >= 20090626
		/* PHP 5.3+ requires this */
	TSRMLS_DC
#endif
	, int num_args, va_list va, zend_hash_key *hash_key
#if PHP_API_VERSION < 20090626
	TSRMLS_DC
#endif
	);

static Automap_Pmap *Automap_Pmap_get_or_create(zval *zapathp
	, long flags TSRMLS_DC);
static Automap_Pmap *Automap_Pmap_get_or_create_extended(zval *zpathp
	, char *buf, int buflen, zval *zufidp, ulong hash
	, zval *zbase_pathp_arg, long flags TSRMLS_DC);
static void Automap_Pmap_dtor(Automap_Pmap *pmp);
static void Automap_Pmap_Entry_dtor(Automap_Pmap_Entry *pep);
static Automap_Pmap_Entry *Automap_Pmap_find_key(Automap_Pmap *pmp
	, zval *zkey, ulong hash TSRMLS_DC);

static int MINIT_Automap_Pmap(TSRMLS_D);
static int MSHUTDOWN_Automap_Pmap(TSRMLS_D);
static int RINIT_Automap_Pmap(TSRMLS_D);
static int RSHUTDOWN_Automap_Pmap(TSRMLS_D);

/*============================================================================*/
#endif
