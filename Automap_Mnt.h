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

#ifndef __AUTOMAP_MNT_H
#define __AUTOMAP_MNT_H

#include "Automap_Pmap.h"

/*============================================================================*/

typedef struct _Automap_Mnt {	/* Per request */
	Automap_Pmap *map;			/* Persistent map */
	zval *map_object;			/* Automap_Map instance (NULL until created) */
	zval *zpath;				/* Map file absolute path (String zval) */
	ulong flags;				/* Load flags */
	long id;					/* Map ID (index in map_array) */
} Automap_Mnt;

/* Load flags */

#define AUTOMAP_FLAG_NO_AUTOLOAD	1
#define AUTOMAP_FLAG_CRC_CHECK		2
#define AUTOMAP_FLAG_PECL_LOAD		4

/*============================================================================*/

static void Automap_Mnt_dtor(Automap_Mnt *mp TSRMLS_DC);
static void Automap_Mnt_remove(Automap_Mnt *mp TSRMLS_DC);
static Automap_Mnt *Automap_Mnt_get(long id, int exception TSRMLS_DC);
static PHP_METHOD(Automap, isActiveID);
static void Automap_Mnt_array_add(Automap_Mnt *mp TSRMLS_DC);
static Automap_Mnt *Automap_Mnt_load_extended(zval *zpathp, zval *zufidp
	, ulong hash, zval *zbasep, Automap_Pmap *pmp, long flags TSRMLS_DC);
static Automap_Mnt *Automap_Mnt_load(zval *zpathp, long flags TSRMLS_DC);
static PHP_METHOD(Automap, load);
static void Automap_unload(long id TSRMLS_DC);
static PHP_METHOD(Automap, unload);
static PHP_METHOD(Automap, activeIDs);
static int Automap_Mnt_resolve_key(Automap_Mnt *mp, zval *zkey, ulong hash TSRMLS_DC);

static int MINIT_Automap_Mnt(TSRMLS_D);
static int MSHUTDOWN_Automap_Mnt(TSRMLS_D);
static int RINIT_Automap_Mnt(TSRMLS_D);
static int RSHUTDOWN_Automap_Mnt(TSRMLS_D);

/*============================================================================*/
#endif
