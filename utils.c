/*
  +----------------------------------------------------------------------+
  | Common utility function for PHP extensions                           |
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef PHP_WIN32
#include "win32/time.h"
#elif defined(NETWARE)
#include <sys/timeval.h>
#include <sys/time.h>
#else
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "php.h"
#include "zend_execute.h"
#include "ext/standard/php_var.h"
#include "ext/standard/html.h"
#include "SAPI.h"
#include "php_streams.h"
#include "TSRM/tsrm_virtual_cwd.h"
#include "ext/standard/php_string.h"

#include "utils.h"

/*============================================================================*/

static HashTable _ut_simul_inodes;

static unsigned long _ut_simul_inode_index;

StaticMutexDeclare(_ut_simul_inodes);

/*============================================================================*/
/* Generic arginfo structures */

ZEND_BEGIN_ARG_INFO_EX(UT_noarg_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_1arg_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, arg1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_2args_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_3args_arginfo, 0, 0, 3)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_ARG_INFO(0, arg3)
ZEND_END_ARG_INFO()

#ifdef RETURN_BY_REF_IS_BROKEN

ZEND_BEGIN_ARG_INFO_EX(UT_noarg_ref_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_1arg_ref_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, arg1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_2args_ref_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_3args_ref_arginfo, 0, 0, 3)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_ARG_INFO(0, arg3)
ZEND_END_ARG_INFO()

#else

ZEND_BEGIN_ARG_INFO_EX(UT_noarg_ref_arginfo, 0, 1, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_1arg_ref_arginfo, 0, 1, 1)
ZEND_ARG_INFO(0, arg1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_2args_ref_arginfo, 0, 1, 2)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(UT_3args_ref_arginfo, 0, 1, 3)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_ARG_INFO(0, arg3)
ZEND_END_ARG_INFO()

#endif

/*============================================================================*/
/*--- Init persistent data */

static void _ut_init_persistent_data(TSRMLS_D)
{
	MutexSetup(_ut_simul_inodes);

	zend_hash_init(&_ut_simul_inodes, 16, NULL,NULL, 1);

	_ut_simul_inode_index = 1;
}

/*---------------------------------------------------------------*/

static void _ut_shutdown_persistent_data(TSRMLS_D)
{
	zend_hash_destroy(&_ut_simul_inodes);

	MutexShutdown(_ut_simul_inodes);
}

/*---------------------------------------------------------------*/

static void *ut_allocate(void *ptr, size_t size, int persistent)
{
	if (ptr) {
		if (size) ptr=perealloc(ptr, size, persistent);
		else {
			pefree(ptr, persistent);
			ptr=NULL;
		}
	} else {
		if (size) ptr=pemalloc(size, persistent);
	}
return ptr;
}
	
#define ut_eallocate(ptr, size) ut_allocate(ptr, size, 0)
#define ut_pallocate(ptr, size) ut_allocate(ptr, size, 1)

#define PEALLOCATE(ptr, size, persistent)	ptr=ut_allocate(ptr, size, persistent)
#define EALLOCATE(ptr, size)	PEALLOCATE(ptr, size, 0)
#define PALLOCATE(ptr, size)	PEALLOCATE(ptr, size, 1)

/*---------------------------------------------------------------*/

static void *ut_duplicate(void *ptr, size_t size, int persistent)
{
	char *p;

	if (!ptr) return NULL;
	if (size==0) return ut_allocate(NULL,1,persistent);

	p=ut_allocate(NULL,size,persistent);
	memmove(p,ptr,size);
	return p;
}

#define ut_eduplicate(ptr, size) ut_duplicate(ptr, size, 0)
#define ut_pduplicate(ptr, size) ut_duplicate(ptr, size, 1)

/*---------------------------------------------------------------*/

#ifdef UT_DEBUG

#ifdef HAVE_GETTIMEOFDAY
static struct timeval _ut_base_tp;
#endif

/*------------*/

UT_SYMBOL void ut_dbg_init_time()
{
#ifdef HAVE_GETTIMEOFDAY
	struct timezone tz;

	(void)gettimeofday(&_ut_base_tp,&tz);
#endif
}

