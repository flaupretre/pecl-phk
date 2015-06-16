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

#ifndef __PECL_UTILS_H
#define __PECL_UTILS_H 1

/*============================================================================*/
/* Configuration */

#define UT_PRIVATE_SYMBOLS	/* Symbols are private - uncomment to export symbols */

#define UT_PATH_MAX 1023

/*============================================================================*/

#include "compat.h"

#include "php.h"
#include "zend_exceptions.h"
#include "zend_hash.h"
#include "TSRM/TSRM.h"

#ifdef UT_PRIVATE_SYMBOLS
#	define UT_SYMBOL static
#else
#	define UT_SYMBOL
#endif

/*----------------*/
/* The ancestor of zend_string :), now using zend_string */
/* These strings have a constant value and are persistent */

#define HKEY(name) ( hkey_ ## name )

#define DECLARE_HKEY(name)	zend_string *HKEY(name)

#define INIT_HKEY(name) INIT_HKEY_VALUE(name, #name)

#define INIT_HKEY_VALUE(name,value) \
	{ \
	HKEY(name)=zend_string_init(value,sizeof(value)-1,1); \
	(void)ZSTR_HASH(HKEY(name)); \
	}

#define _ZSTR_VALUES(zsp) ZSTR_VAL(zsp), ZSTR_LEN(zsp)+1, ZSTR_HASH(zsp)

#ifdef PHPNG
TODO
#else
#	define FIND_ZSTRING(ht, zsp,respp) \
		zend_hash_quick_find(ht,ZSTR_VALUES(zsp),(void **)(respp))

#	define FIND_HKEY(ht,name,respp) FIND_ZSTRING(ht, HKEY(name), respp)

#	define ZSTRING_EXISTS(ht, zsp) zend_hash_quick_exists(ht,ZSTR_VALUES(zsp))

#	define HKEY_EXISTS(ht,name) ZSTRING_EXISTS(ht, HKEY(name))

#endif

#define SERVER_ELEMENT(name) _ut_SERVER_element(HKEY(name) TSRMLS_CC)

#define REQUEST_ELEMENT(name) _ut_REQUEST_element(HKEY(name) TSRMLS_CC)

#define CLEAR_DATA(_v)	memset(&(_v),'\0',sizeof(_v)); \

/*---------------------------------------------------------------*/
/* Debug messages and exception mgt */

#ifdef UT_DEBUG
#define DBG_INIT() ut_dbg_init_time()
#define DBG_MSG(_format) { ut_dbg_print_time(); php_printf(_format "\n"); }
#define DBG_MSG1(_format,_var1) { ut_dbg_print_time(); php_printf(_format "\n",_var1); }
#define DBG_MSG2(_format,_var1,_var2) { ut_dbg_print_time(); php_printf(_format "\n",_var1,_var2); }
#define DBG_MSG3(_format,_var1,_var2,_var3) { ut_dbg_print_time(); php_printf(_format "\n",_var1,_var2,_var3); }
#define CHECK_MEM()	full_mem_check(1)
#else
#define DBG_INIT()
#define DBG_MSG(_format)
#define DBG_MSG1(_format,_var1)
#define DBG_MSG2(_format,_var1,_var2)
#define DBG_MSG3(_format,_var1,_var2,_var3)
#define CHECK_MEM()
#endif

#define THROW_EXCEPTION(_format)	\
	{ \
	DBG_MSG("Throwing exception: " _format); \
	(void)zend_throw_exception_ex(NULL,0 TSRMLS_CC ,_format); \
	}

#define THROW_EXCEPTION_1(_format,_arg1)	\
	{ \
	DBG_MSG1("Throwing exception: " _format , _arg1); \
	(void)zend_throw_exception_ex(NULL,0 TSRMLS_CC ,_format,_arg1); \
	}

#define THROW_EXCEPTION_2(_format,_arg1,_arg2)	\
	{ \
	DBG_MSG2("Throwing exception: " _format , _arg1, _arg2); \
	(void)zend_throw_exception_ex(NULL,0 TSRMLS_CC ,_format,_arg1,_arg2); \
	}

