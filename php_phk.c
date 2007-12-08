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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "SAPI.h"
#include "TSRM/TSRM.h"
#include "ext/standard/info.h"

#define ALLOCATE

#include "php_phk.h"
#include "utils.h"
#include "phk_global.h"
#include "PHK_Cache.h"
#include "PHK_Stream.h"
#include "PHK_Mgr.h"
#include "PHK.h"
#include "phk_stream_parse_uri2.h"

/*------------------------*/

ZEND_DECLARE_MODULE_GLOBALS(phk)

/*------------------------*/

#ifdef COMPILE_DL_PHK
ZEND_GET_MODULE(phk)
#endif

/*------------------------*/

#include "utils.c"

#include "phk_global.c"

#include "PHK_Cache.c"

#include "PHK_Stream.c"

#include "PHK_Mgr.c"

#include "PHK.c"

#include "phk_stream_parse_uri2.c"

/*---------------------------------------------------------------*/
/* phpinfo() output                                              */

static PHP_MINFO_FUNCTION(phk)
{
php_info_print_table_start();

php_info_print_table_row(2,"PHK Accelerator", "enabled");
php_info_print_table_row(2, "Version", PHK_ACCEL_VERSION);
php_info_print_table_row(2, "Cache used",PHK_Cache_cache_name(TSRMLS_C));

php_info_print_table_end();
}

/*---------------------------------------------------------------*/

static PHP_FUNCTION(_phk_techinfo)
{
if (sapi_module.phpinfo_as_text)
	{
	php_printf("Using PHK Accelerator: Yes\n");
	php_printf("Accelerator Version: %s\n",PHK_ACCEL_VERSION);
	}
else
	{
	php_printf("<table border=0>");
	php_printf("<tr><td>Using PHK Accelerator:&nbsp;</td><td>Yes</td></tr>");
	php_printf("<tr><td>Accelerator Version:&nbsp;</td><td>%s</td></tr>",PHK_ACCEL_VERSION);
	php_printf("</table>");
	}
}

/*---------------------------------------------------------------*/
/* Initialize a new zend_phk_globals struct during thread spin-up */

static void phk_globals_ctor(zend_phk_globals *globals TSRMLS_DC)
{
globals->init_done=0;
}

/*------------------------*/
/* Any resources allocated during initialization may be freed here */

#ifndef ZTS
static void phk_globals_dtor(zend_phk_globals *globals TSRMLS_DC)
{
}
#endif

/*---------------------------------------------------------------*/

static void build_constant_values()
{
/* Constant zvalues */

INIT_ZVAL(CZVAL(false));
ZVAL_BOOL(&CZVAL(false),0);

INIT_ZVAL(CZVAL(true));
ZVAL_BOOL(&CZVAL(true),1);

INIT_ZVAL(CZVAL(null));
ZVAL_NULL(&CZVAL(null));

INIT_ZVAL(CZVAL(slash));
ZVAL_STRING(&CZVAL(slash),"/",0);

INIT_ZVAL(CZVAL(application_x_httpd_php));
ZVAL_STRING(&CZVAL(application_x_httpd_php),"application/x-httpd-php",0);

INIT_CZVAL(PHK_Stream_Backend);
INIT_CZVAL(PHK_Backend);
INIT_CZVAL(PHK_Util);
INIT_CZVAL(PHK_Proxy);
INIT_CZVAL(PHK_Creator);
INIT_CZVAL(PHK);
INIT_CZVAL(Autoload);

INIT_CZVAL(cache_enabled);
INIT_CZVAL(umount);
INIT_CZVAL(is_mounted);
INIT_CZVAL(get_file_data);
INIT_CZVAL(get_dir_data);
INIT_CZVAL(get_stat_data);
INIT_CZVAL(get_min_version);
INIT_CZVAL(get_options);
INIT_CZVAL(get_build_info);
INIT_CZVAL(init);
INIT_CZVAL(load);
INIT_CZVAL(crc_check);
INIT_CZVAL(file_is_package);
INIT_CZVAL(data_is_package);
INIT_CZVAL(unload);
INIT_CZVAL(subpath_url);
INIT_CZVAL(call_method);
INIT_CZVAL(run_webinfo);
INIT_CZVAL(require_extensions);
INIT_CZVAL(builtin_prolog);

/* Hash keys */

INIT_HKEY(crc_check);
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
INIT_HKEY(m);
INIT_HKEY(web_main_redirect);
INIT_HKEY(_PHK_path);
INIT_HKEY(ORIG_PATH_INFO);
INIT_HKEY(phk_backend);
INIT_HKEY(lib_run_script);
INIT_HKEY(cli_run_script);
INIT_HKEY(auto_umount);
INIT_HKEY(argc);
INIT_HKEY(argv);
INIT_HKEY(autoload);
INIT_HKEY(phk_stream_backend);
}

