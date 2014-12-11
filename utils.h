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

#include "php.h"
#include "zend_exceptions.h"
#include "zend_hash.h"
#include "TSRM/TSRM.h"

#ifndef NULL
#	define NULL (char *)0
#endif

#ifdef UT_PRIVATE_SYMBOLS
#	define UT_SYMBOL static
#else
#	define UT_SYMBOL
#endif

/*----------------*/
/* Compatibility macros - Allow using these macros with PHP 5.1 and more */

#ifndef Z_ADDREF
#define Z_ADDREF_P(_zp)		ZVAL_ADDREF(_zp)
#define Z_ADDREF(_z)		Z_ADDREF_P(&(_z))
#define Z_ADDREF_PP(ppz)	Z_ADDREF_P(*(ppz))
#endif

#ifndef Z_DELREF_P
#define Z_DELREF_P(_zp)		ZVAL_DELREF(_zp)
#define Z_DELREF(_z)		Z_DELREF_P(&(_z))
#define Z_DELREF_PP(ppz)	Z_DELREF_P(*(ppz))
#endif

#ifndef Z_REFCOUNT_P
#define Z_REFCOUNT_P(_zp)	ZVAL_REFCOUNT(_zp)
#define Z_REFCOUNT_PP(_zpp)	ZVAL_REFCOUNT(*(_zpp))
#endif

#ifndef Z_UNSET_ISREF_P
#define Z_UNSET_ISREF_P(_zp)	{ (_zp)->is_ref=0; }
#endif

#ifndef ZVAL_COPY_VALUE
#define ZVAL_COPY_VALUE(z, v) \
	do { \
		(z)->value = (v)->value; \
		Z_TYPE_P(z) = Z_TYPE_P(v); \
	} while (0)
#endif

#ifndef ALLOC_PERMANENT_ZVAL
#define ALLOC_PERMANENT_ZVAL(z)		{ z=ut_pallocate(NULL, sizeof(zval)); }
#endif

#ifndef GC_REMOVE_ZVAL_FROM_BUFFER
#define GC_REMOVE_ZVAL_FROM_BUFFER(z)
#endif

#ifndef INIT_PZVAL_COPY
#define INIT_PZVAL_COPY(z, v) \
	do { \
		INIT_PZVAL(z); \
		ZVAL_COPY_VALUE(z, v); \
	} while (0)
#endif

/*----------------*/

#define CZVAL(name) ( czval_ ## name )

#define DECLARE_CZVAL(name)	zval CZVAL(name)

#define INIT_CZVAL(name)	INIT_CZVAL_VALUE(name, #name)

#define INIT_CZVAL_VALUE(name,value)	\
	{ \
	INIT_ZVAL(CZVAL(name)); \
	ZVAL_STRING(&(CZVAL(name)), value,0); \
	}

typedef struct {
	char *string;
	unsigned int len;
	ulong hash;
} HKEY_STRUCT;

#define HKEY(name) ( hkey_ ## name )

#define HKEY_STRING(name)	HKEY(name).string
#define HKEY_LEN(name)		HKEY(name).len
#define HKEY_HASH(name)		HKEY(name).hash

#define DECLARE_HKEY(name)	HKEY_STRUCT HKEY(name)

#define INIT_HKEY(name) INIT_HKEY_VALUE(name, #name)

#define INIT_HKEY_VALUE(name,value) \
	{ \
	HKEY_STRING(name)= value ; \
	HKEY_LEN(name)=sizeof( value ); \
	HKEY_HASH(name)=zend_get_hash_value(HKEY_STRING(name),HKEY_LEN(name)); \
	}

#define FIND_HKEY(ht,name,respp) \
	zend_hash_quick_find(ht,HKEY_STRING(name) \
		,HKEY_LEN(name),HKEY_HASH(name),(void **)(respp))

