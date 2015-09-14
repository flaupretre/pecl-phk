/*
  +----------------------------------------------------------------------+
  | Compatibility macros for different PHP versions                      |
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

#ifndef _COMPAT_MISC_H
#define _COMPAT_MISC_H

#ifndef NULL
#	define NULL (char *)0
#endif

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

#ifndef IS_CONSTANT_ARRAY
#define IS_CONSTANT_ARRAY IS_CONSTANT_AST
#endif

#ifndef IS_CONSTANT_TYPE_MASK
#define IS_CONSTANT_TYPE_MASK (~IS_CONSTANT_INDEX)
#endif

#ifndef MIN
#	define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#	define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

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

#ifndef ZVAL_ARRAY
#define ZVAL_ARRAY(zp,ht) \
	Z_TYPE_P(zp)=IS_ARRAY; \
	Z_ARRVAL_P(zp)=ht;
#endif

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

#ifndef MAKE_STD_ZVAL
#define MAKE_STD_ZVAL(zp) { zp=ut_eallocate(NULL,sizeof(zval)); INIT_ZVAL(*zp); }
#endif

#ifndef INIT_ZVAL
#define INIT_ZVAL(z) CLEAR_DATA(z)
#endif

#ifndef ALLOC_INIT_ZVAL
#define ALLOC_INIT_ZVAL(zp) MAKE_STD_ZVAL(zp)
#endif

/*--------------*/

#define PHP_5_0_X_API_NO                220040412
#define PHP_5_1_X_API_NO                220051025
#define PHP_5_2_X_API_NO                220060519
#define PHP_5_3_X_API_NO                220090626
#define PHP_5_4_X_API_NO                220100525
#define PHP_5_5_X_API_NO                220121212
#define PHP_5_6_X_API_NO                220131226

#if PHP_API_VERSION >= 20100412
	typedef size_t PHP_ESCAPE_HTML_ENTITIES_SIZE;
#else
	typedef int PHP_ESCAPE_HTML_ENTITIES_SIZE;
#endif

/*------------------------------------------------------------*/
#endif /* _COMPAT_MISC_H */
/*------------------------------------------------------------*/
