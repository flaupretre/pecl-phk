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

static int MINIT_PHK_Stream(TSRMLS_D);
static int MSHUTDOWN_PHK_Stream(TSRMLS_D);

static int RINIT_PHK_Stream(TSRMLS_D);
static int RSHUTDOWN_PHK_Stream(TSRMLS_D);

static void PHK_Stream_get_file(int dir,zval *z_ret_p,zval *z_uri_p
	,zval *z_mnt_p,zval *z_command_p,zval *z_params_p,zval *z_path_p
	,zval *z_cache_p TSRMLS_DC);

/*============================================================================*/
