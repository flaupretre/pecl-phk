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

#define AUTOMAP_MAP_PROTOCOL	1

/*---------------------------------------------------------------*/

typedef struct {				/* Persistent */
	char stype;					/* Symbol type */
	zval zsname;				/* Symbol name (case sensitive) */
	char ftype;					/* Target type */
	zval zfapath;				/* Absolute target path */
} Automap_Pmap_Entry;

typedef struct {				/* Persistent */
	zval *zsymbols;				/* Array of Automap_Pmap_Entry structures */
} Automap_Pmap;

/*---------------------------------------------------------------*/
/* Persistent data */

static HashTable pmap_array;	/* Key=UFID, Value=(Automap_ptab *) */

StaticMutexDeclare(pmap_array);

/*============================================================================*/

static Automap_Pmap *Automap_Pmap_get(zval *zufidp, ulong hash TSRMLS_DC);
static int Automap_Pmap_create_entry(zval **zpp, zval **zsymbols TSRMLS_DC);
static Automap_Pmap *Automap_Pmap_get_or_create(zval *zapathp
	, long flags TSRMLS_DC);
static Automap_Pmap *Automap_Pmap_get_or_create_extended(zval *zpathp
	, zval *zufidp, ulong hash, zval *zbasePathp_arg, long flags TSRMLS_DC);
static void Automap_Pmap_dtor(Automap_Pmap *pmp TSRMLS_DC);
static void Automap_Pmap_Entry_dtor(Automap_Pmap_Entry *pep TSRMLS_DC);
static Automap_Pmap_Entry *Automap_Pmap_find_key(Automap_Pmap *pmp
	, zval *zkey, ulong hash TSRMLS_DC);
static void Automap_Pmap_exportEntry(Automap_Pmap_Entry *pep, zval *zp TSRMLS_DC);

static int MINIT_Automap_Pmap(TSRMLS_D);
static int MSHUTDOWN_Automap_Pmap(TSRMLS_D);
static int RINIT_Automap_Pmap(TSRMLS_D);
static int RSHUTDOWN_Automap_Pmap(TSRMLS_D);

/*============================================================================*/
#endif
