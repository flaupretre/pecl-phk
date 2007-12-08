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

#include <stdio.h>

#include "php_ini.h"

#include "PHK_Cache.h"

ZEND_EXTERN_MODULE_GLOBALS(phk)

/*============================================================================*/

typedef struct
	{
	char *name;
	int  (*init)(void);
	void (*get)(zval *z_key_p, zval *z_ret_p TSRMLS_DC);
	void (*set)(zval *z_key_p, zval *z_data_p TSRMLS_DC);
	} PHK_CACHE;

static PHK_CACHE cache_table[]=
	{
	{ "apc", PHK_Cache_apc_init, NULL, NULL },
	{ "xcache", PHK_Cache_xcache_init, NULL, NULL },
	{ "eaccelerator", PHK_Cache_eaccelerator_init, NULL, NULL },
	{ "memcache", PHK_Cache_memcache_init
		, PHK_Cache_memcache_get, PHK_Cache_memcache_set },
	{ NULL,NULL,NULL,NULL }
	};

static PHK_CACHE *cache=NULL;

static int maxsize=524288;	/* Default: 512 Kb */

static zend_class_entry *class_entry;

static zval get_funcname;
static zval set_funcname;

#define PHK_TTL 3600
static zval ttl_zval;

/*============================================================================*/
/* APC */

/*-- Init --*/
/* Valid only in a web environment or if CLI is explicitely enabled */

static int PHK_Cache_apc_init()
{
if (ut_is_web() || INI_BOOL("apc.enable_cli"))
	{
	ZVAL_STRING(&get_funcname,"apc_fetch",0);
	ZVAL_STRING(&set_funcname,"apc_store",0);

	return 1;
	}
else return 0;
}

/*============================================================================*/
/* memcache */

/*-- Init --*/

static int PHK_Cache_memcache_init()
{
return 0;
}

/*-- Get --*/

static void PHK_Cache_memcache_get(zval *z_key_p, zval *z_ret_p TSRMLS_DC)
{
}

/*-- Set --*/

static void PHK_Cache_memcache_set(zval *z_key_p, zval *z_data_p TSRMLS_DC)
{
}

/*============================================================================*/
/* xcache */

/*-- Init --*/
/* Valid only in a web environment */

static int PHK_Cache_xcache_init()
{
if (ut_is_web())
	{
	ZVAL_STRING(&get_funcname,"xcache_get",0);
	ZVAL_STRING(&set_funcname,"xcache_set",0);

	return 1;
	}
else return 0;
}

/*============================================================================*/
/* eaccelerator */

/*-- Init --*/

static int PHK_Cache_eaccelerator_init()
{

return 0;
}

/*============================================================================*/
/* PHK_Cache class                                                            */

/*---------------*/
/* cache_present() */

/*-- C API ----*/

static int PHK_Cache_cache_present(TSRMLS_D)
{
return (cache != NULL);
}

/*-- PHP API --*/

static PHP_METHOD(PHK_Cache,cache_present)
{
RETURN_BOOL(PHK_Cache_cache_present(TSRMLS_C));
}

/*---------------*/
/* cache_name() */

/*-- C API ----*/

static char *PHK_Cache_cache_name(TSRMLS_D)
{
return (cache ? cache->name : "none");
}

/*-- PHP API --*/

static PHP_METHOD(PHK_Cache,cache_name)
{
RETVAL_STRING(PHK_Cache_cache_name(TSRMLS_C),1);
}

/*---------------*/
/* cache_id() */
/* Note: We use a different key for accelerated and non-accelerated versions
* because the cached data has different formats, and some caches are persistent
* across PHP invocations (memcache).
*/

/*-- C API ----*/

static void PHK_Cache_cache_id(char *prefix, int prefix_len, char *key
	, int key_len,zval *z_ret_p TSRMLS_DC)
{
char *p;
int len;

p=emalloc((len=prefix_len+key_len+9)+1);
memmove(p,"phk.acc.",8);
memmove(p+8,prefix,prefix_len);
p[prefix_len+8]='.';
memmove(p+prefix_len+9,key,key_len);
p[prefix_len+key_len+9]='\0';

ZVAL_STRINGL(z_ret_p,p,len,0);
}

