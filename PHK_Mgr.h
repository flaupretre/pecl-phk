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

#ifdef ZTS
#include "TSRM.h"
#endif

static int MINIT_PHK_Mgr(TSRMLS_D);
static int MSHUTDOWN_PHK_Mgr(TSRMLS_D);

static int RINIT_PHK_Mgr(TSRMLS_D);
static int RSHUTDOWN_PHK_Mgr(TSRMLS_D);

static PHK_Mnt_Info *PHK_Mgr_get_mnt_info(zval *mnt, ulong hash, int exception TSRMLS_DC);
static void  PHK_Mgr_validate(zval *mnt, ulong hash TSRMLS_DC);
static void  PHK_Mgr_umount(zval *mnt, ulong hash TSRMLS_DC);
static void  PHK_Mgr_umount_mnt_info(PHK_Mnt_Info *mp TSRMLS_DC);
static zval *PHK_Mgr_instance_by_mp(PHK_Mnt_Info *mp TSRMLS_DC);
static zval *PHK_Mgr_instance(zval *mnt, ulong hash TSRMLS_DC);
static zval *PHK_Mgr_proxy_by_mp(PHK_Mnt_Info *mp TSRMLS_DC);
static zval *PHK_Mgr_proxy(zval *mnt, ulong hash TSRMLS_DC);
static void  PHK_Mgr_mnt_list(zval *ret TSRMLS_DC);
static int   PHK_Mgr_is_a_phk_uri(zval *path TSRMLS_DC);
static void  PHK_Mgr_uri(zval *mnt, zval *path, zval *ret TSRMLS_DC);
static void  PHK_Mgr_command_uri(zval *mnt, zval *command, zval *ret TSRMLS_DC);
static void  PHK_Mgr_section_uri(zval *mnt, zval *section, zval *ret TSRMLS_DC);
static void  PHK_Mgr_normalize_uri(zval *uri, zval *ret TSRMLS_DC);
static void  PHK_Mgr_uri_to_mnt(zval *uri, zval *ret TSRMLS_DC);
static void  PHK_Mgr_php_version_check(TSRMLS_D);
static void  PHK_Mgr_set_cache(zval *zp TSRMLS_DC);
static int   PHK_Mgr_cache_enabled(zval *mnt, ulong hash, zval *command, zval *params, zval *path TSRMLS_DC);
static void  PHK_Mgr_path_to_mnt(zval *path, zval *mnt TSRMLS_DC);
static PHK_Mnt_Info *PHK_Mgr_mount(zval *path, int flags TSRMLS_DC);

/*---------------------------------------------------------------*/