#define THROW_EXCEPTION_3(_format,_arg1,_arg2,_arg3)	\
	{ \
	DBG_MSG3("Throwing exception: " _format , _arg1, _arg2, _arg3); \
	(void)zend_throw_exception_ex(NULL,0 TSRMLS_CC ,_format,_arg1,_arg2,_arg3); \
	}

#define EXCEPTION_ABORT(_format)	\
	{ \
	THROW_EXCEPTION(_format); \
	return; \
	}

#define EXCEPTION_ABORT_1(_format,_arg1)	\
	{ \
	THROW_EXCEPTION_1(_format,_arg1); \
	return; \
	}

#define EXCEPTION_ABORT_RET(_ret,_format)	\
	{ \
	THROW_EXCEPTION(_format); \
	return _ret; \
	}

#define EXCEPTION_ABORT_RET_1(_ret,_format,_arg1)	\
	{ \
	THROW_EXCEPTION_1(_format,_arg1); \
	return _ret; \
	}

#define CHECK_PATH_LEN(_zp,_delta) \
	{ \
	if (Z_STRLEN_P((_zp)) > (PATH_MAX-_delta-1)) \
		{ \
		EXCEPTION_ABORT_1("Path too long",NULL); \
		} \
	}

#define CHECK_PATH_LEN_RET(_zp,_delta,_ret) \
	{ \
	if (Z_STRLEN_P((_zp)) > (PATH_MAX-_delta-1)) \
		{ \
		EXCEPTION_ABORT_RET_1(_ret,"Path too long",NULL); \
		} \
	}

/*-----------*/

#define ALLOC_INIT_PERMANENT_ZVAL(zp) { \
	ALLOC_PERMANENT_ZVAL(zp); \
	INIT_ZVAL(*zp); \
	}

#define ENSURE_LONG(zp) { if (Z_TYPE_P((zp))!=IS_LONG) convert_to_long((zp)); }
#define ENSURE_BOOL(zp) { if (Z_TYPE_P((zp))!=IS_BOOL) convert_to_boolean((zp)); }
#define ENSURE_STRING(zp) { if (Z_TYPE_P((zp))!=IS_STRING) convert_to_string((zp)); }

#define RETVAL_BY_VAL(zp) \
	{ \
	INIT_PZVAL_COPY(return_value,zp); \
	zval_copy_ctor(return_value); \
	}

/* Note: return by ref does not work in PHP 5.2 because, after the call,
   the refcount is systematically reset to 1. Workaround: Return a copy
   (slower). This is a bug present at least in 5.2.4. Bug is at line 1005
   in zend_execute_API.c 'v 1.331.2.20.2.24 2007/07/21'. The bug is fixed
   in CVS (5.3 branch).
   Warning: Using RETVAL_BY_REF requires to set the 'return_by_ref' flag in
   the corresponding ARG_INFO declaration. If not set, return_value_ptr is null.
*/

#if ZEND_MODULE_API_NO < 20090626
#define RETURN_BY_REF_IS_BROKEN
#endif

#ifdef RETURN_BY_REF_IS_BROKEN
#define RETVAL_BY_REF(zp) RETVAL_BY_VAL(zp)
#else
#define RETVAL_BY_REF(zp) \
	{ \
	ut_ezval_ptr_dtor(return_value_ptr); \
	Z_ADDREF_P(zp); \
	*return_value_ptr=(zp); \
	}
#endif

#define UT_ADD_PZP_CONST(ce,name) \
	zend_hash_add(&((ce)->constants_table),name,sizeof(name) \
		,(void *)(&zp),sizeof(zp),NULL)

#define UT_DECLARE_CHAR_CONSTANT(_def,_name) \
	{ \
	char *p=NULL; \
	zval *zp; \
	\
	ALLOC_INIT_PERMANENT_ZVAL(zp); \
	PALLOCATE(p,2); \
	p[0]=_def; \
	p[1]='\0'; \
	ZVAL_STRINGL(zp,p,1,0); \
	UT_ADD_PZP_CONST(ce,_name); \
	}

