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

#ifndef FLP_UTILS_H
#define FLP_UTILS_H 1
/*============================================================================*/

#include "php.h"
#include "zend_exceptions.h"
#include "zend_hash.h"

#ifdef ALLOCATE
#define GLOBAL
#else
#define GLOBAL extern
#endif

#ifndef NULL
#define NULL (char *)0
#endif

typedef struct
	{
	char *string;
	unsigned int len;
	ulong hash;
	} HKEY_STRUCT;

#define CZVAL(name) ( czval_ ## name )

#define DECLARE_CZVAL(name)	zval CZVAL(name)

#define INIT_CZVAL(name)	\
	{ \
	INIT_ZVAL(CZVAL(name)); \
	ZVAL_STRING(&(CZVAL(name)), #name,0); \
	}

#define HKEY(name) ( hkey_ ## name )

#define DECLARE_HKEY(name)	HKEY_STRUCT HKEY(name)

#define INIT_HKEY(name) \
	{ \
	HKEY(name).string= #name ; \
	HKEY(name).len=sizeof( #name ); \
	HKEY(name).hash=zend_get_hash_value(HKEY(name).string,HKEY(name).len); \
	}

#define FIND_HKEY(ht,name,respp) \
	zend_hash_quick_find(ht,HKEY(name).string \
		,HKEY(name).len,HKEY(name).hash,(void **)(respp))

#define HKEY_EXISTS(ht,name) \
	zend_hash_quick_exists(ht,HKEY(name).string, HKEY(name).len,HKEY(name).hash)

#define SERVER_ELEMENT(name) _ut_SERVER_element(&HKEY(name) TSRMLS_CC)

#define REQUEST_ELEMENT(name) _ut_REQUEST_element(&HKEY(name) TSRMLS_CC)

/*---------------------------------------------------------------*/
/* (Taken from pcre/pcrelib/internal.h) */
/* To cope with SunOS4 and other systems that lack memmove() but have bcopy(),
define a macro for memmove() if HAVE_MEMMOVE is false, provided that HAVE_BCOPY
is set. Otherwise, include an emulating function for those systems that have
neither (there are some non-Unix environments where this is the case). This
assumes that all calls to memmove are moving strings upwards in store,
which is the case in this extension. */

#if ! HAVE_MEMMOVE
#undef  memmove        /* some systems may have a macro */
#if HAVE_BCOPY
#define memmove(a, b, c) bcopy(b, a, c)
#else  /* HAVE_BCOPY */
static void * phk_memmove(unsigned char *dest, const unsigned char *src, size_t n)
{
int i;
dest += n;
src += n;
for (i = 0; i < n; ++i) *(--dest) =  *(--src);
}
#define memmove(a, b, c) phk_memmove(a, b, c)
#endif   /* not HAVE_BCOPY */
#endif   /* not HAVE_MEMMOVE */

/*---------------------------------------------------------------*/

#ifdef UT_DEBUG
#define DBG_MSG(_format) php_printf(_format "\n")
#define DBG_MSG1(_format,_var1) php_printf(_format "\n",_var1)
#define DBG_MSG2(_format,_var1,_var2) php_printf(_format "\n",_var1,_var2)
#define DBG_MSG3(_format,_var1,_var2,_var3) php_printf(_format "\n",_var1,_var2,_var3)
#else
#define DBG_MSG(_format)
#define DBG_MSG1(_format,_var1)
#define DBG_MSG2(_format,_var1,_var2)
#define DBG_MSG3(_format,_var1,_var2,_var3)
#endif

#define THROW_EXCEPTION_1(_format,_arg1)	\
	{ \
	(void)zend_throw_exception_ex(NULL,0 TSRMLS_CC ,_format,_arg1); \
	}

#define THROW_EXCEPTION_2(_format,_arg1,_arg2)	\
	{ \
	(void)zend_throw_exception_ex(NULL,0 TSRMLS_CC ,_format,_arg1,_arg2); \
	}

#define EXCEPTION_ABORT_1(_format,_arg1)	\
	{ \
	THROW_EXCEPTION_1(_format,_arg1); \
	return; \
	}

#define EXCEPTION_ABORT_RET_1(_ret,_format,_arg1)	\
	{ \
	THROW_EXCEPTION_1(_format,_arg1); \
	return _ret; \
	}

#define ZSTRING_HASH(zp) \
	zend_get_hash_value(Z_STRVAL_P((zp)),Z_STRLEN_P((zp))+1)

#define UT_PATH_MAX 1023

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

/*----- These could go to zend.h --------*/

#define ALLOC_PERSISTENT_ZVAL(zv) (zv)=pemalloc(sizeof(zval),1)

#define MAKE_STD_PERSISTENT_ZVAL(zv) \
	ALLOC_PERSISTENT_ZVAL(zv); \
	INIT_PZVAL(zv);

#ifndef ZVAL_ARRAY /*--------------*/

#define ZVAL_ARRAY_P(zp,ht) ZVAL_ARRAY((*(zp)),ht)
#define ZVAL_ARRAY_PP(zpp,ht) ZVAL_ARRAY((**(zpp)),ht)

#define ZVAL_ARRAY(zv,ht) \
	Z_TYPE(zv)=IS_ARRAY; \
	Z_ARRVAL(zv)=ht;

#endif /* ZVAL_ARRAY --------------*/

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

/* Note: return by ref does not work because, after the call, the refcount is
systematically reset to 1. Workaround: Return a copy (slower).
This is a bug present at least in 5.2.4. Bug is
at line 1005 in zend_execute_API.c 'v 1.331.2.20.2.24 2007/07/21'. The bug
is corrected in CVS (5.3 branch). When we can test for a release version, we
will make it conditional. */

#define RETURN_BY_REF_IS_BROKEN

#ifdef RETURN_BY_REF_IS_BROKEN
#define RETVAL_BY_REF(zp) \
	{ \
	**return_value_ptr=*zp; \
	INIT_PZVAL(*return_value_ptr); \
	zval_copy_ctor(*return_value_ptr); \
	}
#else
#define RETVAL_BY_REF(zp) \
	{ \
	zval_ptr_dtor(return_value_ptr); \
	ZVAL_ADDREF(zp); \
	*return_value_ptr=(zp); \
	}
#endif

/*-- Thread-safe stuff ------*/

#ifdef ZTS
#define MutexDeclare(x) MUTEX_T x
#define MutexSetup(x) x = tsrm_mutex_alloc()
#define MutexShutdown(x) tsrm_mutex_free(x)
#define MutexLock(x) tsrm_mutex_lock(x)
#define MutexUnlock(x) tsrm_mutex_unlock(x)
#else
#define MutexDeclare(x)
#define MutexSetup(x)
#define MutexShutdown(x)
#define MutexLock(x)
#define MutexUnlock(x)
#endif

/*============================================================================*/

static DECLARE_CZVAL(__construct);

/*============================================================================*/
#endif	/* FLP_UTILS_H */