/*---------------------------------------------------------------*/

static PHP_RINIT_FUNCTION(phk)
{
if (RINIT_utils(TSRMLS_C)==FAILURE) return FAILURE;

if (RINIT_PHK_Cache(TSRMLS_C)==FAILURE) return FAILURE;

if (RINIT_PHK_Stream(TSRMLS_C)==FAILURE) return FAILURE;

if (RINIT_PHK_Mgr(TSRMLS_C)==FAILURE) return FAILURE;

if (RINIT_PHK(TSRMLS_C)==FAILURE) return FAILURE;

return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_RSHUTDOWN_FUNCTION(phk)
{
if (RSHUTDOWN_PHK(TSRMLS_C)==FAILURE) return FAILURE;

if (RSHUTDOWN_PHK_Mgr(TSRMLS_C)==FAILURE) return FAILURE;

if (RSHUTDOWN_PHK_Stream(TSRMLS_C)==FAILURE) return FAILURE;

if (RSHUTDOWN_PHK_Cache(TSRMLS_C)==FAILURE) return FAILURE;

if (RSHUTDOWN_utils(TSRMLS_C)==FAILURE) return FAILURE;

return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_MINIT_FUNCTION(phk)
{
ZEND_INIT_MODULE_GLOBALS(phk,phk_globals_ctor,NULL);

REGISTER_STRING_CONSTANT("PHK_ACCEL_VERSION",PHK_ACCEL_VERSION
	,CONST_CS|CONST_PERSISTENT);

build_constant_values();

if (MINIT_utils(TSRMLS_C)==FAILURE) return FAILURE;

if (MINIT_PHK_Cache(TSRMLS_C)==FAILURE) return FAILURE;

if (MINIT_PHK_Stream(TSRMLS_C)==FAILURE) return FAILURE;

if (MINIT_PHK_Mgr(TSRMLS_C)==FAILURE) return FAILURE;

if (MINIT_PHK(TSRMLS_C)==FAILURE) return FAILURE;

return SUCCESS;
}

/*---------------------------------------------------------------*/

static PHP_MSHUTDOWN_FUNCTION(phk)
{
#ifndef ZTS
	phk_globals_dtor(&phk_globals TSRMLS_CC);
#endif

if (MSHUTDOWN_PHK(TSRMLS_C)==FAILURE) return FAILURE;

if (MSHUTDOWN_PHK_Mgr(TSRMLS_C)==FAILURE) return FAILURE;

if (MSHUTDOWN_PHK_Stream(TSRMLS_C)==FAILURE) return FAILURE;

if (MSHUTDOWN_PHK_Cache(TSRMLS_C)==FAILURE) return FAILURE;

if (MSHUTDOWN_utils(TSRMLS_C)==FAILURE) return FAILURE;

return SUCCESS;
}

/*-- Function entries --*/

static ZEND_BEGIN_ARG_INFO_EX(phk_empty_arginfo,0,0,0)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(phk_stream_parse_uri2_arginfo,0,0,5)
ZEND_ARG_INFO(0,uri)
ZEND_ARG_INFO(1,command)
ZEND_ARG_INFO(1,params)
ZEND_ARG_INFO(1,mnt)
ZEND_ARG_INFO(1,path)
ZEND_END_ARG_INFO()

static function_entry phk_functions[] = {
    PHP_FE(_phk_techinfo, phk_empty_arginfo)
    PHP_FE(_phk_stream_parse_uri2, phk_stream_parse_uri2_arginfo)
    {NULL, NULL, NULL, 0, 0 }
};

/*-- Module definition --*/

zend_module_entry phk_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_PHK_EXTNAME,
    phk_functions,
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