#define UT_DECLARE_STRING_CONSTANT(_def,_name) \
	{  \
	char *p=NULL;  \
	zval *zp;  \
	 \
	ALLOC_INIT_PERMANENT_ZVAL(zp);  \
	PALLOCATE(p,sizeof(_def));  \
	memmove(p,_def,sizeof(_def)); \
	ZVAL_STRINGL(zp,p,sizeof(_def)-1,0);  \
	UT_ADD_PZP_CONST(ce,_name); \
	}

#define UT_DECLARE_LONG_CONSTANT(_def,_name) \
	{  \
	zval *zp; \
	\
	ALLOC_INIT_PERMANENT_ZVAL(zp); \
	ZVAL_LONG(zp, _def); \
	UT_ADD_PZP_CONST(ce,_name); \
	}

/*-- Thread-safe stuff ------*/

#ifdef ZTS
#define MutexDeclare(x)		MUTEX_T x ## _mutex
#define StaticMutexDeclare(x)	static MUTEX_T x ## _mutex
#define MutexSetup(x)		x ## _mutex = tsrm_mutex_alloc()
#define MutexShutdown(x)	tsrm_mutex_free(x ## _mutex)
#define MutexLock(x)		tsrm_mutex_lock(x ## _mutex)
#define MutexUnlock(x)		tsrm_mutex_unlock(x ## _mutex)
#else
#define MutexDeclare(x)
#define StaticMutexDeclare(x)
#define MutexSetup(x)
#define MutexShutdown(x)
#define MutexLock(x)
#define MutexUnlock(x)
#endif

/*============================================================================*/

UT_SYMBOL DECLARE_HKEY(_SERVER);
UT_SYMBOL DECLARE_HKEY(_REQUEST);
UT_SYMBOL DECLARE_HKEY(PATH_INFO);
UT_SYMBOL DECLARE_HKEY(PHP_SELF);
UT_SYMBOL DECLARE_HKEY(HTTP_HOST);

/*============================================================================*/

#ifdef UT_DEBUG
UT_SYMBOL void ut_dbg_init_time();
UT_SYMBOL inline void ut_dbg_print_time();
#endif

UT_SYMBOL inline int ut_is_web(void);
UT_SYMBOL void ut_decref(zval *zp);
UT_SYMBOL void ut_pezval_dtor(zval *zp, int persistent);
UT_SYMBOL void ut_ezval_dtor(zval *zp);
UT_SYMBOL void ut_pzval_dtor(zval *zp);
UT_SYMBOL void ut_pezval_ptr_dtor(zval ** zpp, int persistent);
UT_SYMBOL void ut_ezval_ptr_dtor(zval **zpp);
UT_SYMBOL void ut_pzval_ptr_dtor(zval **zpp);
UT_SYMBOL void ut_persistent_array_init(zval * zp);
UT_SYMBOL void ut_persistent_copy_ctor(zval ** ztpp);
UT_SYMBOL zval *ut_persist_zval(zval * zsp);
UT_SYMBOL zval *ut_new_instance(char *class_name, int class_name_len,
	int construct, int nb_args,	zval * args TSRMLS_DC);
UT_SYMBOL inline void ut_call_user_function_void(zval *obj_zp, char *func,
	int func_len, int nb_args, zval * args TSRMLS_DC);
UT_SYMBOL inline int ut_call_user_function_bool(zval * obj_zp, char *func,
	int func_len, int nb_args, zval * args TSRMLS_DC);
UT_SYMBOL inline long ut_call_user_function_long(zval *obj_zp, char *func,
	int func_len, int nb_args, zval * args TSRMLS_DC);
UT_SYMBOL inline void ut_call_user_function_string(zval *obj_zp, char *func,
	int func_len, zval * ret, int nb_args, zval * args TSRMLS_DC);
UT_SYMBOL inline void ut_call_user_function_array(zval * obj_zp, char *func,
	int func_len, zval * ret, int nb_args, zval * args TSRMLS_DC);
