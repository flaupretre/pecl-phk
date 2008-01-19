/*
  +----------------------------------------------------------------------+
  | Automap extension <http://automap.tekwire.net>                     |
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "TSRM/TSRM.h"
#include "ext/standard/info.h"

#define ALLOCATE

#include "php_automap.h"
#include "utils.h"
#include "Automap.h"

/*------------------------*/

ZEND_DECLARE_MODULE_GLOBALS(automap)

#ifdef COMPILE_DL_AUTOMAP
	ZEND_GET_MODULE(automap)
#endif

static int init_done=0;

/*------------------------*/

#include "utils.c"

#include "Automap.c"

/*---------------------------------------------------------------*/
/* phpinfo() output                                              */

static PHP_MINFO_FUNCTION(automap)
{
	php_info_print_table_start();

	php_info_print_table_row(2, "Automap", "enabled");
	php_info_print_table_row(2, "Version", AUTOMAP_EXT_VERSION);

	php_info_print_table_end();
}

/*---------------------------------------------------------------*/
/* Initialize a new zend_automap_globals struct during thread spin-up */

static void automap_globals_ctor(zend_automap_globals * globals TSRMLS_DC)
{
	memset(globals, 0, sizeof(*globals)); /* Init everything to 0/NULL */
}

/*------------------------*/
/* Any resources allocated during initialization may be freed here */

#ifndef ZTS
static void automap_globals_dtor(zend_automap_globals * globals TSRMLS_DC)
{
}
#endif

/*---------------------------------------------------------------*/

static void build_constant_values()
{
	/* Constant zvalues */

	INIT_ZVAL(CZVAL(false));
	ZVAL_BOOL(&CZVAL(false), 0);

	INIT_ZVAL(CZVAL(true));
	ZVAL_BOOL(&CZVAL(true), 1);

	INIT_ZVAL(CZVAL(null));
	ZVAL_NULL(&CZVAL(null));

	INIT_CZVAL(Automap);
	INIT_CZVAL(spl_autoload_register);

	/* Hash keys */

	INIT_HKEY(map);
	INIT_HKEY(options);
	INIT_HKEY(automap);

	INIT_HKEY_VALUE(mp_property_name,MP_PROPERTY_NAME);
}

/*---------------------------------------------------------------*/

static PHP_RINIT_FUNCTION(automap)
{
	if (!init_done) return SUCCESS;

	DBG_INIT();

	if (RINIT_utils(TSRMLS_C) == FAILURE) return FAILURE;

	if (RINIT_Automap(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_RSHUTDOWN_FUNCTION(automap)
{
	if (!init_done) return SUCCESS;

	if (RSHUTDOWN_Automap(TSRMLS_C) == FAILURE) return FAILURE;

	if (RSHUTDOWN_utils(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_MINIT_FUNCTION(automap)
{
	build_constant_values();

	/* Handle case where the Automap extension is dynamically loaded after
	   the Automap PHP runtime has been initialized. In this case,
	   we must not define anything/ */

	if (EG(class_table) && HKEY_EXISTS(EG(class_table),automap)) return SUCCESS;
	else init_done=1;

	ZEND_INIT_MODULE_GLOBALS(automap, automap_globals_ctor, NULL);

	REGISTER_STRING_CONSTANT("AUTOMAP_EXT_VERSION", AUTOMAP_EXT_VERSION,
							 CONST_CS | CONST_PERSISTENT);

	if (MINIT_utils(TSRMLS_C) == FAILURE) return FAILURE;

	if (MINIT_Automap(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_MSHUTDOWN_FUNCTION(automap)
{
	if (!init_done) return SUCCESS;

#ifndef ZTS
	automap_globals_dtor(&automap_globals TSRMLS_CC);
#endif

	if (MSHUTDOWN_Automap(TSRMLS_C) == FAILURE) return FAILURE;

	if (MSHUTDOWN_utils(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*-- Module definition --*/

zend_module_entry automap_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_AUTOMAP_EXTNAME,
	NULL,
	PHP_MINIT(automap),
	PHP_MSHUTDOWN(automap),
	PHP_RINIT(automap),
	PHP_RSHUTDOWN(automap),
	PHP_MINFO(automap),
#if ZEND_MODULE_API_NO >= 20010901
	AUTOMAP_EXT_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

/*---------------------------------------------------------------*/
