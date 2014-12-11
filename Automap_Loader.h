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
	static PHP_METHOD(Automap, get_ ## _name); \
	static PHP_METHOD(Automap, require_ ## _name); \

/*============================================================================*/

static PHP_METHOD(Automap, autoload_hook);
static void Automap_Loader_register_hook(TSRMLS_D);
static int Automap_resolve_symbol(char type, char *symbol, int slen, int autoload
	, int exception TSRMLS_DC);

AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(function)
AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(constant)
AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(class)
AUTOMAP_DECLARE_GET_REQUIRE_FUNCTIONS(extension)

static int MINIT_Automap_Loader(TSRMLS_D);
static int MSHUTDOWN_Automap_Loader(TSRMLS_D);
static int RINIT_Automap_Loader(TSRMLS_D);
static int RSHUTDOWN_Automap_Loader(TSRMLS_D);

/*============================================================================*/
#endif