UT_SYMBOL inline void ut_call_str_user_function(zval *obj_zp, char *func,
	int func_len, zval *ret, int nb_args, zval * args TSRMLS_DC);
UT_SYMBOL inline void ut_call_zval_user_function(zval *obj_zp, zval *func_zp,
	zval *ret, int nb_args, zval * args TSRMLS_DC);
UT_SYMBOL int ut_extension_loaded(char *name, int len TSRMLS_DC);
UT_SYMBOL void ut_load_extension_file(zval *file TSRMLS_DC);
UT_SYMBOL void ut_load_extension(zend_string *name TSRMLS_DC);
UT_SYMBOL void ut_load_extensions(zval * extensions TSRMLS_DC);
UT_SYMBOL void ut_require(char *string, zval *ret TSRMLS_DC);
UT_SYMBOL inline int ut_strings_are_equal(zval * zp1, zval * zp2 TSRMLS_DC);
UT_SYMBOL void ut_header(long response_code, char *string TSRMLS_DC);
UT_SYMBOL void ut_http403Fail(TSRMLS_D);
UT_SYMBOL void ut_http404Fail(TSRMLS_D);
UT_SYMBOL void ut_exit(int status TSRMLS_DC);
UT_SYMBOL inline zval *_ut_SERVER_element(HKEY_STRUCT * hkey TSRMLS_DC);
UT_SYMBOL inline zval *_ut_REQUEST_element(HKEY_STRUCT * hkey TSRMLS_DC);
UT_SYMBOL char *ut_httpBaseURL(TSRMLS_D);
UT_SYMBOL void ut_http301Redirect(char *path, int must_free TSRMLS_DC);
UT_SYMBOL char *ut_trim_char(char *str, int *lenp, char c);
UT_SYMBOL inline void ut_rtrim_zval(zval * zp TSRMLS_DC);
UT_SYMBOL inline void ut_tolower(char *p, int len TSRMLS_DC);
UT_SYMBOL inline void ut_fileSuffix(zval * path, zval * ret TSRMLS_DC);
UT_SYMBOL void ut_unserialize_zval(const unsigned char *buffer
	, unsigned long len, zval *ret TSRMLS_DC);
UT_SYMBOL void ut_file_get_contents(char *path, zval *ret TSRMLS_DC);
UT_SYMBOL char *ut_htmlspecialchars(char *src, int srclen, PHP_ESCAPE_HTML_ENTITIES_SIZE *retlen TSRMLS_DC);
UT_SYMBOL char *ut_ucfirst(char *ptr, int len TSRMLS_DC);
UT_SYMBOL void ut_repeat_printf(char c, int count TSRMLS_DC);
UT_SYMBOL void ut_printf_pad_right(char *str, int len, int size TSRMLS_DC);
UT_SYMBOL void ut_printf_pad_both(char *str, int len, int size TSRMLS_DC);
UT_SYMBOL zend_string *ut_absolute_dirname(zend_string *path, int separ TSRMLS_DC);
UT_SYMBOL zend_string *ut_dirname(zend_string *path TSRMLS_DC);
UT_SYMBOL inline int ut_is_uri(zend_string *path TSRMLS_DC);
UT_SYMBOL zend_string *ut_mkAbsolutePath(zend_string *path, int separ TSRMLS_DC);
UT_SYMBOL int ut_cut_at_space(char *p);
UT_SYMBOL zend_string *ut_pathUniqueID(char prefix, zend_string *path, time_t *mtp  TSRMLS_DC);
UT_SYMBOL void ut_compute_crc32(const zend_string *input, char *output TSRMLS_DC);

UT_SYMBOL int MINIT_utils(TSRMLS_D);
UT_SYMBOL int MSHUTDOWN_utils(TSRMLS_D);
UT_SYMBOL inline int RINIT_utils(TSRMLS_D);
UT_SYMBOL inline int RSHUTDOWN_utils(TSRMLS_D);

/*============================================================================*/
#endif	/* __PECL_UTILS_H */
