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

#ifndef __PHK_MGR_H
#define __PHK_MGR_H

#ifdef ZTS
#include "TSRM.h"
#endif

/*----- Mount flags --------*/

#define PHK_F_CRC_CHECK			4
#define PHK_F_NO_MOUNT_SCRIPT	8
#define PHK_F_CREATOR			16

/*============================================================================*/

typedef struct {
	int init_done;
	time_t ctime;

	zval *min_version;			/* String */
	zval *options;				/* Array */
	zval *build_info;			/* Array */

	/* Shortcuts */

	int no_cache;				/* Bool */
	int no_opcode_cache;		/* Bool */
	int web_main_redirect;		/* Bool */
	int auto_umount;			/* Bool */
	zval *mime_types;			/* Array zval or null */
	zval *web_run_script;		/* String zval or null */
	zval *plugin_class;			/* String zval or null */
	zval *web_access;			/* String|Array zval or null */
	zval *min_php_version;		/* String zval or null */
	zval *max_php_version;		/* String zval or null */

	/* Pre-computed constant values */

	zval *base_uri;				/* String zval */
	zval *automap_uri;			/* String zval or null if map not defined */
	zval *mount_script_uri;		/* String zval or null if not defined */
	zval *umount_script_uri;	/* String zval or null if not defined */
	zval *lib_run_script_uri;	/* String zval or null if not defined */
	zval *cli_run_command;		/* String zval or null if not defined */
} PHK_Pdata;

/*============================================================================*/

typedef struct _PHK_Mnt {
	PHK_Pdata *pdata;

	struct _PHK_Mnt *parent;
	int nb_children;
	struct _PHK_Mnt **children;

	zval *mnt;					/* (String zval *) */
	ulong hash;					/* Mnt hash */
	int order;

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

	/* Shortcuts (copy from persistent data) */

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
	long automap_id;			/* Long */
	zval *mount_script_uri;		/* (String zval *)|NULL */
	zval *umount_script_uri;	/* (String zval *)|NULL */
	zval *lib_run_script_uri;	/* (String zval *)|NULL */
	zval *cli_run_command;		/* (String zval *)|NULL */
} PHK_Mnt;

/*============================================================================*/

static HashTable persistent_mtab;

static int tmp_mnt_num;

StaticMutexDeclare(persistent_mtab);

/*============================================================================*/

static void PHK_Mgr_init_pdata(TSRMLS_D);
static void PHK_Mgr_shutdown_pdata(TSRMLS_D);
static void PHK_Mgr_mnt_dtor(PHK_Mnt * mp);
static void PHK_Mgr_remove_mnt(PHK_Mnt * mp TSRMLS_DC);
static PHK_Mnt *PHK_Mgr_new_mnt(zval * mnt, ulong hash TSRMLS_DC);
static PHK_Mnt *PHK_Mgr_get_mnt(zval * mnt, ulong hash,
										  int exception TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, is_mounted);
static void PHK_Mgr_validate(zval * mnt, ulong hash TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, validate);
static void PHK_Mgr_umount(zval * mnt, ulong hash TSRMLS_DC);
static void PHK_Mgr_umount_mnt(PHK_Mnt * mp TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, umount);
static zval *PHK_Mgr_instance_by_mp(PHK_Mnt * mp TSRMLS_DC);
static zval *PHK_Mgr_instance(zval * mnt, ulong hash TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, instance);
static zval *PHK_Mgr_proxy_by_mp(PHK_Mnt * mp TSRMLS_DC);
static zval *PHK_Mgr_proxy(zval * mnt, ulong hash TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, proxy);
static void PHK_Mgr_mnt_list(zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, mnt_list);
static int PHK_Mgr_is_a_phk_uri(zval * path TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, is_a_phk_uri);
static void PHK_Mgr_uri(zval * mnt, zval * path, zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, uri);
static void PHK_Mgr_command_uri(zval * mnt, zval * command,
								zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, command_uri);
static void PHK_Mgr_section_uri(zval * mnt, zval * section,
								zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, section_uri);
static void PHK_Mgr_normalize_uri(zval * uri, zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, normalize_uri);
static void compute_automap_uri(zval * mnt, zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, automap_uri);
static void compute_base_uri(zval * mnt, zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, base_uri);
static void PHK_Mgr_uri_to_mnt(zval * uri, zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, uri_to_mnt);
static void PHK_Mgr_toplevel_path(zval * zpath, zval * ret TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, toplevel_path);
static void PHK_Mgr_php_version_check(TSRMLS_D);
static PHP_METHOD(PHK_Mgr, php_version_check);
static void PHK_Mgr_set_cache(zval * zp TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, set_cache);
static int PHK_Mgr_cache_enabled(zval * mnt, ulong hash, zval * command,
								 zval * params, zval * path TSRMLS_DC);
static void PHK_Mgr_path_to_mnt(zval * path, zval * mnt TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, path_to_mnt);
static void PHK_Mgr_compute_mnt(zval * path, PHK_Mnt ** parent_mpp,
								zval ** mnt, zval ** mtime TSRMLS_DC);
static PHK_Mnt *PHK_Mgr_mount(zval * path, long flags TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, mount);
static PHK_Pdata *PHK_Mgr_get_pdata(zval * mnt, ulong hash TSRMLS_DC);
static PHK_Pdata *PHK_Mgr_get_or_create_pdata(zval * mnt,ulong hash TSRMLS_DC);
static void PHK_Mgr_populate_pdata(zval * mnt, ulong hash,PHK_Mnt * mp TSRMLS_DC);
static PHP_METHOD(PHK_Mgr, mime_header);
static void PHK_Mgr_Persistent_Data_dtor(PHK_Pdata * entry);

static int MINIT_PHK_Mgr(TSRMLS_D);
static int MSHUTDOWN_PHK_Mgr(TSRMLS_D);
static inline int RINIT_PHK_Mgr(TSRMLS_D);
static inline int RSHUTDOWN_PHK_Mgr(TSRMLS_D);

/*============================================================================*/
#endif
