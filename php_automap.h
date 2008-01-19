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

/*#define AUTOMAP_DEBUG*/

#ifndef PHP_AUTOMAP_H
#define PHP_AUTOMAP_H 1

#ifdef AUTOMAP_DEBUG
#define UT_DEBUG
#endif

#include "utils.h"

/*---------------------------------------------------------------*/

#define AUTOMAP_EXT_VERSION "1.1.0"

#define AUTOMAP_API "1.1.0"

#define PHP_AUTOMAP_EXTNAME "automap"

GLOBAL zend_module_entry automap_module_entry;

#define phpext_automap_ptr &automap_module_entry

/*---------------------------------------------------------------*/

static DECLARE_CZVAL(false);
static DECLARE_CZVAL(true);
static DECLARE_CZVAL(null);

static DECLARE_CZVAL(Automap);
static DECLARE_CZVAL(spl_autoload_register);

/* Hash keys */

static DECLARE_HKEY(map);
static DECLARE_HKEY(options);
static DECLARE_HKEY(automap);

static DECLARE_HKEY(mp_property_name);

/*============================================================================*/

typedef struct _Automap_Mnt_Info {
	zval *mnt;					/* (String zval *) */
	ulong hash;					/* Mnt hash */
	unsigned long *refcountp;
	int mnt_count;				/* Allows to mount/umount the same map */
	zval *instance;				/* Automap object (NULL until created) */
	zval *path;					/* (String zval *) */
	zval *base_dir;				/* (String zval *) */
	zval *flags;				/* (Long zval *) */
	int order;

	/* Persistent data */

	zval *min_version;			/* (String zval *) */
	zval *version;				/* (String zval *) */
	zval *options;				/* (Array zval *) */
	zval *symbols;				/* (Array zval *) */
} Automap_Mnt_Info;

/*============================================================================*/

ZEND_BEGIN_MODULE_GLOBALS(automap)

HashTable *mtab;				/* Null before the first mount */
Automap_Mnt_Info **mount_order; /* Array of (Automap_Mnt_Info *)|NULL */
int mcount;						/* Size of the mount_order table */

zval **failure_handlers;
int fh_count;					/* Failure handler count */

zval **success_handlers;
int sh_count;					/* Success handler count */

ZEND_END_MODULE_GLOBALS(automap)

#ifdef ZTS
#define AUTOMAP_G(v) TSRMG(automap_globals_id, zend_automap_globals *, v)
#else
#define AUTOMAP_G(v) (automap_globals.v)
#endif

/*---------------------------------------------------------------*/

/* We need a private property here, so that it cannot be accessed nor
   modified by a malicious PHP script */

#define MP_PROPERTY_NAME "\0Automap\0m"

/*---------------------------------------------------------------*/
#endif							/* PHP_AUTOMAP_H */
