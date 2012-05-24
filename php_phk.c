/*
  +----------------------------------------------------------------------+
  | PHK accelerator extension <http://phk.tekwire.net>                   |
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

#define ALLOCATE

#include "php_phk.h"

/*------------------------*/

ZEND_DECLARE_MODULE_GLOBALS(phk)

#ifdef COMPILE_DL_PHK
	ZEND_GET_MODULE(phk)
#endif

static int init_done=0;

/*------------------------*/

#include "utils.c"
#include "PHK_Cache.c"
#include "PHK_Stream.c"
#include "PHK_Mgr.c"
#include "PHK.c"

/*---------------------------------------------------------------*/
/* phpinfo() output                                              */

static PHP_MINFO_FUNCTION(phk)
{

	php_info_print_table_start();

	php_info_print_table_row(2, "PHK Accelerator", "enabled");
	php_info_print_table_row(2, "Version", PHK_ACCEL_VERSION);
	php_info_print_table_row(2, "Cache used",PHK_Cache_cache_name(TSRMLS_C));
#ifdef PHK_DEBUG
	{
	char buf[10];

	sprintf(buf,"%d",zend_hash_num_elements(&persistent_mtab));
	php_info_print_table_row(2, "Persistent package count",buf);
	}
#endif

	php_info_print_table_end();
}

/*---------------------------------------------------------------*/
/* Initialize a new zend_phk_globals struct during thread spin-up */

static void phk_globals_ctor(zend_phk_globals * globals TSRMLS_DC)
{
	CLEAR_DATA(*globals); /* Init everything to 0/NULL */
}

/*------------------------*/
/* Any resources allocated during initialization may be freed here */

#ifndef ZTS
static void phk_globals_dtor(zend_phk_globals * globals TSRMLS_DC)
{
}
#endif

/*---------------------------------------------------------------*/

static void init_hkeys()
{
	INIT_HKEY(no_cache);
	INIT_HKEY(no_opcode_cache);
	INIT_HKEY(required_extensions);
	INIT_HKEY(map_defined);
	INIT_HKEY(mount_script);
	INIT_HKEY(umount_script);
	INIT_HKEY(plugin_class);
	INIT_HKEY(web_access);
	INIT_HKEY(min_php_version);
	INIT_HKEY(max_php_version);
	INIT_HKEY(mime_types);
	INIT_HKEY(web_run_script);
	INIT_HKEY_VALUE(mp_property_name,MP_PROPERTY_NAME);
	INIT_HKEY(web_main_redirect);
	INIT_HKEY(_PHK_path);
	INIT_HKEY(ORIG_PATH_INFO);
	INIT_HKEY(phk_backend);
	INIT_HKEY(lib_run_script);
	INIT_HKEY(cli_run_script);
	INIT_HKEY(auto_umount);
	INIT_HKEY(argc);
	INIT_HKEY(argv);
	INIT_HKEY(automap);
	INIT_HKEY(phk_stream_backend);
	INIT_HKEY(eaccelerator_get);
	INIT_HKEY(phk);
}

/*---------------------------------------------------------------*/

static PHP_RINIT_FUNCTION(phk)
{
	if (!init_done) return SUCCESS;

	DBG_INIT();

	if (RINIT_utils(TSRMLS_C) == FAILURE) return FAILURE;
	if (RINIT_PHK_Cache(TSRMLS_C) == FAILURE) return FAILURE;
	if (RINIT_PHK_Stream(TSRMLS_C) == FAILURE) return FAILURE;
	if (RINIT_PHK_Mgr(TSRMLS_C) == FAILURE) return FAILURE;
	if (RINIT_PHK(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_RSHUTDOWN_FUNCTION(phk)
{
	if (!init_done) return SUCCESS;

	if (RSHUTDOWN_PHK(TSRMLS_C) == FAILURE) return FAILURE;
	if (RSHUTDOWN_PHK_Mgr(TSRMLS_C) == FAILURE) return FAILURE;
	if (RSHUTDOWN_PHK_Stream(TSRMLS_C) == FAILURE) return FAILURE;
	if (RSHUTDOWN_PHK_Cache(TSRMLS_C) == FAILURE) return FAILURE;
	if (RSHUTDOWN_utils(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_MINIT_FUNCTION(phk)
{
	init_hkeys();

	/* Handle case where the PHK extension is dynamically loaded from a
	   PHK package (as when Automap.phk registers extensions). In this case,
	   we must not define anything/ */

	if (EG(class_table) && HKEY_EXISTS(EG(class_table),phk)) return SUCCESS;
	else init_done=1;

	ZEND_INIT_MODULE_GLOBALS(phk, phk_globals_ctor, NULL);

	REGISTER_STRING_CONSTANT("PHK_ACCEL_VERSION", PHK_ACCEL_VERSION,
							 CONST_CS | CONST_PERSISTENT);

	if (MINIT_utils(TSRMLS_C) == FAILURE) return FAILURE;
	if (MINIT_PHK_Cache(TSRMLS_C) == FAILURE) return FAILURE;
	if (MINIT_PHK_Stream(TSRMLS_C) == FAILURE) return FAILURE;
	if (MINIT_PHK_Mgr(TSRMLS_C) == FAILURE) return FAILURE;
	if (MINIT_PHK(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_MSHUTDOWN_FUNCTION(phk)
{
	if (!init_done) return SUCCESS;

#ifndef ZTS
	phk_globals_dtor(&phk_globals TSRMLS_CC);
#endif

	if (MSHUTDOWN_PHK(TSRMLS_C) == FAILURE) return FAILURE;
	if (MSHUTDOWN_PHK_Mgr(TSRMLS_C) == FAILURE) return FAILURE;
	if (MSHUTDOWN_PHK_Stream(TSRMLS_C) == FAILURE) return FAILURE;
	if (MSHUTDOWN_PHK_Cache(TSRMLS_C) == FAILURE) return FAILURE;
	if (MSHUTDOWN_utils(TSRMLS_C) == FAILURE) return FAILURE;

	return SUCCESS;
}

/*-- Module definition --*/

zend_module_entry phk_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_PHK_EXTNAME,
	NULL,
	PHP_MINIT(phk),
	PHP_MSHUTDOWN(phk),
	PHP_RINIT(phk),
	PHP_RSHUTDOWN(phk),
	PHP_MINFO(phk),
#if ZEND_MODULE_API_NO >= 20010901
	PHK_ACCEL_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

/*---------------------------------------------------------------*/
