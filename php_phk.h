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

#include "utils.h"

/*---------------------------------------------------------------*/

#define PHK_ACCEL_VERSION "1.1.0"

#define PHK_ACCEL_MIN_VERSION "1.4.0"

#define PHP_PHK_EXTNAME "phk"

GLOBAL zend_module_entry phk_module_entry;

#define phpext_phk_ptr &phk_module_entry

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
static DECLARE_CZVAL(Autoload);
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
static DECLARE_CZVAL(load);
static DECLARE_CZVAL(crc_check);
static DECLARE_CZVAL(file_is_package);
static DECLARE_CZVAL(data_is_package);
static DECLARE_CZVAL(unload);
static DECLARE_CZVAL(subpath_url);
static DECLARE_CZVAL(call_method);
static DECLARE_CZVAL(run_webinfo);
static DECLARE_CZVAL(require_extensions);
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
static DECLARE_HKEY(autoload);
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
	unsigned int *persistent_refcount_p;	/* Persistent reference counter */

	zval *mnt;					/* String */
	ulong hash;					/* Mnt hash */
	zval *instance;				/* PHK object */
	zval *proxy;				/* PHK_Proxy object (null until created) */
	zval *path;					/* String */
	zval *plugin;				/* Object zval|null */
	zval *flags;				/* Long */
	zval *caching;				/* Bool|null */
	zval *mtime;				/* Long */
	zval *backend;				/* PHK_Backend object | null */

	/* Persistent data */

	zval *min_version;			/* String */
	zval *options;				/* Array */
	zval *build_info;			/* Array */

	/* Shortcuts (persistent) */

	int crc_check;				/* Bool */
	int no_cache;				/* Bool */
	int no_opcode_cache;		/* Bool */
	int web_main_redirect;		/* Bool */
	int auto_umount;			/* Bool */
	zval *required_extensions;	/* Array zval or null */
	zval *mime_types;			/* Array zval or null */
	zval *web_run_script;		/* String zval or null */
	zval *plugin_class;			/* String zval or null */
	zval *web_access;			/* String zval or null */
	zval *min_php_version;		/* String zval or null */
	zval *max_php_version;		/* String zval or null */

	/* Pre-computed constant values (persistent) */

	zval *base_uri;				/* String zval */
	zval *autoload_uri;			/* String zval or null */
	zval *mount_script_uri;		/* String zval or null */
	zval *umount_script_uri;	/* String zval or null */
	zval *lib_run_script_uri;	/* String zval or null */
	zval *cli_run_command;		/* String zval or null */
} PHK_Mnt_Info;

/*============================================================================*/

ZEND_BEGIN_MODULE_GLOBALS(phk)
int init_done;
HashTable mnt_infos;			/* PHK_Mgr */
zval caching;					/* PHK_Mgr - Can be null/true/false */
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