/*------------*/

UT_SYMBOL void ut_dbg_print_time()
{
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tp;
	struct timezone tz;
	time_t sec;

	(void)gettimeofday(&tp,&tz);
	sec=tp.tv_sec-base_tp.tv_sec;
	if (ut_is_web()) php_printf("<br>");
	php_printf("<");
	if (sec) php_printf("%ld/",sec);
	php_printf("%ld> : ",tp.tv_usec-_ut_base_tp.tv_usec);
	(void)gettimeofday(&_ut_base_tp,&tz);
#endif
}

#endif	/* UT_DEBUG */

/*---------------------------------------------------------------*/

UT_SYMBOL int ut_is_web()
{
	static int init_done=0;
	static int web;

	if (!init_done)
		{
		web=strcmp(sapi_module.name, "cli");
		init_done=1;
		}
	return web;
}

/*---------*/

UT_SYMBOL void ut_decref(zval *zp)
{
	Z_DELREF_P(zp);
	if (Z_REFCOUNT_P(zp)<=1) Z_UNSET_ISREF_P(zp);
}

/*---------*/
/* Free zval content and reset it */

UT_SYMBOL void ut_pezval_dtor(zval *zp, int persistent)
{
	if (persistent) {
		switch (Z_TYPE_P(zp) & ~IS_CONSTANT_INDEX) {
		  case IS_STRING:
		  case IS_CONSTANT:
			  pefree(Z_STRVAL_P(zp), persistent);
			  break;

		  case IS_ARRAY:
		  case IS_CONSTANT_ARRAY:
			  zend_hash_destroy(Z_ARRVAL_P(zp));
			  pefree(Z_ARRVAL_P(zp), persistent);
			  break;
		}
	} else {
		zval_dtor(zp);
	}
	INIT_ZVAL(*zp);
}

/*---------*/

UT_SYMBOL void ut_ezval_dtor(zval *zp) { ut_pezval_dtor(zp,0); }
UT_SYMBOL void ut_pzval_dtor(zval *zp) { ut_pezval_dtor(zp,1); }

/*---------*/
/* clear the zval pointer */

UT_SYMBOL void ut_pezval_ptr_dtor(zval ** zpp, int persistent)
{
	if (*zpp) {
		if (persistent) {
			ut_decref(*zpp);
			/* php_printf("Reference count = %d\n",Z_REFCOUNT_PP(zpp)); */
			if (Z_REFCOUNT_PP(zpp) == 0) {
				ut_pzval_dtor(*zpp);
				GC_REMOVE_ZVAL_FROM_BUFFER(*zpp);
				ut_pallocate(*zpp, 0);
			} 
		} else {
			zval_ptr_dtor(zpp);
		}
		(*zpp)=NULL;
	}
}

/*---------*/

UT_SYMBOL void ut_ezval_ptr_dtor(zval **zpp) { ut_pezval_ptr_dtor(zpp,0); }
UT_SYMBOL void ut_pzval_ptr_dtor(zval **zpp) { ut_pezval_ptr_dtor(zpp,1); }

/*---------*/

UT_SYMBOL void ut_persistent_array_init(zval * zp)
{
	HashTable *htp;

	htp=ut_pallocate(NULL,sizeof(HashTable));
	(void)zend_hash_init(htp,0, NULL,(dtor_func_t)ut_pzval_ptr_dtor,1);
	INIT_PZVAL(zp);
	ZVAL_ARRAY(zp, htp);
}

/*---------*/

UT_SYMBOL void ut_persistent_copy_ctor(zval ** ztpp)
{
	*ztpp=ut_persist_zval(*ztpp);
}

/*---------*/
/* Duplicates a zval and all its descendants to persistent storage */
/* Does not support objects and resources */

