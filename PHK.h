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

static int MINIT_PHK(TSRMLS_D);
static int MSHUTDOWN_PHK(TSRMLS_D);

static int RINIT_PHK(TSRMLS_D);
static int RSHUTDOWN_PHK(TSRMLS_D);

static void  PHK_set_mp_property(zval *obj, PHK_Mnt_Info *mp TSRMLS_DC);
static void  PHK_need_php_runtime(TSRMLS_D);
static void  PHK_init(PHK_Mnt_Info *mp TSRMLS_DC);
static int   PHK_cache_enabled(PHK_Mnt_Info *mp, zval *command, zval *params, zval *path TSRMLS_DC);
static void  PHK_umount(PHK_Mnt_Info *mp TSRMLS_DC);
static void  PHK_mime_header(PHK_Mnt_Info *mp, zval *path TSRMLS_DC);
static zval *PHK_mime_type(PHK_Mnt_Info *mp, zval *path TSRMLS_DC);
static int   PHK_is_php_source_path(PHK_Mnt_Info *mp, zval *path TSRMLS_DC);
static void  PHK_crc_check(PHK_Mnt_Info *mp TSRMLS_DC);
static void  PHK_supports_php_version(PHK_Mnt_Info *mp TSRMLS_DC);
static void  PHK_get_subpath(zval *ret TSRMLS_DC);
static zval *PHK_backend(PHK_Mnt_Info *mp, zval *phk TSRMLS_DC);