#define HKEY_EXISTS(ht,name) \
	zend_hash_quick_exists(ht,HKEY_STRING(name), HKEY_LEN(name),HKEY_HASH(name))

#define SERVER_ELEMENT(name) _ut_SERVER_element(&HKEY(name) TSRMLS_CC)

#define REQUEST_ELEMENT(name) _ut_REQUEST_element(&HKEY(name) TSRMLS_CC)

#ifndef MIN
#	define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#	define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define CLEAR_DATA(_v)	memset(&(_v),'\0',sizeof(_v)); \

/*---------------------------------------------------------------*/
/* (Taken from pcre/pcrelib/internal.h) */
/* To cope with SunOS4 and other systems that lack memmove() but have bcopy(),
define a macro for memmove() if HAVE_MEMMOVE is false, provided that HAVE_BCOPY
is set. Otherwise, include an emulating function for those systems that have
neither (there are some non-Unix environments where this is the case). This
assumes that all calls to memmove are moving strings upwards in store,
which is the case in this extension. */

#if ! HAVE_MEMMOVE
#	undef  memmove					/* some systems may have a macro */
#	if HAVE_BCOPY
#		define memmove(a, b, c) bcopy(b, a, c)
#	else							/* HAVE_BCOPY */
		static void *my_memmove(unsigned char *dest, const unsigned char *src,
								size_t n)
		{
			int i;

			dest += n;
			src += n;
			for (i = 0; i < n; ++i)
				*(--dest) = *(--src);
		}
#		define memmove(a, b, c) my_memmove(a, b, c)
#	endif	/* not HAVE_BCOPY */
#endif		/* not HAVE_MEMMOVE */

#ifdef _AIX
#	undef PHP_SHLIB_SUFFIX
#	define PHP_SHLIB_SUFFIX "a"
#endif

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

#define ZSTRING_HASH(zp) \
	zend_get_hash_value(Z_STRVAL_P((zp)),Z_STRLEN_P((zp))+1)

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

#ifndef ZVAL_ARRAY /*--------------*/

#define ZVAL_ARRAY(zp,ht) \
	Z_TYPE_P(zp)=IS_ARRAY; \
	Z_ARRVAL_P(zp)=ht;

#endif /* ZVAL_ARRAY -------------- */

#ifndef ZVAL_IS_ARRAY
#define ZVAL_IS_ARRAY(zp)	(Z_TYPE_P((zp))==IS_ARRAY)
#endif

#ifndef ZVAL_IS_STRING
#define ZVAL_IS_STRING(zp)	(Z_TYPE_P((zp))==IS_STRING)
#endif

#ifndef ZVAL_IS_LONG
#define ZVAL_IS_LONG(zp)	(Z_TYPE_P((zp))==IS_LONG)
#endif

#ifndef ZVAL_IS_BOOL
#define ZVAL_IS_BOOL(zp)	(Z_TYPE_P((zp))==IS_BOOL)
#endif

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
/* Compatibility */

#if PHP_API_VERSION >= 20100412
/* PHP 5.4+ */
	typedef size_t PHP_ESCAPE_HTML_ENTITIES_SIZE;
#else
	typedef int PHP_ESCAPE_HTML_ENTITIES_SIZE;
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
	int construct, int nb_args,	zval ** args TSRMLS_DC);
UT_SYMBOL inline void ut_call_user_function_void(zval *obj_zp, char *func,
	int func_len, int nb_args, zval ** args TSRMLS_DC);
UT_SYMBOL inline int ut_call_user_function_bool(zval * obj_zp, char *func,
	int func_len, int nb_args, zval ** args TSRMLS_DC);
UT_SYMBOL inline long ut_call_user_function_long(zval *obj_zp, char *func,
	int func_len, int nb_args, zval ** args TSRMLS_DC);
UT_SYMBOL inline void ut_call_user_function_string(zval *obj_zp, char *func,
	int func_len, zval * ret, int nb_args, zval ** args TSRMLS_DC);