UT_SYMBOL zval *ut_persist_zval(zval * zsp)
{
	int type, len;
	char *p;
	zval *ztp;

	ALLOC_PERMANENT_ZVAL(ztp);
	INIT_PZVAL_COPY(ztp,zsp);

	switch (type = Z_TYPE_P(ztp)) {	/* Default: do nothing (when no additional data) */
	  case IS_STRING:
	  case IS_CONSTANT:
		  len = Z_STRLEN_P(zsp);
		  p=ut_pduplicate(Z_STRVAL_P(zsp), len + 1);
		  ZVAL_STRINGL(ztp, p, len, 0);
		  break;

	  case IS_ARRAY:
	  case IS_CONSTANT_ARRAY:
		  ut_persistent_array_init(ztp);
		  zend_hash_copy(Z_ARRVAL_P(ztp), Z_ARRVAL_P(zsp)
						 , (copy_ctor_func_t) ut_persistent_copy_ctor,
						 NULL, sizeof(zval *));
		  Z_TYPE_P(ztp) = type;
		  break;

	  case IS_OBJECT:
	  case IS_RESOURCE:
			{
			TSRMLS_FETCH();
			EXCEPTION_ABORT_RET(NULL,"Cannot make resources/objects persistent");
			}
	}
	return ztp;
}

/*---------------------------------------------------------------*/

