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

#define PHK_VERSION "1.4.0"

#ifdef HAVE_STDLIB_H
#	include <stdlib.h>
#endif

#include <stdio.h>

#ifdef HAVE_SYS_TYPES_H
#	include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif

#if HAVE_STRING_H
#	include <string.h>
#endif

#if HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifndef S_ISDIR
#	define S_ISDIR(mode)	(((mode)&S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#	define S_ISREG(mode)	(((mode)&S_IFMT) == S_IFREG)
#endif

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "php.h"
#include "php_version.h"
#include "ext/standard/php_versioning.h"
#include "ext/standard/url.h"
#include "zend_compile.h"
#include "php_ini.h"

#include "php_phk.h"
#include "utils.h"
#include "PHK.h"
#include "PHK_Mgr.h"
#include "PHK_Cache.h"

ZEND_EXTERN_MODULE_GLOBALS(phk)

/*---------------------------------------------------------------*/

#define PHK_GET_INSTANCE_DATA(_func) \
	zval **_tmp; \
	PHK_Mnt_Info *mp; \
	\
	CHECK_MEM(); \
	DBG_MSG("Entering PHK::" #_func); \
	FIND_HKEY(Z_OBJPROP_P(getThis()),m,&_tmp); \
	mp= *((PHK_Mnt_Info **)(Z_STRVAL_PP(_tmp)));

/*---------------------------------------------------------------*/

static zval mime_table;

typedef struct {
	char *suffix;
	char *mime_type;
} mime_entry;

static mime_entry mime_init[] = {
	{"", "text/plain"},
	{"gif", "image/gif"},
	{"jpeg", "image/jpeg"},
	{"jpg", "image/jpeg"},
	{"png", "image/png"},
	{"psd", "image/psd"},
	{"bmp", "image/bmp"},
	{"tif", "image/tiff"},
	{"tiff", "image/tiff"},
	{"iff", "image/iff"},
	{"wbmp", "image/vnd.wap.wbmp"},
	{"ico", "image/x-icon"},
	{"xbm", "image/xbm"},
	{"txt", "text/plain"},
	{"htm", "text/html"},
	{"html", "text/html"},
	{"css", "text/css"},
	{"php", "application/x-httpd-php"},
	{"phk", "application/x-httpd-php"},
	{"pdf", "application/pdf"},
	{"js", "application/x-javascript"},
	{"swf", "application/x-shockwave-flash"},
	{"xml", "application/xml"},
	{"xsl", "application/xml"},
	{"xslt", "application/xslt+xml"},
	{"mp3", "audio/mpeg"},
	{"ram", "audio/x-pn-realaudio"},
	{"svg", "image/svg+xml"},
	{NULL, NULL}
};

static char *autoindex_fnames[] = {
	"index.htm",
	"index.html",
	"index.php",
	NULL
};

/*---------------------------------------------------------------*/

static int web_access_matches(zval * apath, zval * path TSRMLS_DC);
static int web_access_allowed(PHK_Mnt_Info * mp, zval * path TSRMLS_DC);
static char *goto_main(PHK_Mnt_Info * mp TSRMLS_DC);
static char *web_tunnel(PHK_Mnt_Info * mp, zval * path,
						int webinfo TSRMLS_DC);
static void init_mime_table(TSRMLS_D);
static void shutdown_mime_table(TSRMLS_D);
static void set_constants(zend_class_entry * ce);

/*---------------------------------------------------------------*/

static void PHK_set_mp_property(zval * obj, PHK_Mnt_Info * mp TSRMLS_DC)
{
	add_property_stringl_ex(obj, "m", 2, (char *) (&mp), sizeof(mp),
							1 TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* Slow path */

static void PHK_need_php_runtime(TSRMLS_D)
{
	FILE *fp;
	int size, offset;
	char buf1[241], *buf;

	if (PHK_G(php_runtime_is_loaded))
		return;

	if (HKEY_EXISTS(EG(class_table), phk_stream_backend)) {
		PHK_G(php_runtime_is_loaded) = 1;
		return;
	}

	if (!PHK_G(root_package)[0])
		EXCEPTION_ABORT
			("Internal error - Cannot load PHP runtime code because root_package is not set");

	DBG_MSG1("Loading PHP runtime code from %s", PHK_G(root_package));

	if (!(fp = fopen(PHK_G(root_package), "rb")))
		EXCEPTION_ABORT_1
			("Cannot load PHP runtime code - Unable to open file %s",
			 PHK_G(root_package));

	(void) fread(buf1, 241, 1, fp);
	sscanf(buf1 + 212, "%d", &offset);
	sscanf(buf1 + 227, "%d", &size);

	buf = emalloc(size + 1);
	fseek(fp, offset, SEEK_SET);
	fread(buf, size, 1, fp);
	fclose(fp);
	buf[size] = '\0';

	zend_eval_string(buf, NULL, "PHK php runtime" TSRMLS_CC);

	efree(buf);

	PHK_G(php_runtime_is_loaded) = 1;
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, need_php_runtime)
{
	PHK_need_php_runtime(TSRMLS_C);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, __construct)
{
	PHK_Mnt_Info **mpp;
	int dummy_len;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "s", &mpp, &dummy_len) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_set_mp_property(getThis(), *mpp TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void PHK_init(PHK_Mnt_Info * mp TSRMLS_DC)
{
	zval *args[2];

	PHK_supports_php_version(mp TSRMLS_CC);
	if (EG(exception))
		return;

	if ((mp->crc_check) || (Z_LVAL_P(mp->flags) & PHK_F_CRC_CHECK)) {
		PHK_crc_check(mp TSRMLS_CC);
		if (EG(exception))
			return;
	}

	if (mp->required_extensions) {
		phk_require_extensions(mp->required_extensions TSRMLS_CC);
		if (EG(exception))
			return;
	}

	if (mp->autoload_uri) {
		args[0] = mp->autoload_uri;
		args[1] = mp->base_uri;
		if (!HKEY_EXISTS(EG(class_table), autoload))
			PHK_need_php_runtime(TSRMLS_C);
		ut_call_user_function_void(&CZVAL(Autoload), &CZVAL(load), 2,
								   args TSRMLS_CC);
		if (EG(exception))
			return;
	}

	if (mp->mount_script_uri
		&& (!(Z_LVAL_P(mp->flags) & PHK_F_NO_MOUNT_SCRIPT))) {
		ut_require(Z_STRVAL_P(mp->mount_script_uri), NULL TSRMLS_CC);
		if (EG(exception))
			return;
	}

	if (mp->plugin_class) {
		ut_new_instance(&(mp->plugin), mp->plugin_class, 1, 1,
						&(mp->mnt) TSRMLS_CC);
		if (EG(exception))
			return;
	}
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, map_defined)
{
	PHK_GET_INSTANCE_DATA(map_defined)

	RETVAL_BOOL((mp->autoload_uri != NULL));
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, mtime)
{
	PHK_GET_INSTANCE_DATA(mtime)

	RETVAL_BY_REF(mp->mtime);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, set_cache)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(set_cache)

		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
			FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	SEPARATE_ARG_IF_REF(zp);
	mp->caching = zp;
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, file_is_package)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_need_php_runtime(TSRMLS_C);
	RETVAL_BOOL(ut_call_user_function_bool(&CZVAL(PHK_Proxy)
										   , &CZVAL(file_is_package), 1,
										   &zp TSRMLS_CC));
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, data_is_package)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_need_php_runtime(TSRMLS_C);
	RETVAL_BOOL(ut_call_user_function_bool(&CZVAL(PHK_Proxy)
										   , &CZVAL(data_is_package), 1,
										   &zp TSRMLS_CC));
}

/*---------------------------------------------------------------*/

static int PHK_cache_enabled(PHK_Mnt_Info * mp, zval * command,
							 zval * params, zval * path TSRMLS_DC)
{
	if (mp->no_cache || (Z_LVAL_P(mp->flags) & PHK_F_CREATOR)
		|| (!PHK_Cache_cache_present(TSRMLS_C)))
		return 0;

	if (Z_TYPE_P(mp->caching) != IS_NULL)
		return Z_BVAL_P(mp->caching);

	return 1;
}

/*---------------------------------------------------------------*/

static void PHK_umount(PHK_Mnt_Info * mp TSRMLS_DC)
{
	if (mp->plugin) {
		zval_ptr_dtor(&(mp->plugin));
		mp->plugin = NULL;
	}

	if (mp->umount_script_uri
		&& (!(Z_LVAL_P(mp->flags) & PHK_F_NO_MOUNT_SCRIPT))) {
		ut_require(Z_STRVAL_P(mp->umount_script_uri), NULL TSRMLS_CC);
		/* Don't catch exception here. Unload map first */
	}

	if (mp->autoload_uri) {
		/* No need to load the PHP runtime, the Autoload class is defined */
		ut_call_user_function_void(&CZVAL(Autoload), &CZVAL(unload), 1,
								   &(mp->autoload_uri) TSRMLS_CC);
	}
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, mnt)
{
	PHK_GET_INSTANCE_DATA(mnt)

	RETVAL_BY_REF(mp->mnt);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, flags)
{
	PHK_GET_INSTANCE_DATA(flags)

	RETVAL_BY_REF(mp->flags);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, path)
{
	PHK_GET_INSTANCE_DATA(path)

	RETVAL_BY_REF(mp->path);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, uri)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(uri)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
			FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_uri(mp->mnt, zp, return_value TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, section_uri)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(section_uri)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
			FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_section_uri(mp->mnt, zp, return_value TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, command_uri)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(command_uri)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
			FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_command_uri(mp->mnt, zp, return_value TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, base_uri)
{
	PHK_GET_INSTANCE_DATA(base_uri)

	RETVAL_BY_REF(mp->base_uri);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, autoload_uri)
{
	PHK_GET_INSTANCE_DATA(autoload_uri)

	RETVAL_BY_REF(mp->autoload_uri);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, option)
{
	zval *zp, **zpp;

	PHK_GET_INSTANCE_DATA(option)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	DBG_MSG1("Entering PHK::option(%s)", Z_STRVAL_P(zp));

	if ((Z_TYPE_P(zp) == IS_STRING)
		&& zend_hash_find(Z_ARRVAL_P(mp->options)
						  , Z_STRVAL_P(zp), Z_STRLEN_P(zp) + 1,
						  (void **) &zpp) == SUCCESS) {
		RETVAL_BY_REF(*zpp);
	}
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, options)
{
	PHK_GET_INSTANCE_DATA(options)

	RETVAL_BY_REF(mp->options);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, parent_mnt)
{
	PHK_GET_INSTANCE_DATA(parent_mnt)

	if (mp->parent) RETVAL_BY_REF(mp->parent->mnt);
}

/*---------------------------------------------------------------*/

static int web_access_matches(zval * apath, zval * path TSRMLS_DC)
{
	int alen, plen;

	alen = Z_STRLEN_P(apath);
	plen = Z_STRLEN_P(path);

	if ((alen == 1) && (Z_STRVAL_P(apath)[0] == '/'))
		return 1;

	return ((plen >= alen)
			&& ((alen == plen) || (Z_STRVAL_P(path)[alen] == '/'))
			&& (!memcmp(Z_STRVAL_P(path), Z_STRVAL_P(apath), alen)));
}

/*---------------------------------------------------------------*/
/* mp->web_access can only be array|string (type is filtered and forced
  to string if not array when computing persistent data in PHK_Mgr) */

static int web_access_allowed(PHK_Mnt_Info * mp, zval * path TSRMLS_DC)
{
	HashPosition pos;
	zval **apath_pp;
	HashTable *ht;

	if (mp->web_access) {
		if (Z_TYPE_P(mp->web_access) == IS_ARRAY) {
			ht = Z_ARRVAL_P(mp->web_access);
			zend_hash_internal_pointer_reset_ex(ht, &pos);
			while (zend_hash_get_current_data_ex
				   (ht, (void **) (&apath_pp), &pos) == SUCCESS) {
				if (Z_TYPE_PP(apath_pp) != IS_STRING)
					continue;
				if (web_access_matches(*apath_pp, path TSRMLS_CC))
					return 1;
				zend_hash_move_forward_ex(ht, &pos);
			}
		} else {
			return web_access_matches(mp->web_access, path TSRMLS_CC);
		}
	}
	return 0;
}

/*---------------------------------------------------------------*/

static char *goto_main(PHK_Mnt_Info * mp TSRMLS_DC)
{
	char *p;
	zval zv;

	if (mp->web_main_redirect) {
		ut_http_301_redirect(mp->web_run_script, 0 TSRMLS_CC);
	} else {
		PHK_Mgr_uri(mp->mnt, mp->web_run_script, &zv TSRMLS_CC);
		spprintf(&p, 1024, "require('%s');", Z_STRVAL(zv));
		zval_dtor(&zv);
	}
	return p;
}

/*---------------------------------------------------------------*/
/* Returns an allocated value */

#define CLEANUP_WEB_TUNNEL() \
	{ \
	zval_dtor(&tpath); \
	zval_dtor(&uri); \
	}

static char *web_tunnel(PHK_Mnt_Info * mp, zval * path,
						int webinfo TSRMLS_DC)
{
	int last_slash, found;
	zval tpath, uri, *zp;
	php_stream_statbuf ssb, ssb2;
	char **pp, *p;

	if (!path)
		PHK_get_subpath(&tpath TSRMLS_CC);
	else {
		tpath = *path;
		zval_copy_ctor(&tpath);
		convert_to_string(&tpath);
	}

	INIT_ZVAL(uri);

	last_slash = (Z_STRVAL(tpath)[Z_STRLEN(tpath) - 1] == '/');

	if (last_slash && (Z_STRLEN(tpath) != 1))
		ut_rtrim_zval(&tpath TSRMLS_CC);

	if (!Z_STRLEN(tpath)) {
		CLEANUP_WEB_TUNNEL();
		if (mp->web_run_script)
			return goto_main(mp TSRMLS_CC);
		else
			ut_http_301_redirect(&CZVAL(slash), 0 TSRMLS_CC);
	}

	if ((!webinfo) && (!web_access_allowed(mp, &tpath TSRMLS_CC))
		&& (!ut_strings_are_equal(&tpath, mp->web_run_script TSRMLS_CC))) {
		CLEANUP_WEB_TUNNEL();
		if (mp->web_run_script)
			return goto_main(mp TSRMLS_CC);
		else
			ut_http_403_fail(TSRMLS_C);
	}

	PHK_Mgr_uri(mp->mnt, &tpath, &uri TSRMLS_CC);

	if (php_stream_stat_path(Z_STRVAL(uri), &ssb) != 0) {
		CLEANUP_WEB_TUNNEL();
		ut_http_404_fail(TSRMLS_C);
	}

	if (S_ISDIR(ssb.sb.st_mode))	// Directory autoindex
	{
		found = 0;
		if (last_slash) {
			for (pp = autoindex_fnames; *pp; p++) {
				spprintf(&p, UT_PATH_MAX, Z_STRVAL(uri), "%s%s", *pp);
				if ((php_stream_stat_path(p, &ssb2) == 0)
					&& S_ISREG(ssb2.sb.st_mode)) {
					found = 1;
					zval_dtor(&tpath);
					ZVAL_STRING((&tpath), p, 0);
					zval_dtor(&uri);
					PHK_Mgr_uri(mp->mnt, &tpath, &uri TSRMLS_CC);
					break;
				}
				efree(p);
			}
			if (!found) {
				CLEANUP_WEB_TUNNEL();
				ut_http_404_fail(TSRMLS_C);
			}
		} else {
			MAKE_STD_ZVAL(zp);
			spprintf(&p, UT_PATH_MAX, "%s/", Z_STRVAL(uri));
			ZVAL_STRING(zp, p, 0);
			CLEANUP_WEB_TUNNEL();
			ut_http_301_redirect(zp, 1 TSRMLS_CC);
		}
	}

/* Here, the path is in tpath and the corresponding uri is in uri */

	if ((!webinfo) && (PHK_is_php_source_path(mp, &tpath TSRMLS_CC))) {
		spprintf(&p, UT_PATH_MAX, "require('%s');", Z_STRVAL(tpath));
	} else {
		spprintf(&p, UT_PATH_MAX,
				 "PHK_Mgr::mime_header('%s',%lu,'%s'); readfile('%s');",
				 Z_STRVAL_P(mp->mnt), mp->hash, Z_STRVAL(tpath),
				 Z_STRVAL(uri));
	}

	CLEANUP_WEB_TUNNEL();
	return p;
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, web_tunnel)
{
	zval *path;
	int webinfo;

	PHK_GET_INSTANCE_DATA(web_tunnel)

		path = NULL;
	webinfo = 0;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "|zb", &path, &webinfo) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	RETVAL_STRING(web_tunnel(mp, path, webinfo TSRMLS_CC), 0);
}

