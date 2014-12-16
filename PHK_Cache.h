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

#ifndef __PHK_CACHE_H
#define __PHK_CACHE_H

/*============================================================================*/

static int PHK_Cache_apc_init(TSRMLS_D);
static int PHK_Cache_xcache_init(TSRMLS_D);
static int PHK_Cache_eaccelerator_init(TSRMLS_D);

/*============================================================================*/

#define PHK_TTL 3600

typedef struct {
	char *name;
	int (*init) (TSRMLS_D);
	void (*get) (zval * z_key_p, zval * z_ret_p TSRMLS_DC);
	char *get_funcname_string;
	int get_funcname_len;
	void (*set) (zval * z_key_p, zval * z_data_p TSRMLS_DC);
	char *set_funcname_string;
	int set_funcname_len;
} PHK_CACHE;

static PHK_CACHE cache_table[] = {
	{"apc", PHK_Cache_apc_init
		, NULL, ZEND_STRL("apc_fetch"), NULL, ZEND_STRL("apc_store")},
	{"xcache", PHK_Cache_xcache_init
		, NULL, ZEND_STRL("xcache_get"), NULL, ZEND_STRL("xcache_set")},
	{"eaccelerator", PHK_Cache_eaccelerator_init
		, NULL, ZEND_STRL("eaccelerator_get"), NULL, ZEND_STRL("eaccelerator_put")},
	{ NULL }
};

static PHK_CACHE *cache = NULL;

static int cache_maxsize = 524288;	/* Default: 512 Kb */

/*============================================================================*/

static int PHK_Cache_cache_present(TSRMLS_D);
static PHP_METHOD(PHK_Cache, cache_present);
static char *PHK_Cache_cache_name(TSRMLS_D);
static PHP_METHOD(PHK_Cache, cache_name);
static void PHK_Cache_cache_id(const char *prefix, int prefix_len, const char *key,
							   int key_len, zval * z_ret_p TSRMLS_DC);
static PHP_METHOD(PHK_Cache, cache_id);
static PHP_METHOD(PHK_Cache, set_maxsize);
static void PHK_Cache_get(zval * z_key_p, zval * z_ret_p TSRMLS_DC);
static PHP_METHOD(PHK_Cache, get);
static void PHK_Cache_set(zval * z_key_p, zval * z_data_p TSRMLS_DC);
static PHP_METHOD(PHK_Cache, set);

static int MINIT_PHK_Cache(TSRMLS_D);
static int MSHUTDOWN_PHK_Cache(TSRMLS_D);
static inline int RINIT_PHK_Cache(TSRMLS_D);
static inline int RSHUTDOWN_PHK_Cache(TSRMLS_D);

/*============================================================================*/

#endif
