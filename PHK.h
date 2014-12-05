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

#ifndef __PHK_H
#define __PHK_H
/*============================================================================*/

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

/*============================================================================*/

#define PHK_GET_INSTANCE_DATA(_func) \
	zval **_tmp; \
	int _order,_valid; \
	PHK_Mnt *mp; \
	\
	DBG_MSG("Entering PHK::" #_func); \
	CHECK_MEM(); \
	_valid=0; \
	if (FIND_HKEY(Z_OBJPROP_P(getThis()),mp_property_name,&_tmp)==SUCCESS) { \
		_order=Z_LVAL_PP(_tmp); \
		if (_order<PHK_G(mcount)) { \
			mp=PHK_G(mount_order)[_order]; \
			if (mp) _valid=1; \
		} \
	} \
	if (!_valid) { \
		EXCEPTION_ABORT("Accessing invalid or unmounted object"); \
	}

/*============================================================================*/

static void PHK_set_mp_property(zval * obj, int order TSRMLS_DC);
ZEND_DLEXPORT void PHK_need_php_runtime(TSRMLS_D);
static PHP_METHOD(PHK, need_php_runtime);
static void PHK_init(PHK_Mnt * mp TSRMLS_DC);
static PHP_METHOD(PHK, map_defined);
static PHP_METHOD(PHK, set_cache);
static PHP_METHOD(PHK, file_is_package);
static PHP_METHOD(PHK, data_is_package);
static int PHK_cache_enabled(PHK_Mnt * mp, zval * command,
							 zval * params, zval * path TSRMLS_DC);
static void PHK_umount(PHK_Mnt * mp TSRMLS_DC);
static PHP_METHOD(PHK, mnt);
static PHP_METHOD(PHK, path);
static PHP_METHOD(PHK, flags);
static PHP_METHOD(PHK, mtime);
static PHP_METHOD(PHK, options);
static PHP_METHOD(PHK, base_uri);
static PHP_METHOD(PHK, automap_uri);
static PHP_METHOD(PHK, plugin);
static PHP_METHOD(PHK, uri);
static PHP_METHOD(PHK, section_uri);
static PHP_METHOD(PHK, command_uri);
static PHP_METHOD(PHK, option);
static PHP_METHOD(PHK, parent_mnt);
static int web_access_matches(zval * apath, zval * path TSRMLS_DC);
static int web_access_allowed(PHK_Mnt * mp, zval * path TSRMLS_DC);
static char *goto_main(PHK_Mnt * mp TSRMLS_DC);
static char *web_tunnel(PHK_Mnt * mp, zval * path,int webinfo TSRMLS_DC);
static PHP_METHOD(PHK, web_tunnel);
static void PHK_mime_header(PHK_Mnt * mp, zval * path TSRMLS_DC);
static PHP_METHOD(PHK, mime_header);
static void PHK_mime_type(zval *ret, PHK_Mnt * mp, zval * path TSRMLS_DC);
static PHP_METHOD(PHK, mime_type);
static int PHK_is_php_source_path(PHK_Mnt * mp, zval * path TSRMLS_DC);
static PHP_METHOD(PHK, is_php_source_path);
static PHP_METHOD(PHK, proxy);
static void PHK_crc_check(PHK_Mnt * mp TSRMLS_DC);
static PHP_METHOD(PHK, accelerator_is_present);
static PHP_METHOD(PHK, build_info);
static PHP_METHOD(PHK, subpath_url);
static void PHK_get_subpath(zval * ret TSRMLS_DC);
static PHP_METHOD(PHK, get_subpath);
static zval *PHK_backend(PHK_Mnt * mp, zval * phk TSRMLS_DC);
static PHP_METHOD(PHK, __call);
static PHP_METHOD(PHK, accel_techinfo);
static PHP_METHOD(PHK, prolog);
static inline void init_mime_table(TSRMLS_D);
static void shutdown_mime_table(TSRMLS_D);
static void set_constants(zend_class_entry * ce);

static int MINIT_PHK(TSRMLS_D);
static int MSHUTDOWN_PHK(TSRMLS_D);
static inline int RINIT_PHK(TSRMLS_D);
static inline int RSHUTDOWN_PHK(TSRMLS_D);

/*============================================================================*/
#endif
