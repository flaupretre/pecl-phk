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

#ifndef __AUTOMAP_LOADER_H
#define __AUTOMAP_LOADER_H

/*---------------------------------------------------------------*/

#define AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(_name) \
	static PHP_METHOD(Automap, get ## _name); \
	static PHP_METHOD(Automap, require ## _name); \

/*============================================================================*/

static PHP_METHOD(Automap, autoloadHook);
static void Automap_Loader_register_hook(TSRMLS_D);
static int Automap_resolve_symbol(char type, char *symbol, int slen, zend_bool autoload
	, zend_bool exception TSRMLS_DC);
static PHP_METHOD(Automap, resolve);

AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(Function)
AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(Constant)
AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(Class)
AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(Extension)

static int MINIT_Automap_Loader(TSRMLS_D);
static int MSHUTDOWN_Automap_Loader(TSRMLS_D);
static int RINIT_Automap_Loader(TSRMLS_D);
static int RSHUTDOWN_Automap_Loader(TSRMLS_D);

/*============================================================================*/
#endif