/*-- PHP API --*/

static PHP_METHOD(PHK_Cache,cache_id)
{
char *prefix,*key;
int prefix_len,key_len;

if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &prefix,&prefix_len
	,&key,&key_len)	== FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters",0);

PHK_Cache_cache_id(prefix,prefix_len,key,key_len,return_value TSRMLS_CC);
}

/*---------------*/
/* set_maxsize() */

static PHP_METHOD(PHK_Cache,set_maxsize)
{
if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &maxsize)
	== FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);
}

/*---------------*/
/* get() */

/*-- C API ----*/

static void PHK_Cache_get(zval *z_key_p, zval *z_ret_p TSRMLS_DC)
{
ZVAL_NULL(z_ret_p);

if (!cache) return;

if (cache->get) cache->get(z_key_p,z_ret_p TSRMLS_CC);
else
	{
	/* Forward the call to the registered 'get' function */

	ut_call_user_function(NULL,&get_funcname,z_ret_p,1,&z_key_p TSRMLS_CC);
	}
/* Convert false to null */

if ((Z_TYPE_P(z_ret_p) == IS_BOOL) && (!Z_BVAL_P(z_ret_p))) ZVAL_NULL(z_ret_p);
}

/*-- PHP API ----*/

static PHP_METHOD(PHK_Cache,get)
{
zval *z_key_p;

if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &z_key_p)
	== FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

PHK_Cache_get(z_key_p,return_value TSRMLS_CC);
}

/*---------------*/
/* set() */

/*-- C API ----*/

static void PHK_Cache_set(zval *z_key_p, zval *z_data_p TSRMLS_DC)
{
zval *args[3];

if (!cache) return;

/* Check max size if string */

if (Z_TYPE_P(z_data_p) != IS_ARRAY)
	{
	if (Z_TYPE_P(z_data_p) != IS_STRING) convert_to_string(z_data_p);
	if (Z_STRLEN_P(z_data_p) > maxsize) return;
	}

/* Call setter */

if (cache->set) cache->set(z_key_p,z_data_p TSRMLS_CC);
else
	{
	/* Forward to a PHP function when there is no C API */

	args[0]=z_key_p;
	args[1]=z_data_p;
	args[2]=&ttl_zval;
	ut_call_user_function_void(NULL,&set_funcname,3,args TSRMLS_CC);
	}
}

/*-- PHP API ----*/

static PHP_METHOD(PHK_Cache,set)
{
zval *z_key_p,*z_data_p;

if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"zz",&z_key_p,&z_data_p)
	 == FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

PHK_Cache_set(z_key_p,z_data_p TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static zend_function_entry PHK_Cache_functions[]= {
	PHP_ME(PHK_Cache,cache_present,UT_noarg_arginfo,ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache,cache_name,UT_noarg_arginfo,ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache,cache_id,UT_2args_arginfo,ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache,set_maxsize,UT_1arg_arginfo,ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache,get,UT_1arg_arginfo,ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(PHK_Cache,set,UT_2args_arginfo,ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    {NULL, NULL, NULL, 0, 0 }
	};

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_PHK_Cache(TSRMLS_D)
{
zend_class_entry ce;
PHK_CACHE *cp;

/*------*/
/* Init class */

INIT_CLASS_ENTRY(ce, "PHK_Cache", PHK_Cache_functions);
class_entry=zend_register_internal_class(&ce TSRMLS_CC);

/*------*/
/* Which cache system do we use ? */

cp=cache_table;
while (cp->name)
	{
	DBG_MSG1("PHK_Cache: Should I use %s ?",cp->name);
	if (ut_extension_loaded(cp->name,strlen(cp->name) TSRMLS_CC)
		&& cp->init())
		{
		cache=cp;
		DBG_MSG1("PHK_Cache: I will use %s",cp->name);
		break;
		}
	cp++;
	}

if (cache)
	{
	/* Initialize the TTL zval */

	INIT_ZVAL(ttl_zval);
	ZVAL_LONG((&ttl_zval),PHK_TTL);
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

static int RINIT_PHK_Cache(TSRMLS_D)
{
return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_PHK_Cache(TSRMLS_D)
{
return SUCCESS;
}

/*============================================================================*/
