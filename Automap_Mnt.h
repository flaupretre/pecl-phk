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

/* $Id$ */

#ifndef __AUTOMAP_MNT_H
#define __AUTOMAP_MNT_H

#include "Automap_Pmap.h"

/*============================================================================*/

typedef struct _Automap_Mnt {	/* Per request */
	Automap_Pmap *map;			/* Persistent map */
	int mnt_count;				/* Allows to mount/umount the same map */
	zval *instance;				/* Automap object (NULL until created) */
	zval *zpath;				/* Map file path (String zval) */
	zval *zbase;				/* Map base directory (String zval) */
	ulong flags;				/* (Long zval) */
	int order;
} Automap_Mnt;

/*============================================================================*/

static void Automap_Mnt_dtor(Automap_Mnt *mp);
static void Automap_Mnt_remove(Automap_Mnt *mp TSRMLS_DC);
static Automap_Mnt *Automap_Mnt_create(Automap_Pmap *pmp, zval *zpathp, zval *zbasep
	, ulong flags TSRMLS_DC);
static Automap_Mnt *Automap_Mnt_get(zval *mnt, ulong hash, int exception TSRMLS_DC);
static PHP_METHOD(Automap, is_mounted);
static void Automap_Mnt_validate(zval * mnt, ulong hash TSRMLS_DC);
static PHP_METHOD(Automap, validate);
static Automap_Mnt *Automap_Mnt_mount(zval * path, zval * base_dir,
									   zval * mnt, ulong flags TSRMLS_DC);
static PHP_METHOD(Automap, mount);
static void Automap_umount(zval *mnt, ulong hash TSRMLS_DC);
static PHP_METHOD(Automap, umount);
static PHP_METHOD(Automap, mnt_list);
static char *Automap_Mnt_abs_path(Automap_Mnt *mp, Automap_Pmap_Entry *pep, int *lenp TSRMLS_DC);
static int Automap_Mnt_resolve_key(Automap_Mnt *mp, zval *zkey, ulong hash TSRMLS_DC);

static int MINIT_Automap_Mnt(TSRMLS_D);
static int MSHUTDOWN_Automap_Mnt(TSRMLS_D);
static int RINIT_Automap_Mnt(TSRMLS_D);
static int RSHUTDOWN_Automap_Mnt(TSRMLS_D);

/*============================================================================*/
#endif