UT_SYMBOL inline void ut_call_user_function_array(zval * obj_zp, char *func,
	int func_len, zval * ret, int nb_args, zval ** args TSRMLS_DC);
UT_SYMBOL inline void ut_call_user_function(zval *obj_zp, char *func,
	int func_len, zval *ret, int nb_args, zval ** args TSRMLS_DC);
UT_SYMBOL int ut_extension_loaded(char *name, int len TSRMLS_DC);
UT_SYMBOL void ut_load_extension_file(zval *file TSRMLS_DC);
UT_SYMBOL void ut_load_extension(char *name, int len TSRMLS_DC);
UT_SYMBOL void ut_load_extensions(zval * extensions TSRMLS_DC);
UT_SYMBOL void ut_require(char *string, zval * ret TSRMLS_DC);
UT_SYMBOL inline int ut_strings_are_equal(zval * zp1, zval * zp2 TSRMLS_DC);
UT_SYMBOL void ut_header(long response_code, char *string TSRMLS_DC);
UT_SYMBOL void ut_http_403_fail(TSRMLS_D);
UT_SYMBOL void ut_http_404_fail(TSRMLS_D);
UT_SYMBOL void ut_exit(int status TSRMLS_DC);
UT_SYMBOL inline zval *_ut_SERVER_element(HKEY_STRUCT * hkey TSRMLS_DC);
UT_SYMBOL inline zval *_ut_REQUEST_element(HKEY_STRUCT * hkey TSRMLS_DC);
UT_SYMBOL char *ut_http_base_url(TSRMLS_D);
UT_SYMBOL void ut_http_301_redirect(char *path, int must_free TSRMLS_DC);
UT_SYMBOL inline void ut_rtrim_zval(zval * zp TSRMLS_DC);
UT_SYMBOL inline void ut_tolower(char *p, int len TSRMLS_DC);
UT_SYMBOL inline void ut_file_suffix(zval * path, zval * ret TSRMLS_DC);
UT_SYMBOL void ut_unserialize_zval(const unsigned char *buffer
	, unsigned long len, zval *ret TSRMLS_DC);
UT_SYMBOL void ut_file_get_contents(char *path, zval *ret TSRMLS_DC);
UT_SYMBOL char *ut_htmlspecialchars(char *src, int srclen, PHP_ESCAPE_HTML_ENTITIES_SIZE *retlen TSRMLS_DC);
UT_SYMBOL char *ut_ucfirst(char *ptr, int len TSRMLS_DC);
UT_SYMBOL void ut_repeat_printf(char c, int count TSRMLS_DC);
UT_SYMBOL void ut_printf_pad_right(char *str, int len, int size TSRMLS_DC);
UT_SYMBOL void ut_printf_pad_both(char *str, int len, int size TSRMLS_DC);
UT_SYMBOL char *ut_absolute_dirname(char *path, int len, int *reslen, int separ TSRMLS_DC);
UT_SYMBOL char *ut_dirname(char *path, int len, int *reslen TSRMLS_DC);
UT_SYMBOL inline int ut_is_uri(char *path, int len TSRMLS_DC);
UT_SYMBOL char *ut_mk_absolute_path(char *path, int len, int *reslen
	, int separ TSRMLS_DC);
UT_SYMBOL int ut_cut_at_space(char *p);
UT_SYMBOL void ut_path_unique_id(char prefix, zval * path, zval ** mnt
	, time_t *mtp  TSRMLS_DC);

UT_SYMBOL int MINIT_utils(TSRMLS_D);
UT_SYMBOL int MSHUTDOWN_utils(TSRMLS_D);
UT_SYMBOL inline int RINIT_utils(TSRMLS_D);
UT_SYMBOL inline int RSHUTDOWN_utils(TSRMLS_D);

/*============================================================================*/
#endif	/* __PECL_UTILS_H */
