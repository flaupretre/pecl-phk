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

#ifndef __AUTOMAP_UTIL_H
#define __AUTOMAP_UTIL_H

/*============================================================================*/

static PHP_METHOD(Automap, min_map_version);
static void Automap_path_id(zval * path, zval **id_zp TSRMLS_DC);
static PHP_METHOD(Automap, path_id);
static int Automap_symbol_is_defined(char type, char *symbol
	, unsigned int slen TSRMLS_DC);
static PHP_METHOD(Automap, using_accelerator);
static PHP_METHOD(Automap, accel_techinfo);

static int MINIT_Automap_Util(TSRMLS_D);
static int MSHUTDOWN_Automap_Util(TSRMLS_D);
static int RINIT_Automap_Util(TSRMLS_D);
static int RSHUTDOWN_Automap_Util(TSRMLS_D);

/*============================================================================*/
#endif
