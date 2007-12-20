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

static int PHK_Cache_apc_init(TSRMLS_D);

static int PHK_Cache_xcache_init(TSRMLS_D);

static int PHK_Cache_eaccelerator_init(TSRMLS_D);

static int MINIT_PHK_Cache(TSRMLS_D);
static int MSHUTDOWN_PHK_Cache(TSRMLS_D);

static int RINIT_PHK_Cache(TSRMLS_D);
static int RSHUTDOWN_PHK_Cache(TSRMLS_D);

static int PHK_Cache_cache_present(TSRMLS_D);
static char *PHK_Cache_cache_name(TSRMLS_D);
static void PHK_Cache_cache_id(char *prefix, int prefix_len, char *key,
							   int key_len, zval * z_ret_p TSRMLS_DC);
static void PHK_Cache_get(zval * z_key_p, zval * z_ret_p TSRMLS_DC);
static void PHK_Cache_set(zval * z_key_p, zval * z_data_p TSRMLS_DC);

/*============================================================================*/
