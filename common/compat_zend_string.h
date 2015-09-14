/*
  +----------------------------------------------------------------------+
  | zend_string compatibility layer for PHP 5                            |
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

#ifndef _COMPAT_ZEND_STRING_H
#define _COMPAT_ZEND_STRING_H

#ifdef PHP_7
#include "zend_string.h"
#else
/*---- zend_string for PHP 5 ----*/

typedef struct _zend_string zend_string;

struct _zend_string {
		char		persistent;
        zend_ulong	h; /* hash value */
        size_t		len;
        char		val[1];
};

/* Shortcuts */

#define ZSTR_VAL(zstr)  (zstr)->val
#define ZSTR_LEN(zstr)  (zstr)->len
#define ZSTR_H(zstr)    (zstr)->h
#define ZSTR_HASH(zstr) zend_string_hash_val(zstr)

#define _ZSTR_HEADER_SIZE ((int)(&(((zend_string*)0)->val)))
#define _ZSTR_STRUCT_SIZE(len) (_ZSTR_HEADER_SIZE + len + 1)

/*---------*/

static zend_always_inline zend_ulong zend_string_hash_val(zend_string *s)
{
	if (!ZSTR_H(s)) {
		ZSTR_H(s) = zend_get_hash_value(ZSTR_VAL(s), ZSTR_LEN(s)+1);
	}
	return ZSTR_H(s);
}

/*---------*/

static zend_always_inline void zend_string_forget_hash_val(zend_string *s)
{
	ZSTR_H(s) = 0;
}

/*---------*/

static zend_always_inline zend_string *zend_string_alloc(size_t len, int persistent)
{
	zend_string *ret = (zend_string *)pemalloc(ZEND_MM_ALIGNED_SIZE(_ZSTR_STRUCT_SIZE(len)), persistent);
	ret->persistent = (persistent ? (char)1 : (char)0);
	zend_string_forget_hash_val(ret);
	ZSTR_LEN(ret) = len;
	return ret;
}

/*---------*/

static zend_always_inline zend_string *zend_string_init(const char *str, size_t len, int persistent)
{
	zend_string *ret = zend_string_alloc(len, persistent);

	memcpy(ZSTR_VAL(ret), str, len);
	ZSTR_VAL(ret)[len] = '\0';
	return ret;
}

/*---------*/

static zend_always_inline zend_string *zend_string_dup(zend_string *s, int persistent)
{
	return zend_string_init(ZSTR_VAL(s), ZSTR_LEN(s), persistent);
}

/*---------*/

static zend_always_inline zend_string *zend_string_realloc(zend_string *s, size_t len, int persistent)
{
	zend_string *ret;

	ret = (zend_string *)perealloc(s, ZEND_MM_ALIGNED_SIZE(_ZSTR_STRUCT_SIZE(len)), persistent);
	ZSTR_LEN(ret) = len;
	zend_string_forget_hash_val(ret);
	return ret;
}

/*---------*/

static zend_always_inline void zend_string_release(zend_string *s)
{
	if (s) pefree(s, s->persistent);
}

/*---------*/

static zend_always_inline zend_bool zend_string_equals(zend_string *s1, zend_string *s2)
{
	return s1 == s2 || (s1->len == s2->len && !memcmp(s1->val, s2->val, s1->len));
}

#define zend_string_equals_literal(str, literal) \
	((str)->len == sizeof(literal)-1 && !memcmp((str)->val, literal, sizeof(literal) - 1))

#endif /* PHP_7 */
#endif /* _COMPAT_ZEND_STRING_H */