/*---------------------------------------------------------------*/

static void PHK_mime_header(PHK_Mnt_Info * mp, zval * path TSRMLS_DC)
{
	zval *type;
	char *p;

	if (!(type = PHK_mime_type(mp, path TSRMLS_CC)))
		return;

	spprintf(&p, UT_PATH_MAX, "Content-type: %s", Z_STRVAL_P(type));
	ut_header(200, p TSRMLS_CC);
	efree(p);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, mime_header)
{
	zval *path;

	PHK_GET_INSTANCE_DATA(mime_header)

		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
			FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_mime_header(mp, path TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static zval *PHK_mime_type(PHK_Mnt_Info * mp, zval * path TSRMLS_DC)
{
	zval suffix, *zp, **zpp;

	ut_file_suffix(path, &suffix TSRMLS_CC);

	if (mp->mime_types && zend_hash_find(Z_ARRVAL_P(mp->mime_types)
										 , Z_STRVAL(suffix),
										 Z_STRLEN(suffix) + 1,
										 (void **) (&zpp)) == SUCCESS) {
		zp = *zpp;
	} else {
		if (zend_hash_find
			(Z_ARRVAL(mime_table), Z_STRVAL(suffix), Z_STRLEN(suffix) + 1,
			 (void **) (&zpp)) == SUCCESS) {
			zval_dtor(&suffix);
			return *zpp;
		} else {
			zp = ((strstr(Z_STRVAL(suffix), "php")) ?
				  (&CZVAL(application_x_httpd_php)) : NULL);
		}
	}

	zval_dtor(&suffix);
	return zp;
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, mime_type)
{
	zval *path, *zp;

	PHK_GET_INSTANCE_DATA(mime_type)

		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
			FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	zp = PHK_mime_type(mp, path TSRMLS_CC);
	if (zp)
		RETVAL_BY_REF(zp);
}

/*---------------------------------------------------------------*/

static int PHK_is_php_source_path(PHK_Mnt_Info * mp, zval * path TSRMLS_DC)
{
	return (PHK_mime_type(mp, path TSRMLS_CC) ==
			(&CZVAL(application_x_httpd_php)));
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, is_php_source_path)
{
	zval *path;

	PHK_GET_INSTANCE_DATA(is_php_source_path)

		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
			FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	RETVAL_BOOL(PHK_is_php_source_path(mp, path TSRMLS_CC));
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, proxy)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(proxy)

		zp = PHK_Mgr_proxy_by_mp(mp TSRMLS_CC);

	RETVAL_BY_REF(zp);
}

/*---------------------------------------------------------------*/

static void PHK_crc_check(PHK_Mnt_Info * mp TSRMLS_DC)
{
	PHK_need_php_runtime(TSRMLS_C);
	ut_call_user_function_void(PHK_Mgr_proxy_by_mp(mp TSRMLS_CC),
							   &CZVAL(crc_check), 0, NULL TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void PHK_supports_php_version(PHK_Mnt_Info * mp TSRMLS_DC)
{
	if (mp->min_php_version
		&& php_version_compare(PHP_VERSION,
							   Z_STRVAL_P(mp->min_php_version)) < 0) {
		EXCEPTION_ABORT_1("PHP minimum supported version: ",
						  Z_STRVAL_P(mp->min_php_version));
	}

	if (mp->max_php_version
		&& php_version_compare(PHP_VERSION,
							   Z_STRVAL_P(mp->max_php_version)) > 0) {
		EXCEPTION_ABORT_1("PHP maximum supported version: ",
						  Z_STRVAL_P(mp->max_php_version));
	}
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, plugin)
{
	PHK_GET_INSTANCE_DATA(plugin)

		if (mp->plugin) {
		RETVAL_BY_REF(mp->plugin);
	}
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, accelerator_is_present)
{
	RETVAL_TRUE;
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, build_info)
{
	zval *zp, **zpp;

	PHK_GET_INSTANCE_DATA(build_info)

		zp = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z!", &zp) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	if (!zp) {
		RETVAL_BY_REF(mp->build_info);
		return;
	}

	if ((Z_TYPE_P(zp) == IS_STRING)
		&& zend_hash_find(Z_ARRVAL_P(mp->build_info)
						  , Z_STRVAL_P(zp), Z_STRLEN_P(zp) + 1,
						  (void **) &zpp) == SUCCESS) {
		RETVAL_BY_REF(*zpp);
	} else {
		EXCEPTION_ABORT_1("%s: unknown build info", Z_STRVAL_P(zp));
	}
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, subpath_url)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_need_php_runtime(TSRMLS_C);
	ut_call_user_function_string(&CZVAL(PHK_Backend), &CZVAL(subpath_url)
								 , return_value, 1, &zp TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void PHK_get_subpath(zval * ret TSRMLS_DC)
{
	zval *zp;
	char *p;
	int len, decode, s1;

	zp = REQUEST_ELEMENT(_PHK_path);
	if (zp)
		decode = 1;
	else {
		decode = 0;
		zp = SERVER_ELEMENT(PATH_INFO);
		if (!zp)
			zp = SERVER_ELEMENT(ORIG_PATH_INFO);
		if (!zp) {
			ZVAL_STRINGL(ret, "", 0, 1);
			return;
		}
	}

	if (Z_TYPE_P(zp) != IS_STRING)
		convert_to_string(zp);

	s1 = Z_STRVAL_P(zp)[0] == '/' ? 0 : 1;
	p = emalloc(Z_STRLEN_P(zp) + s1 + 1);

	if (s1)
		(*p) = '/';
	memmove(p + s1, Z_STRVAL_P(zp), Z_STRLEN_P(zp) + 1);

	len = php_url_decode(p, Z_STRLEN_P(zp) + s1);
	ZVAL_STRINGL(ret, p, len, 0);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, get_subpath)
{
	PHK_get_subpath(return_value TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static zval *PHK_backend(PHK_Mnt_Info * mp, zval * phk TSRMLS_DC)
{
	if (!mp->backend) {
		ut_new_instance(&(mp->backend), &CZVAL(PHK_Backend), 1, 1,
						&phk TSRMLS_CC);
	}

	return mp->backend;
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, __call)
{
	zval *method, *call_args, *args[3];

	PHK_GET_INSTANCE_DATA(__call)

	if (zend_parse_parameters
			(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &method,
			 &call_args) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	DBG_MSG1("Method: %s", Z_STRVAL_P(method));

	PHK_need_php_runtime(TSRMLS_C);

	args[0] = PHK_backend(mp, getThis()TSRMLS_CC);
	args[1] = method;
	args[2] = call_args;
	ut_call_user_function(&CZVAL(PHK_Util), &CZVAL(call_method)
						  , return_value, 3, args TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static PHP_METHOD(PHK, prolog)
{
	int cli, is_main, status;
	PHK_Mnt_Info *mp;
	zval *file, *cmd, *ret, **argv1_pp, *instance;
	char *p;
	struct stat dummy_sb;

	DBG_MSG("Entering Prolog");

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zzz", &file, &cmd, &ret) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	if (!PHK_G(root_package)[0]) {	/* Store path to load PHP code later, if needed */
		if (Z_STRLEN_P(file) > UT_PATH_MAX)
			EXCEPTION_ABORT_1("Path too long - max size=%d", UT_PATH_MAX);

		memmove(PHK_G(root_package), Z_STRVAL_P(file),
				Z_STRLEN_P(file) + 1);
	}

	cli = (!ut_is_web());

	if (cli) {
		(void) zend_alter_ini_entry("display_errors",
									sizeof("display_errors"), "1", 1,
									PHP_INI_USER, PHP_INI_STAGE_RUNTIME);
		if (!PG(safe_mode))
			(void) zend_alter_ini_entry("memory_limit",
										sizeof("memory_limit"), "1024M", 5,
										PHP_INI_USER,
										PHP_INI_STAGE_RUNTIME);
	}

	PHK_Mgr_php_version_check(TSRMLS_C);

	mp = PHK_Mgr_mount(file, 0 TSRMLS_CC);

	DBG_MSG1("Mounted on %s", Z_STRVAL_P(mp->mnt));

	if (EG(exception))
		return;					/* TODO: message */

	is_main = (EG(current_execute_data)->prev_execute_data == NULL);

	DBG_MSG1("Is main: %d", is_main);

	if (!is_main) {
		if (mp->lib_run_script_uri) {
			ut_require(Z_STRVAL_P(mp->lib_run_script_uri), NULL TSRMLS_CC);
		}
		if (mp->auto_umount) {
			PHK_Mgr_umount_mnt_info(mp TSRMLS_CC);
		} else {
			ZVAL_STRINGL(ret, Z_STRVAL_P(mp->mnt), Z_STRLEN_P(mp->mnt), 1);
		}
		return;
	}

	/* Main script - Dispatch */

	if (cli) {
		if (Z_LVAL_P(SERVER_ELEMENT(argc)) > 1) {
			zend_hash_index_find(Z_ARRVAL_P(SERVER_ELEMENT(argv)),
								 (ulong) 1, (void **) &argv1_pp);
			if (Z_STRVAL_PP(argv1_pp)[0] == '@') {
				PHK_need_php_runtime(TSRMLS_C);
				instance = PHK_Mgr_instance_by_mp(mp TSRMLS_CC);
				ZVAL_LONG(ret,
						  ut_call_user_function_long(instance,
													 &CZVAL
													 (builtin_prolog), 1,
													 &file TSRMLS_CC));
				return;
			}
		}

		if (mp->cli_run_command) {
			ZVAL_STRINGL(cmd, Z_STRVAL_P(mp->cli_run_command)
						 , Z_STRLEN_P(mp->cli_run_command), 1);
		}
		return;
	}

	/* HTTP mode */

	spprintf(&p, UT_PATH_MAX, "%s.webinfo", Z_STRVAL_P(file));
	status = stat(p, &dummy_sb);
	efree(p);
	if (!status) {
		PHK_need_php_runtime(TSRMLS_C);
		instance = PHK_Mgr_instance_by_mp(mp TSRMLS_CC);
		ut_call_user_function_void(&CZVAL(PHK_Util), &CZVAL(run_webinfo)
								   , 1, &instance TSRMLS_CC);
		return;
	}

	ZVAL_STRING(cmd, web_tunnel(mp, NULL, 0 TSRMLS_CC), 0);
}

/*---------------------------------------------------------------*/

static ZEND_BEGIN_ARG_INFO_EX(PHK_prolog_arginfo, 0, 0, 3)
ZEND_ARG_INFO(0, file)
ZEND_ARG_INFO(1, cmd)
ZEND_ARG_INFO(1, ret)
ZEND_END_ARG_INFO()

/*---------------------------------------------------------------*/
 static zend_function_entry PHK_functions[] = {
	PHP_ME(PHK, need_php_runtime, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, __construct, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, map_defined, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, mtime, UT_1arg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, set_cache, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, file_is_package, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, data_is_package, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, mnt, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, flags, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, path, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, uri, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, section_uri, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, command_uri, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, base_uri, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, autoload_uri, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, option, UT_1arg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, options, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, parent_mnt, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, web_tunnel, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, mime_header, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, mime_type, UT_1arg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, is_php_source_path, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, proxy, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, plugin, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, accelerator_is_present, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, build_info, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, subpath_url, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, get_subpath, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, __call, UT_2args_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, prolog, PHK_prolog_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/

static void init_mime_table(TSRMLS_D)
{
	mime_entry *mip;
	zval tmp;

	INIT_ZVAL(tmp);
	array_init(&tmp);

	for (mip = mime_init; mip->suffix; mip++) {
		add_assoc_string(&tmp, mip->suffix, mip->mime_type, 1);
	}

	ut_persist_zval(&tmp, &mime_table);
	zval_dtor(&tmp);
}

/*---------------------------------------------------------------*/

static void shutdown_mime_table(TSRMLS_D)
{
	ut_persistent_zval_dtor(&mime_table);
}

/*---------------------------------------------------------------*/

static void set_constants(zend_class_entry * ce)
{
	zval *zp;
	char *p;
	int len;

#define PHK_NEW_ZP() zp=pemalloc(sizeof(zval),1); INIT_PZVAL(zp);

#define PHK_ADD_ZP_CONST(name) \
	zend_hash_add(&ce->constants_table,name,sizeof(name) \
		,(void *)(&zp),sizeof(zp),NULL)

	PHK_NEW_ZP();
	len = sizeof(PHK_VERSION);
	p = pemalloc(len, 1);
	memmove(p, PHK_VERSION, len);
	ZVAL_STRINGL(zp, p, len, 0);
	PHK_ADD_ZP_CONST("VERSION");

	PHK_NEW_ZP();
	ZVAL_LONG(zp, PHK_F_CRC_CHECK);
	PHK_ADD_ZP_CONST("F_CRC_CHECK");

	PHK_NEW_ZP();
	ZVAL_LONG(zp, PHK_F_NO_MOUNT_SCRIPT);
	PHK_ADD_ZP_CONST("F_NO_MOUNT_SCRIPT");

	PHK_NEW_ZP();
	ZVAL_LONG(zp, PHK_F_CREATOR);
	PHK_ADD_ZP_CONST("F_CREATOR");
}

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_PHK(TSRMLS_D)
{
	zend_class_entry ce, *entry;

	/*--- Init class and set constants*/

	INIT_CLASS_ENTRY(ce, "PHK", PHK_functions);
	entry = zend_register_internal_class(&ce TSRMLS_CC);
	set_constants(entry);

	/*--- Init mime table */

	init_mime_table(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Module shutdown                                               */

static int MSHUTDOWN_PHK(TSRMLS_D)
{
	/*--- Free mime table */

	shutdown_mime_table(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_PHK(TSRMLS_D)
{
	PHK_G(root_package)[0] = '\0';

	PHK_G(php_runtime_is_loaded) = 0;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_PHK(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/
