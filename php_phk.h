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

/*#define PHK_DEBUG*/

#ifndef PHP_PHK_H
#define PHP_PHK_H 1

#ifdef PHK_DEBUG
#define UT_DEBUG
#endif

#include "zend_extensions.h"

#include "utils.h"

/*---------------------------------------------------------------*/

#define PHK_ACCEL_VERSION "1.1.0"

#define PHK_ACCEL_MIN_VERSION "1.4.0"

#define PHP_PHK_EXTNAME "phk"

GLOBAL zend_module_entry phk_module_entry;

#define phpext_phk_ptr &phk_module_entry

/* Condition is experimental and will change */

#if ZEND_EXTENSION_API_NO >= 220071023
#	define ZEND_ENGINE_SUPPORTS_CACHE_KEY_WRAPPER_OPS
#endif

/*---------------------------------------------------------------*/

static DECLARE_CZVAL(false);
static DECLARE_CZVAL(true);
static DECLARE_CZVAL(null);
static DECLARE_CZVAL(slash);
static DECLARE_CZVAL(application_x_httpd_php);

static DECLARE_CZVAL(PHK_Stream_Backend);
static DECLARE_CZVAL(PHK_Backend);
static DECLARE_CZVAL(PHK_Util);
static DECLARE_CZVAL(PHK_Proxy);
static DECLARE_CZVAL(PHK_Creator);
static DECLARE_CZVAL(PHK);
static DECLARE_CZVAL(Automap);
static DECLARE_CZVAL(cache_enabled);
static DECLARE_CZVAL(umount);
static DECLARE_CZVAL(is_mounted);
static DECLARE_CZVAL(get_file_data);
static DECLARE_CZVAL(get_dir_data);
static DECLARE_CZVAL(get_stat_data);
static DECLARE_CZVAL(get_min_version);
static DECLARE_CZVAL(get_options);
static DECLARE_CZVAL(get_build_info);
static DECLARE_CZVAL(init);
static DECLARE_CZVAL(mount);
static DECLARE_CZVAL(crc_check);
static DECLARE_CZVAL(file_is_package);
static DECLARE_CZVAL(data_is_package);
static DECLARE_CZVAL(umount);
static DECLARE_CZVAL(subpath_url);
static DECLARE_CZVAL(call_method);
static DECLARE_CZVAL(run_webinfo);
static DECLARE_CZVAL(builtin_prolog);

/* Hash keys */

static DECLARE_HKEY(crc_check);
static DECLARE_HKEY(no_cache);
static DECLARE_HKEY(no_opcode_cache);
static DECLARE_HKEY(required_extensions);
static DECLARE_HKEY(map_defined);
static DECLARE_HKEY(mount_script);
static DECLARE_HKEY(umount_script);
static DECLARE_HKEY(plugin_class);
static DECLARE_HKEY(web_access);
static DECLARE_HKEY(min_php_version);
static DECLARE_HKEY(max_php_version);
static DECLARE_HKEY(mime_types);
static DECLARE_HKEY(web_run_script);
static DECLARE_HKEY(m);
static DECLARE_HKEY(web_main_redirect);
static DECLARE_HKEY(_PHK_path);
static DECLARE_HKEY(ORIG_PATH_INFO);
static DECLARE_HKEY(phk_backend);
static DECLARE_HKEY(lib_run_script);
static DECLARE_HKEY(cli_run_script);
static DECLARE_HKEY(auto_umount);
static DECLARE_HKEY(argc);
static DECLARE_HKEY(argv);
static DECLARE_HKEY(automap);
static DECLARE_HKEY(phk_stream_backend);
static DECLARE_HKEY(eaccelerator_get);

/*----- Mount flags --------*/

#define PHK_F_CRC_CHECK			4
#define PHK_F_NO_MOUNT_SCRIPT	8
#define PHK_F_CREATOR			16

/*============================================================================*/

typedef struct _PHK_Mnt_Info {
	struct _PHK_Mnt_Info *parent;
	int nb_children;
	struct _PHK_Mnt_Info **children;

	zval *mnt;					/* (String zval *) */
	ulong hash;					/* Mnt hash */
	zval *instance;				/* PHK object (NULL until created) */
	zval *proxy;				/* PHK_Proxy object (NULL until created) */
	zval *path;					/* (String zval *) */
	zval *plugin;				/* (Object zval *)|NULL */
	zval *flags;				/* (Long zval *) */
	zval *caching;				/* (Bool|null zval)* */
	zval *mtime;				/* Long */
	zval *backend;				/* PHK_Backend object (NULL until created) */

	/* Persistent data */

	zval *min_version;			/* (String zval *) */
	zval *options;				/* (Array zval *) */
	zval *build_info;			/* (Array zval *) */

	/* Shortcuts (persistent) */

	int crc_check;				/* Bool */
	int no_cache;				/* Bool */
	int no_opcode_cache;		/* Bool */
	int web_main_redirect;		/* Bool */
	int auto_umount;			/* Bool */
	zval *mime_types;			/* (Array zval *)|NULL */
	zval *web_run_script;		/* (String zval *)|NULL */
	zval *plugin_class;			/* (String zval *)|NULL */
	zval *web_access;			/* (Array zval *)|(String zval *)|NULL */
	zval *min_php_version;		/* (String zval *)|NULL */
	zval *max_php_version;		/* (String zval *)|NULL */

	/* Pre-computed constant values (persistent) */

	zval *base_uri;				/* (String zval *) */
	zval *automap_uri;			/* (String zval *)|NULL */
	zval *mount_script_uri;		/* (String zval *)|NULL */
	zval *umount_script_uri;	/* (String zval *)|NULL */
	zval *lib_run_script_uri;	/* (String zval *)|NULL */
	zval *cli_run_command;		/* (String zval *)|NULL */
} PHK_Mnt_Info;

/*============================================================================*/

ZEND_BEGIN_MODULE_GLOBALS(phk)

HashTable *mtab;		/* PHK_Mgr - Null until initialized */

zval caching;			/* PHK_Mgr - Can be null/true/false */

char root_package[UT_PATH_MAX + 1];

int php_runtime_is_loaded;

ZEND_END_MODULE_GLOBALS(phk)

#ifdef ZTS
#	define PHK_G(v) TSRMG(phk_globals_id, zend_phk_globals *, v)
#else
#	define PHK_G(v) (phk_globals.v)
#endif

/*---------------------------------------------------------------*/
#endif							/* PHP_PHK_H */