UT_SYMBOL zval *ut_new_instance(char *class_name, int class_name_len,
	int construct, int nb_args,	zval ** args TSRMLS_DC)
{
	zend_class_entry **ce;
	zval *instance;

	if (zend_lookup_class_ex(class_name, class_name_len
#if PHP_API_VERSION >= 20100412
/* PHP 5.4+: additional argument to zend_lookup_class_ex */
							, NULL
#endif
							, 0, &ce TSRMLS_CC) == FAILURE) {
		EXCEPTION_ABORT_RET_1(NULL,"%s: class does not exist",class_name);
	}

	ALLOC_INIT_ZVAL(instance);
	object_init_ex(instance, *ce);

	if (construct) {
		ut_call_user_function_void(instance, ZEND_STRL("__construct"), nb_args,
			args TSRMLS_CC);
	}

	return instance;
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_call_user_function_void(zval *obj_zp, char *func,
	int func_len, int nb_args, zval ** args TSRMLS_DC)
{
	zval *ret;

	ALLOC_INIT_ZVAL(ret);
	ut_call_user_function(obj_zp, func, func_len, ret, nb_args, args TSRMLS_CC);
	ut_ezval_ptr_dtor(&ret);		/* Discard return value */
}

/*---------------------------------------------------------------*/

UT_SYMBOL int ut_call_user_function_bool(zval * obj_zp, char *func,
	int func_len, int nb_args, zval ** args TSRMLS_DC)
{
	zval *ret;
	int result;

	ALLOC_INIT_ZVAL(ret);
	ut_call_user_function(obj_zp, func, func_len, ret, nb_args, args TSRMLS_CC);
	result = zend_is_true(ret);
	ut_ezval_ptr_dtor(&ret);

	return result;
}

/*---------------------------------------------------------------*/

UT_SYMBOL long ut_call_user_function_long(zval *obj_zp, char *func,
	int func_len, int nb_args, zval ** args TSRMLS_DC)
{
	zval *ret;
	long result;

	ALLOC_INIT_ZVAL(ret);
	ut_call_user_function(obj_zp, func, func_len, ret, nb_args, args TSRMLS_CC);

	ENSURE_LONG(ret);
	result=Z_LVAL_P(ret);
	ut_ezval_ptr_dtor(&ret);

	return result;
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_call_user_function_string(zval *obj_zp, char *func,
	int func_len, zval * ret, int nb_args, zval ** args TSRMLS_DC)
{
	ut_call_user_function(obj_zp, func, func_len, ret, nb_args, args TSRMLS_CC);
	if (EG(exception)) return;

	ENSURE_STRING(ret);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_call_user_function_array(zval * obj_zp, char *func,
	int func_len, zval * ret, int nb_args, zval ** args TSRMLS_DC)
{
	ut_call_user_function(obj_zp, func, func_len, ret, nb_args, args TSRMLS_CC);
	if (EG(exception)) return;

	if (!ZVAL_IS_ARRAY(ret)) {
		THROW_EXCEPTION_2("%s method should return an array (type=%d)",
			func, Z_TYPE_P(ret));
	}
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_call_user_function(zval *obj_zp, char *func,
	int func_len, zval *ret, int nb_args, zval ** args TSRMLS_DC)
{
	int status;
	zval *func_zp;

#if ZEND_MODULE_API_NO <= 20050922
/* PHP version 5.1 doesn't accept 'class::function' as func_zp (static method).
It requires passing the class name as a string zval in obj_zp, and the method
name in func_zp. Unfortunately, this mechanism is rejected by PHP 5.3, which
requires a null obj_zp and a string 'class::function' in func_zp. This function
always receives 'class::function' and makes it compatible with old versions.
Note: I am not sure of the ZEND_MODULE_API value where this behavior changed. If
it is wrong, let me know. The only thing I know is that it changed between 5.1.6
and 5.3.9 */

	char *p,*op,*p2;
	int clen;

	MAKE_STD_ZVAL(func_zp);
	clen=0;
	if (!obj_zp) { /* Only on static calls */
		for (op=p=func;;p++) {
			if (!(*p)) break;
			if (((*p)==':')&&((*(p+1))==':')) {
				clen=p-op;
				break;
			}
		}
	}	
	if (clen) {
		p2=ut_eduplicate(op,clen+1);
		p2[clen]='\0';
		MAKE_STD_ZVAL(obj_zp);
		ZVAL_STRINGL(obj_zp,p2,clen,0);
		ZVAL_STRINGL(func_zp,op+clen+2,func_len-clen-2,1);
	} else {
		ZVAL_STRINGL(func_zp,func,func_len,1);	/* Default */
	}
#else
	MAKE_STD_ZVAL(func_zp);
	ZVAL_STRINGL(func_zp,func,func_len,1);
#endif

	status=call_user_function(EG(function_table), &obj_zp, func_zp, ret, nb_args,
		args TSRMLS_CC);
	ut_ezval_ptr_dtor(&func_zp);

#if ZEND_MODULE_API_NO <= 20050922
	if (clen) {
		ut_ezval_ptr_dtor(&obj_zp);
	}
#endif

	if (status!=SUCCESS) {
		THROW_EXCEPTION_1("call_user_function(func=%s) failed",func);
	}
}

/*---------------------------------------------------------------*/

UT_SYMBOL int ut_extension_loaded(char *name, int len TSRMLS_DC)
{
	int status;

	status=zend_hash_exists(&module_registry, name, len + 1);
	DBG_MSG2("Checking if extension %s is loaded: %s",name,(status ? "yes" : "no"));
	return status;
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_load_extension_file(zval *file TSRMLS_DC)
{
	if (!ut_call_user_function_bool(NULL,ZEND_STRL("dl"),1,&file TSRMLS_CC)) {
		THROW_EXCEPTION_1("%s: Cannot load extension",Z_STRVAL_P(file));
	}
}

/*---------------------------------------------------------------*/

#ifdef PHP_WIN32
#define _UT_LE_PREFIX "php_"
#else
#define _UT_LE_PREFIX
#endif

UT_SYMBOL void ut_load_extension(char *name, int len TSRMLS_DC)
{
	zval *zp;
	char *p;

	if (ut_extension_loaded(name, len TSRMLS_CC)) return;

	spprintf(&p,1024,_UT_LE_PREFIX "%s." PHP_SHLIB_SUFFIX,name);
	MAKE_STD_ZVAL(zp);
	ZVAL_STRING(zp,p,0);

	ut_load_extension_file(zp TSRMLS_CC);

	ut_ezval_ptr_dtor(&zp);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_load_extensions(zval * extensions TSRMLS_DC)
{
	HashTable *ht;
	HashPosition pos;
	zval **zpp;

	if (!ZVAL_IS_ARRAY(extensions)) {
		THROW_EXCEPTION("ut_load_extensions: argument should be an array");
		return;
	}

	ht = Z_ARRVAL_P(extensions);

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (zend_hash_get_current_data_ex(ht, (void **) (&zpp), &pos) ==
		   SUCCESS) {
		if (ZVAL_IS_STRING(*zpp)) {
			ut_load_extension(Z_STRVAL_PP(zpp),Z_STRLEN_PP(zpp) TSRMLS_CC);
			if (EG(exception)) return;
		}
		zend_hash_move_forward_ex(ht, &pos);
	}
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_require(char *string, zval * ret TSRMLS_DC)
{
	char *p;

	spprintf(&p, 1024, "require '%s';", string);

	zend_eval_string(p, ret, "eval" TSRMLS_CC);

	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

UT_SYMBOL int ut_strings_are_equal(zval * zp1, zval * zp2 TSRMLS_DC)
{
	if ((!zp1) || (!zp2))
		return 0;

	ENSURE_STRING(zp1);
	ENSURE_STRING(zp2);

	if (Z_STRLEN_P(zp1) != Z_STRLEN_P(zp2)) return 0;

	return (!memcmp(Z_STRVAL_P(zp1), Z_STRVAL_P(zp1), Z_STRLEN_P(zp1)));
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_header(long response_code, char *string TSRMLS_DC)
{
	sapi_header_line ctr;

	ctr.response_code = response_code;
	ctr.line = string;
	ctr.line_len = strlen(string);

	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_http_403_fail(TSRMLS_D)
{
	ut_header(403, "HTTP/1.0 403 Forbidden" TSRMLS_CC);

	ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_http_404_fail(TSRMLS_D)
{
	ut_header(404, "HTTP/1.0 404 Not Found" TSRMLS_CC);

	ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_exit(int status TSRMLS_DC)
{
	EG(exit_status) = status;
	zend_bailout();
}

/*---------------------------------------------------------------*/

UT_SYMBOL zval *_ut_SERVER_element(HKEY_STRUCT * hkey TSRMLS_DC)
{
	zval **array, **token;
	int status;

	status=FIND_HKEY(&EG(symbol_table), _SERVER, &array);
	if (status == FAILURE) {
		EXCEPTION_ABORT_RET(NULL,"_SERVER: symbol not found");
	}

	if (!ZVAL_IS_ARRAY(*array))
		EXCEPTION_ABORT_RET(NULL,"_SERVER: symbol is not of type array");

	status=zend_hash_quick_find(Z_ARRVAL_PP(array), hkey->string, hkey->len
		, hkey->hash, (void **) (&token));
	return (status == SUCCESS) ? (*token) : NULL;
}

/*---------------------------------------------------------------*/

UT_SYMBOL zval *_ut_REQUEST_element(HKEY_STRUCT * hkey TSRMLS_DC)
{
	zval **array, **token;
	int status;

	status=FIND_HKEY(&EG(symbol_table), _REQUEST, &array);
	if (status == FAILURE) {
		EXCEPTION_ABORT_RET(NULL,"_REQUEST: symbol not found");
	}

	if (!ZVAL_IS_ARRAY(*array))
		EXCEPTION_ABORT_RET(NULL,"_REQUEST: symbol is not of type array");

	status=zend_hash_quick_find(Z_ARRVAL_PP(array), hkey->string, hkey->len
		, hkey->hash, (void **) (&token));
	return (status == SUCCESS) ? (*token) : NULL;
}

/*---------------------------------------------------------------*/

UT_SYMBOL char *ut_http_base_url(TSRMLS_D)
{
	zval *pathinfo, *php_self;
	int ilen, slen, nslen;
	static char buffer[UT_PATH_MAX];

	if (!ut_is_web()) return "";

	pathinfo = SERVER_ELEMENT(PATH_INFO);
	if (EG(exception)) return NULL;
	php_self=SERVER_ELEMENT(PHP_SELF);
	if (EG(exception)) return NULL;

	if (!pathinfo) return Z_STRVAL_P(php_self);

	ilen = Z_STRLEN_P(pathinfo);
	slen = Z_STRLEN_P(php_self);

	nslen = slen - ilen;
	if ((nslen > 0)	&& (!memcmp(Z_STRVAL_P(pathinfo)
		, Z_STRVAL_P(php_self) + nslen, ilen))) {
		if (nslen >= UT_PATH_MAX) nslen = UT_PATH_MAX - 1;
		memmove(buffer, Z_STRVAL_P(php_self), nslen);
		buffer[nslen] = '\0';
		return buffer;
	}

	return Z_STRVAL_P(php_self);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_http_301_redirect(char *path, int must_free TSRMLS_DC)
{
	char *p,*base_url;

	base_url=ut_http_base_url(TSRMLS_C);
	if (EG(exception)) return;

	spprintf(&p, UT_PATH_MAX, "Location: http://%s%s%s",
			 Z_STRVAL_P(SERVER_ELEMENT(HTTP_HOST)),base_url,path);

	ut_header(301, p TSRMLS_CC);
	efree(p);

	ut_header(301, "HTTP/1.1 301 Moved Permanently" TSRMLS_CC);

	if (must_free) EALLOCATE(path,0);
	ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_rtrim_zval(zval * zp TSRMLS_DC)
{
	char *p;

	p = Z_STRVAL_P(zp) + Z_STRLEN_P(zp) - 1;
	while (Z_STRLEN_P(zp)) {
		if ((*p) != '/')
			break;
		(*p) = '\0';
		Z_STRLEN_P(zp)--;
		p--;
	}
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_tolower(char *p, int len TSRMLS_DC)
{
	int i;

	if (!len) return;
	for (i=0;i<len;i++,p++) {
		if (((*p) >= 'A') && ((*p) <= 'Z')) (*p) += ('a' - 'A');
	}
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_file_suffix(zval * path, zval * ret TSRMLS_DC)
{
	int found, suffix_len;
	char *p;

	if (Z_STRLEN_P(path) < 2) {
		ZVAL_STRINGL(ret, "", 0, 1);
		return;
	}

	for (suffix_len = found = 0, p =
		 Z_STRVAL_P(path) + Z_STRLEN_P(path) - 1; p >= Z_STRVAL_P(path);
		 p--, suffix_len++) {
		if ((*p) == '.') {
			found = 1;
			break;
		}
		if ((*p) == '/') break;
	}

	if (!found) {
		ZVAL_STRINGL(ret, "", 0, 1);
	} else {
		ZVAL_STRINGL(ret, p + 1, suffix_len, 1);
		ut_tolower(Z_STRVAL_P(ret),suffix_len TSRMLS_CC);
	}
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_unserialize_zval(const unsigned char *buffer
	, unsigned long len, zval *ret TSRMLS_DC)
{
	php_unserialize_data_t var_hash;

	if (len==0) {
		THROW_EXCEPTION("Empty buffer");
		return;
		}

	PHP_VAR_UNSERIALIZE_INIT(var_hash);

	INIT_ZVAL(*ret);
	if (!php_var_unserialize(&ret,&buffer,buffer+len,&var_hash TSRMLS_CC)) {
		ut_ezval_dtor(ret);
		THROW_EXCEPTION("Unserialize error");
		}
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
}

/*---------------------------------------------------------------*/
/* Basic (and fast) file_get_contents(). Ignores safe mode and magic quotes */

UT_SYMBOL void ut_file_get_contents(char *path, zval *ret TSRMLS_DC)
{
	php_stream *stream;
	char *contents;
	int len;

	stream=php_stream_open_wrapper(path,"rb",REPORT_ERRORS,NULL);
	if (!stream) EXCEPTION_ABORT_1("%s : Cannot open file",path);

	len=php_stream_copy_to_mem(stream,&contents,PHP_STREAM_COPY_ALL,0);
	php_stream_close(stream);

	if (len < 0) EXCEPTION_ABORT_1("%s : Cannot read file",path);

	ut_ezval_dtor(ret);
	ZVAL_STRINGL(ret,contents,len,0);
}

/*---------------------------------------------------------------*/

UT_SYMBOL char *ut_htmlspecialchars(char *src, int srclen
	, PHP_ESCAPE_HTML_ENTITIES_SIZE *retlen TSRMLS_DC)
{
	PHP_ESCAPE_HTML_ENTITIES_SIZE dummy;

	if (!retlen) retlen = &dummy;

	return php_escape_html_entities((unsigned char *)src,srclen,retlen,0
		,ENT_COMPAT,NULL TSRMLS_CC);
}

/*---------------------------------------------------------------*/

UT_SYMBOL char *ut_ucfirst(char *ptr, int len TSRMLS_DC)
{
	char *p2;

	if (!ptr) return NULL;

	p2=ut_eduplicate(ptr,len+1);

	if (((*p2) >= 'a') && ((*p2) <= 'z')) (*p2) -= 'a' - 'A';

	return p2;
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_repeat_printf(char c, int count TSRMLS_DC)
{
	char *p;

	if (!count) return;

	p=ut_eallocate(NULL,count);
	memset(p,c,count);
	PHPWRITE(p,count);
	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_printf_pad_right(char *str, int len, int size TSRMLS_DC)
{
	char *p;

	if (len >= size) {
		php_printf("%s",str);
		return;
	}

	p=ut_eallocate(NULL,size);
	memset(p,' ',size);
	memmove(p,str,len);
	PHPWRITE(p,size);
	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_printf_pad_both(char *str, int len, int size TSRMLS_DC)
{
	int pad;
	char *p;

	if (len >= size) {
		php_printf("%s",str);
		return;
	}

	p=ut_eallocate(NULL,size);
	memset(p,' ',size);
	pad=(size-len)/2;
	memmove(p+pad,str,len);
	PHPWRITE(p,size);
	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

UT_SYMBOL char *ut_absolute_dirname(char *path, int len, int *reslen, int separ TSRMLS_DC)
{
	char *dir,*res;
	int dlen;

	dir=ut_dirname(path,len,&dlen TSRMLS_CC);
	res=ut_mk_absolute_path(dir,dlen,reslen,separ TSRMLS_CC);
	EALLOCATE(dir,0);
	return res;
}

/*---------------------------------------------------------------*/

UT_SYMBOL char *ut_dirname(char *path, int len, int *reslen TSRMLS_DC)
{
	char *p;

	p=ut_eduplicate(path,len+1);
	(*reslen)=php_dirname(p,len);
	return p;
}

/*---------------------------------------------------------------*/

UT_SYMBOL int ut_is_uri(char *path, int len TSRMLS_DC)
{
	const char *p;
	int n;

	p=path;
	n=0;
	while(isalnum((int)(*p)) || (*p)=='+' || (*p)=='-' || *p=='.') {
		p++;
		n++;
	}

	return ((n>1) && ((*p)==':') && ((((*(p+1))=='/') && ((*(p+2))=='/'))
		|| ((n==4) && (path[0]=='d') && (path[1]=='a') && (path[2]=='t')
		&& (path[3]=='a'))));
}

/*---------------------------------------------------------------*/
/* Return an absolute path with a trailing separator or not*/

UT_SYMBOL char *ut_mk_absolute_path(char *path, int len, int *reslen
	, int separ TSRMLS_DC)
{
	char buf[1024];
	char *resp,*p;
	size_t clen;
	int dummy_reslen;

	if (!reslen) reslen=&dummy_reslen;

	if (ut_is_uri(path,len TSRMLS_CC)) {
		resp=ut_eallocate(NULL,len+2);
		memmove(resp,path,len+1);
		(*reslen)=len;
		if (separ && (resp[len-1]!='/')) {
			resp[len]='/';
			resp[len+1]='\0';
			(*reslen)++;
		}
		return resp;
	}

	if (IS_ABSOLUTE_PATH(path, len)) {
		resp=ut_eallocate(NULL,len+2);
		memmove(resp,path,len+1);
		(*reslen)=len;
		if (separ && (!IS_SLASH(resp[len-1]))) {
			resp[len]=DEFAULT_SLASH;
			resp[len+1]='\0';
			(*reslen)++;
		}
		return resp;
	}

	/* Relative path */

	virtual_getcwd(buf,sizeof(buf) TSRMLS_CC);
	clen=strlen(buf);
	resp=ut_eallocate(NULL,clen+len+3);
	memmove(resp,buf,clen+1);
	p=&(resp[clen]);

	if ((path[0]!='.')||(path[1])) {
		if (!IS_SLASH(*(p-1))) (*(p++))=DEFAULT_SLASH;
		memmove(p,path,len+1);
		p += len;
	}

	if (separ && (!IS_SLASH(*(p-1)))) {
		(*(p++))=DEFAULT_SLASH;
		(*p)='\0';
	}
	(*reslen)=p-resp;

	return resp;
}

/*---------------------------------------------------------------*/

UT_SYMBOL int ut_cut_at_space(char *p)
{
char *p1;

for(p1=p;;p++) {
	if (((*p)==' ')||((*p)=='\t')) (*p)='\0';
	if (!(*p)) break;
}
return (p-p1);
}

#define ut_zval_cut_at_space(zp) { \
	Z_STRLEN_P(zp)=ut_cut_at_space(Z_STRVAL_P(zp)); \
	}

/*---------------------------------------------------------------*/

UT_SYMBOL void ut_path_unique_id(char prefix, zval * path, zval ** mnt
	, time_t *mtp  TSRMLS_DC)
{
	char *p;
	php_stream_statbuf ssb;
	time_t mtime;
	unsigned long inode,dev,*lp,hash;
	int rlen;
	char resolved_path_buff[MAXPATHLEN];

	if (php_stream_stat_path(Z_STRVAL_P(path), &ssb) != 0) {
		EXCEPTION_ABORT_1("%s: Cannot stat",Z_STRVAL_P(path));
	}

	dev=(unsigned long) ssb.sb.st_dev;
	inode=(unsigned long) ssb.sb.st_ino;

#ifdef NETWARE
	mtime= ssb.sb.st_mtime.tv_sec;
#else
	mtime = ssb.sb.st_mtime;
#endif

	if (mnt) {
		if (!inode) {
			if (!VCWD_REALPATH(Z_STRVAL_P(path), resolved_path_buff))
				EXCEPTION_ABORT_1("%s: Cannot compute realpath",Z_STRVAL_P(path));

			rlen=strlen(resolved_path_buff)+1;
			hash=zend_get_hash_value(resolved_path_buff,rlen);

			MutexLock(_ut_simul_inodes);
			if (zend_hash_quick_find(&_ut_simul_inodes,resolved_path_buff
				,rlen,hash,(void **)(&lp))==SUCCESS) inode=(*lp);
			else {
				inode=_ut_simul_inode_index++;
				zend_hash_quick_add(&_ut_simul_inodes,resolved_path_buff
					,rlen,hash,(void *)(&inode),sizeof(inode),NULL);
			}
			MutexUnlock(_ut_simul_inodes);
		}

		spprintf(&p, 256, "%c_%lX_%lX_%lX", prefix, dev, inode, mtime);
		MAKE_STD_ZVAL(*mnt);
		ZVAL_STRING((*mnt), p, 0);
	}

	if (mtp) (*mtp)=mtime;
}	

/*---------------------------------------------------------------*/

UT_SYMBOL void _ut_build_hkeys(TSRMLS_D)
{
	INIT_HKEY(_SERVER);
	INIT_HKEY(_REQUEST);
	INIT_HKEY(PATH_INFO);
	INIT_HKEY(PHP_SELF);
	INIT_HKEY(HTTP_HOST);
}

/*========================================================================*/

UT_SYMBOL int MINIT_utils(TSRMLS_D)
{
	_ut_build_hkeys(TSRMLS_C);
	_ut_init_persistent_data(TSRMLS_C);

	return SUCCESS;
}

/*-------------*/

UT_SYMBOL int MSHUTDOWN_utils(TSRMLS_D)
{
	_ut_shutdown_persistent_data(TSRMLS_C);

	return SUCCESS;
}

/*-------------*/

UT_SYMBOL int RINIT_utils(TSRMLS_D)
{
	(void)zend_is_auto_global("_SERVER", sizeof("_SERVER")-1 TSRMLS_CC);
	(void)zend_is_auto_global("_REQUEST", sizeof("_REQUEST")-1 TSRMLS_CC);

	return SUCCESS;
}

/*-------------*/

UT_SYMBOL int RSHUTDOWN_utils(TSRMLS_D)
{
	return SUCCESS;
}

/*============================================================================*/
