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

/* #define AUTOMAP_DEBUG */

#ifndef __PHP_AUTOMAP_H
#define __PHP_AUTOMAP_H 1

#ifdef AUTOMAP_DEBUG
#define UT_DEBUG
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "TSRM/TSRM.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_versioning.h"
#include "zend_constants.h"
#include "zend_execute.h"
#include "zend_errors.h"
#include "TSRM/tsrm_virtual_cwd.h"

#include "utils.h"

#include "Automap_Handlers.h"
#include "Automap_Instance.h"
#include "Automap_Key.h"
#include "Automap_Loader.h"
#include "Automap_Pmap.h"
#include "Automap_Mnt.h"
#include "Automap_Type.h"
#include "Automap_Util.h"

/*---------------------------------------------------------------*/

#define AUTOMAP_EXT_VERSION "2.0.0"

#define AUTOMAP_API "2.0.0"

#define AUTOMAP_MIN_MAP_VERSION "1.1.0"

#define PHP_AUTOMAP_EXTNAME "automap"

GLOBAL zend_module_entry automap_module_entry;

#define phpext_automap_ptr &automap_module_entry

/*---------------------------------------------------------------*/

static DECLARE_CZVAL(false);
static DECLARE_CZVAL(true);
static DECLARE_CZVAL(null);

static DECLARE_CZVAL(Automap);
static DECLARE_CZVAL(spl_autoload_register);
static DECLARE_CZVAL(Automap__autoload_hook);

/* Hash keys */

static DECLARE_HKEY(map);
static DECLARE_HKEY(options);
static DECLARE_HKEY(automap);

static DECLARE_HKEY(mp_property_name);

/*============================================================================*/

ZEND_BEGIN_MODULE_GLOBALS(automap)

HashTable mnttab;
Automap_Mnt **mount_order; /* Array of (Automap_Mnt *)|NULL */
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
#endif							/* __PHP_AUTOMAP_H */
