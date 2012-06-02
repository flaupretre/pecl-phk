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

#ifndef __AUTOMAP_INSTANCE_H
#define __AUTOMAP_INSTANCE_H

#include "Automap_Mnt.h"

/*---------------------------------------------------------------*/
/* Using additional security here as property should always contain
   a valid order value (property is destroyed on umount */

#define AUTOMAP_GET_INSTANCE_DATA(_func) \
	zval **_tmp; \
	int _order,_valid; \
	Automap_Mnt *mp; \
	\
	DBG_MSG("Entering Automap::" #_func); \
	CHECK_MEM(); \
	_valid=0; \
	if (FIND_HKEY(Z_OBJPROP_P(getThis()),mp_property_name,&_tmp)==SUCCESS) { \
		_order=Z_LVAL_PP(_tmp); \
		if (_order<AUTOMAP_G(mcount)) { \
			mp=AUTOMAP_G(mount_order)[_order]; \
			if (mp) _valid=1; \
		} \
	} \
	if (!_valid) { \
		EXCEPTION_ABORT("Accessing invalid or unmounted object"); \
	}

/*---------------------------------------------------------------*/

static zval *Automap_instance_by_mp(Automap_Mnt *mp TSRMLS_DC);
static zval *Automap_instance(zval * mnt, ulong hash TSRMLS_DC);
static PHP_METHOD(Automap, instance);
static PHP_METHOD(Automap, is_valid);
static PHP_METHOD(Automap, mnt);
static PHP_METHOD(Automap, path);
static PHP_METHOD(Automap, base_dir);
static PHP_METHOD(Automap, flags);
static PHP_METHOD(Automap, options);
static PHP_METHOD(Automap, version);
static PHP_METHOD(Automap, min_version);
static void Automap_Instance_export_entry(Automap_Mnt *mp, Automap_Pmap_Entry *pep, zval *zp TSRMLS_DC);
static PHP_METHOD(Automap, symbols);
static PHP_METHOD(Automap, get_symbol);
static unsigned long Automap_symbol_count(Automap_Mnt *mp TSRMLS_DC);
static PHP_METHOD(Automap, option);
static PHP_METHOD(Automap, check);
static PHP_METHOD(Automap, show);
static PHP_METHOD(Automap, show_text);
static PHP_METHOD(Automap, show_html);
static PHP_METHOD(Automap, export);
static void Automap_Instance_set_constants(zend_class_entry * ce);

static int MINIT_Automap_Instance(TSRMLS_D);
static int MSHUTDOWN_Automap_Instance(TSRMLS_D);
static int RINIT_Automap_Instance(TSRMLS_D);
static int RSHUTDOWN_Automap_Instance(TSRMLS_D);

/*---------------------------------------------------------------*/
#endif
