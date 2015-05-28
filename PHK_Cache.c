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

/*============================================================================*/
/* APC */

/*-- Init --*/
/* Valid only in a web environment or if CLI is explicitely enabled */

static int PHK_Cache_apc_init(TSRMLS_D)
{
	return ((ut_is_web() || INI_BOOL("apc.enable_cli")) ? 1 : 0);
}

/*============================================================================*/
/* xcache */

/*-- Init --*/
/* Valid only in a web environment */

static int PHK_Cache_xcache_init(TSRMLS_D)
{
	return ((ut_is_web()) ? 1 : 0);
}

/*============================================================================*/
/* eaccelerator */

/*-- Init --*/
/* Valid only in a web environment */

static int PHK_Cache_eaccelerator_init(TSRMLS_D)
{
	if (!HKEY_EXISTS(EG(function_table),eaccelerator_get)) return 0;

	return ((ut_is_web()) ? 1 : 0);
}

/*============================================================================*/
/* PHK_Cache class                                                            */

/*---------------*/
/* cachePresent() */

/*-- C API ----*/

static int PHK_Cache_cachePresent(TSRMLS_D)
{
	return (cache != NULL);
}

/*-- PHP API --*/
/* {{{ proto bool PHK_Cache::cachePresent() */

static PHP_METHOD(PHK_Cache, cachePresent)
{
	RETURN_BOOL(PHK_Cache_cachePresent(TSRMLS_C));
}

/* }}} */
/*---------------*/
/* cacheName() */

/*-- C API ----*/

static char *PHK_Cache_cacheName(TSRMLS_D)
{
	return (cache ? cache->name : "none");
}

/*-- PHP API --*/
/* {{{ proto string PHK_Cache::cacheName() */

static PHP_METHOD(PHK_Cache, cacheName)
{
	RETVAL_STRING(PHK_Cache_cacheName(TSRMLS_C), 1);
}

/* }}} */
/*---------------*/
/* cacheID() */
/* Note: We use a different key for accelerated and non-accelerated versions
* because the cached data has different formats and, in theory, the cached data
* can be persistent across PHP invocations.
*/

/*-- C API ----*/

static void PHK_Cache_cacheID(const char *prefix, int prefix_len, const char *key,
	int key_len, zval * z_ret_p TSRMLS_DC)
{
	char *p;
	int len;

	p = ut_eallocate(NULL,(len = prefix_len + key_len + 9) + 1);
	memmove(p, "phk.acc.", 8);
	memmove(p + 8, prefix, prefix_len);
	p[prefix_len + 8] = '.';
	memmove(p + prefix_len + 9, key, key_len);
	p[prefix_len + key_len + 9] = '\0';

	ut_ezval_dtor(z_ret_p);
	ZVAL_STRINGL(z_ret_p, p, len, 0);
}

/*-- PHP API --*/
/* {{{ proto string PHK_Cache::cacheID(string prefix, string key) */

static PHP_METHOD(PHK_Cache, cacheID)
{
	char *prefix, *key;
	int prefix_len, key_len;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "ss", &prefix, &prefix_len, &key,
		 &key_len) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Cache_cacheID(prefix, prefix_len, key, key_len,
					   return_value TSRMLS_CC);
}

/* }}} */
/*---------------*/
/* {{{ proto void PHK_Cache::setCacheMaxSize(int size) */

static PHP_METHOD(PHK_Cache, setCacheMaxSize)
{
	long tmp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "l", &tmp)
 		== FAILURE)
 		EXCEPTION_ABORT("Cannot parse parameters");
	cache_maxsize = (int)tmp;
}

/* }}} */
/*---------------*/
/* get() */

/*-- C API ----*/

static void PHK_Cache_get(zval * z_key_p, zval * z_ret_p TSRMLS_DC)
{
	ut_ezval_dtor(z_ret_p);

	if (!cache) return;

	if (cache->get) cache->get(z_key_p, z_ret_p TSRMLS_CC);
	else {
		/* Forward the call to the registered 'get' function */

		ut_call_user_function(NULL, cache->get_funcname_string
			, cache->get_funcname_len, z_ret_p, 1, &z_key_p TSRMLS_CC);
	}
	/* Convert false to null */

	if ((Z_TYPE_P(z_ret_p) == IS_BOOL) && (!Z_BVAL_P(z_ret_p)))
		ZVAL_NULL(z_ret_p);
}

/*-- PHP API ----*/
/* {{{ proto string|null PHK_Cache::get(string key) */

static PHP_METHOD(PHK_Cache, get)
{
	zval *z_key_p;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &z_key_p)
		== FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Cache_get(z_key_p, return_value TSRMLS_CC);
}

/* }}} */
/*---------------*/
/* set() */

/*-- C API ----*/

static void PHK_Cache_set(zval * z_key_p, zval * z_data_p TSRMLS_DC)
{
	zval *args[3],*ttl;

	if (!cache) return;

	/* Check max size if string */

	if (Z_TYPE_P(z_data_p) != IS_ARRAY) {
		if (Z_TYPE_P(z_data_p) != IS_STRING) convert_to_string(z_data_p);
		if (Z_STRLEN_P(z_data_p) > cache_maxsize) return;
	}

	/* Call setter */

	if (cache->set) cache->set(z_key_p, z_data_p TSRMLS_CC);
	else {
		/* Forward to a PHP function when there is no C API */

		MAKE_STD_ZVAL(ttl);
		ZVAL_LONG(ttl,PHK_TTL);

		args[0] = z_key_p;
		args[1] = z_data_p;
		args[2] = ttl;

		ut_call_user_function_void(NULL, cache->set_funcname_string
			,cache->set_funcname_len, 3, args TSRMLS_CC);
		
		ut_ezval_ptr_dtor(&ttl);
	}
}

/*-- PHP API ----*/
/* {{{ proto void PHK_Cache::set(string key, string data) */

static PHP_METHOD(PHK_Cache, set)
{
	zval *z_key_p, *z_data_p;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &z_key_p, &z_data_p)
		== FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Cache_set(z_key_p, z_data_p TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static zend_function_entry PHK_Cache_functions[] = {
	PHP_ME(PHK_Cache, cachePresent, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache, cacheName, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache, cacheID, UT_2args_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache, setCacheMaxSize, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache, get, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache, set, UT_2args_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_PHK_Cache(TSRMLS_D)
{
	zend_class_entry ce, *entry;
	PHK_CACHE *cp;

	/*------*/
	/* Init class */

	INIT_CLASS_ENTRY(ce, "PHK\\Cache", PHK_Cache_functions);
	entry = zend_register_internal_class(&ce TSRMLS_CC);

	/*------*/
	/* Which cache system do we use ? */

	cp = cache_table;
	while (cp->name) {
		DBG_MSG1("PHK_Cache: Should I use %s ?", cp->name);
		if (ut_extension_loaded(cp->name, strlen(cp->name) TSRMLS_CC)
			&& cp->init(TSRMLS_C)) {
			cache = cp;
			DBG_MSG1("PHK_Cache: I will use %s", cp->name);
			break;
		}
		cp++;
	}

	return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Module shutdown                                               */

static int MSHUTDOWN_PHK_Cache(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RINIT_PHK_Cache(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RSHUTDOWN_PHK_Cache(TSRMLS_D)
{
	return SUCCESS;
}

/*============================================================================*/
