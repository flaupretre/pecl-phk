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
/* Generic arginfo structures */

static ZEND_BEGIN_ARG_INFO_EX(UT_noarg_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_noarg_ref_arginfo, 0, 1, 0)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_1arg_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, arg1)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_1arg_ref_arginfo, 0, 1, 1)
ZEND_ARG_INFO(0, arg1)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_2args_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_2args_ref_arginfo, 0, 1, 2)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_3args_arginfo, 0, 0, 3)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_ARG_INFO(0, arg3)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_3args_ref_arginfo, 0, 1, 3)
ZEND_ARG_INFO(0, arg1)
ZEND_ARG_INFO(0, arg2)
ZEND_ARG_INFO(0, arg3)
ZEND_END_ARG_INFO()

/*============================================================================*/

static inline void *_allocate(void *ptr, size_t size, int persistent)
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
	
#define eallocate(ptr, size) _allocate(ptr, size, 0)
#define pallocate(ptr, size) _allocate(ptr, size, 1)

#define EALLOCATE(ptr, size)	ptr=eallocate(ptr, size)
#define PALLOCATE(ptr, size)	ptr=pallocate(ptr, size)

/*---------------------------------------------------------------*/

static inline void *_duplicate(void *ptr, size_t size, int persistent)
{
	char *p;

	if (!ptr) return NULL;
	if (size==0) return _allocate(NULL,1,persistent);

	p=_allocate(NULL,size,persistent);
	memmove(p,ptr,size);
	return p;
}

#define eduplicate(ptr, size) _duplicate(ptr, size, 0)
#define pduplicate(ptr, size) _duplicate(ptr, size, 1)

/*---------------------------------------------------------------*/

#ifdef UT_DEBUG

#ifdef HAVE_GETTIMEOFDAY
static struct timeval base_tp;
#endif

/*------------*/

static void dbg_init_time()
{
#ifdef HAVE_GETTIMEOFDAY
	struct timezone tz;

	(void)gettimeofday(&base_tp,&tz);
#endif
}

/*------------*/

static inline void dbg_print_time()
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
	php_printf("%ld> : ",tp.tv_usec-base_tp.tv_usec);
	(void)gettimeofday(&base_tp,&tz);
#endif
}

#endif	/* UT_DEBUG */

/*---------------------------------------------------------------*/

static inline int ut_is_web()
{
	static int init_done=0;
	static int web;

	if (!init_done)	web=strcmp(sapi_module.name, "cli");
	return web;
}

/*---------*/

static void ut_persistent_zval_dtor(zval * zvalue)
{
	switch (zvalue->type & ~IS_CONSTANT_INDEX) {
	  case IS_STRING:
	  case IS_CONSTANT:
		  pefree(Z_STRVAL_P(zvalue), 1);
		  break;

	  case IS_ARRAY:
	  case IS_CONSTANT_ARRAY:
		  zend_hash_destroy(Z_ARRVAL_P(zvalue));
		  pefree(Z_ARRVAL_P(zvalue), 1);
		  break;
	}
}

/*---------*/

static void ut_persistent_zval_ptr_dtor(zval ** zval_ptr)
{
	(*zval_ptr)->refcount--;
	if ((*zval_ptr)->refcount == 0) {
		ut_persistent_zval_dtor(*zval_ptr);
		pefree(*zval_ptr, 1);
	} else {
		if ((*zval_ptr)->refcount == 1)
			(*zval_ptr)->is_ref = 0;
	}
}

/*---------*/

static void ut_persistent_array_init(zval * zp)
{
	HashTable *htp=NULL;

	INIT_PZVAL(zp);

	PALLOCATE(htp,sizeof(HashTable));
	(void)zend_hash_init(htp,0, NULL,(dtor_func_t)ut_persistent_zval_ptr_dtor,1);
	ZVAL_ARRAY_P(zp, htp);
}

/*---------*/

static void ut_persistent_copy_ctor(zval ** ztpp)
{
	zval *zsp;

	zsp = *ztpp;

	(*ztpp) = pallocate(NULL,sizeof(zval));

	ut_persist_zval(zsp, *ztpp);
}

/*---------*/
/* Duplicates a zval and all its descendants to persistent storage */
/* Does not support objects and resources */

static void ut_persist_zval(zval * zsp, zval * ztp)
{
	int type, len;
	char *p;

	(*ztp) = (*zsp);
	INIT_PZVAL(ztp);

	switch (type = Z_TYPE_P(ztp)) {	/* Default: do nothing (when no additional data) */
	  case IS_STRING:
	  case IS_CONSTANT:
		  len = Z_STRLEN_P(zsp);
		  p = pallocate(NULL,len + 1);
		  memmove(p, Z_STRVAL_P(zsp), len + 1);
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
			  EXCEPTION_ABORT_1("Cannot make resources/objects persistent",
								NULL);
		  }
	}
}

/*---------------------------------------------------------------*/

static void ut_new_instance(zval ** ret_pp, zval * class_name,
							int construct, int nb_args,
							zval ** args TSRMLS_DC)
{
	zend_class_entry **ce;

	if (zend_lookup_class_ex(Z_STRVAL_P(class_name), Z_STRLEN_P(class_name)
							 , 0, &ce TSRMLS_CC) == FAILURE) {
		EXCEPTION_ABORT_1("%s: class does not exist",
						  Z_STRVAL_P(class_name));
	}

	MAKE_STD_ZVAL(*ret_pp);
	object_init_ex(*ret_pp, *ce);

	if (construct) {
		ut_call_user_function_void(*ret_pp, &CZVAL(__construct), nb_args,
								   args TSRMLS_CC);
	}
}

/*---------------------------------------------------------------*/

static inline void ut_call_user_function_void(zval * obj_zp, zval * func_zp,
									   int nb_args, zval ** args TSRMLS_DC)
{
	zval dummy_ret;

	ut_call_user_function(obj_zp, func_zp, &dummy_ret, nb_args,
						  args TSRMLS_CC);
	zval_dtor(&dummy_ret);		/* Discard return value */
}

/*---------------------------------------------------------------*/

static inline int ut_call_user_function_bool(zval * obj_zp, zval * func_zp,
									  int nb_args, zval ** args TSRMLS_DC)
{
	zval dummy_ret;
	int result;

	ut_call_user_function(obj_zp, func_zp, &dummy_ret, nb_args,
						  args TSRMLS_CC);
	result = zend_is_true(&dummy_ret);
	zval_dtor(&dummy_ret);

	return result;
}

/*---------------------------------------------------------------*/

static inline long ut_call_user_function_long(zval * obj_zp, zval * func_zp,
									   int nb_args, zval ** args TSRMLS_DC)
{
	zval ret;

	ut_call_user_function(obj_zp, func_zp, &ret, nb_args, args TSRMLS_CC);
	if (EG(exception))
		return 0;

	if (Z_TYPE(ret) != IS_LONG)
		convert_to_long((&ret));
	return Z_LVAL(ret);
}

/*---------------------------------------------------------------*/

static inline void ut_call_user_function_string(zval * obj_zp, zval * func_zp,
										 zval * ret, int nb_args,
										 zval ** args TSRMLS_DC)
{

	ut_call_user_function(obj_zp, func_zp, ret, nb_args, args TSRMLS_CC);
	if (EG(exception))
		return;

	if (Z_TYPE_P(ret) != IS_STRING)
		convert_to_string(ret);
}

/*---------------------------------------------------------------*/

static inline void ut_call_user_function_array(zval * obj_zp, zval * func_zp,
										zval * ret, int nb_args,
										zval ** args TSRMLS_DC)
{

	ut_call_user_function(obj_zp, func_zp, ret, nb_args, args TSRMLS_CC);
	if (EG(exception))
		return;

	if (Z_TYPE_P(ret) != IS_ARRAY) {
		THROW_EXCEPTION_2("%s method should return a string (type=%d)",
						  Z_STRVAL_P(func_zp), Z_TYPE_P(ret));
	}
}

/*---------------------------------------------------------------*/

static inline void ut_call_user_function(zval * obj_zp, zval * func_zp,
								  zval * ret, int nb_args,
								  zval ** args TSRMLS_DC)
{
	call_user_function(EG(function_table), &obj_zp, func_zp, ret, nb_args,
					   args TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static int ut_extension_loaded(char *name, int len TSRMLS_DC)
{
	return zend_hash_exists(&module_registry, name, len + 1);
}

/*---------------------------------------------------------------*/

#ifdef PHP_WIN32
#define _UT_LE_PREFIX "php_"
#else
#define _UT_LE_PREFIX
#endif

static void ut_load_extension(char *name, int len TSRMLS_DC)
{
	zval zv,*zp;
	char *p;
	int status;

	if (ut_extension_loaded(name, len TSRMLS_CC)) return;

	spprintf(&p,1024,_UT_LE_PREFIX "%s." PHP_SHLIB_SUFFIX,name);
	INIT_ZVAL(zv);
	zp=&zv;
	ZVAL_STRING(zp,p,0);

	status=ut_call_user_function_bool(NULL,&CZVAL(dl),1,&zp TSRMLS_CC);

	zval_dtor(zp);

	if (!status) THROW_EXCEPTION_1("%s: Cannot load extension",name);
}

/*---------------------------------------------------------------*/

static void ut_load_extensions(zval * extensions TSRMLS_DC)
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

static void ut_require(char *string, zval * ret TSRMLS_DC)
{
	char *p;

	spprintf(&p, 1024, "require '%s';", string);

	zend_eval_string(p, ret, "eval" TSRMLS_CC);

	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

static inline int ut_strings_are_equal(zval * zp1, zval * zp2 TSRMLS_DC)
{
	if ((!zp1) || (!zp2))
		return 0;

	if (Z_TYPE_P(zp1) != IS_STRING)
		convert_to_string(zp1);
	if (Z_TYPE_P(zp2) != IS_STRING)
		convert_to_string(zp2);

	if (Z_STRLEN_P(zp1) != Z_STRLEN_P(zp2))
		return 0;

	return (!memcmp(Z_STRVAL_P(zp1), Z_STRVAL_P(zp1), Z_STRLEN_P(zp1)));
}

/*---------------------------------------------------------------*/

static void ut_header(long response_code, char *string TSRMLS_DC)
{
	sapi_header_line ctr;

	ctr.response_code = response_code;
	ctr.line = string;
	ctr.line_len = strlen(string);

	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void ut_http_403_fail(TSRMLS_D)
{
	ut_header(403, "HTTP/1.0 403 Forbidden" TSRMLS_CC);

	ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void ut_http_404_fail(TSRMLS_D)
{
	ut_header(404, "HTTP/1.0 404 Not Found" TSRMLS_CC);

	ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void ut_exit(int status TSRMLS_DC)
{
	EG(exit_status) = status;
	zend_bailout();
}

/*---------------------------------------------------------------*/

static inline zval *_ut_SERVER_element(HKEY_STRUCT * hkey TSRMLS_DC)
{
	zval **array, **token;

	if ((FIND_HKEY(&EG(symbol_table), _SERVER, &array) == SUCCESS)
		&& (Z_TYPE_PP(array) == IS_ARRAY)
		&&
		(zend_hash_quick_find
		 (Z_ARRVAL_PP(array), hkey->string, hkey->len, hkey->hash,
		  (void **) (&token)) == SUCCESS)) {
		return *token;
	}
	return NULL;
}

/*---------------------------------------------------------------*/

static inline zval *_ut_REQUEST_element(HKEY_STRUCT * hkey TSRMLS_DC)
{
	zval **array, **token;

	if ((FIND_HKEY(&EG(symbol_table), _REQUEST, &array) == SUCCESS)
		&& (Z_TYPE_PP(array) == IS_ARRAY)
		&&
		(zend_hash_quick_find
		 (Z_ARRVAL_PP(array), hkey->string, hkey->len, hkey->hash,
		  (void **) (&token)) == SUCCESS)) {
		return *token;
	}
	return NULL;
}

/*---------------------------------------------------------------*/

static char *ut_http_base_url(TSRMLS_D)
{
	zval *pathinfo, *php_self;
	int ilen, slen, nslen;
	static char buffer[UT_PATH_MAX];

	if (!ut_is_web())
		return "";

	if (!(pathinfo = SERVER_ELEMENT(PATH_INFO)))
		return Z_STRVAL_P(SERVER_ELEMENT(PHP_SELF));

	ilen = Z_STRLEN_P(pathinfo);

	php_self = SERVER_ELEMENT(PHP_SELF);
	slen = Z_STRLEN_P(php_self);

	nslen = slen - ilen;
	if ((nslen > 0)
		&&
		(!memcmp
		 (Z_STRVAL_P(pathinfo), Z_STRVAL_P(php_self) + nslen, ilen))) {
		if (nslen >= UT_PATH_MAX)
			nslen = UT_PATH_MAX - 1;
		memmove(buffer, Z_STRVAL_P(php_self), nslen);
		buffer[nslen] = '\0';
		return buffer;
	}

	return Z_STRVAL_P(php_self);
}

/*---------------------------------------------------------------*/

static void ut_http_301_redirect(zval * path, int must_free TSRMLS_DC)
{
	char *p;

	spprintf(&p, UT_PATH_MAX, "Location: http://%s%s%s",
			 Z_STRVAL_P(SERVER_ELEMENT(HTTP_HOST)),
			 ut_http_base_url(TSRMLS_C)
			 , Z_STRVAL_P(path));

	ut_header(301, p TSRMLS_CC);
	efree(p);

	ut_header(301, "HTTP/1.1 301 Moved Permanently" TSRMLS_CC);

	if (must_free)
		zval_dtor(path);
	ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static inline void ut_rtrim_zval(zval * zp TSRMLS_DC)
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

static inline void ut_tolower(char *p, int len TSRMLS_DC)
{
	int i;

	if (!len) return;
	for (i=0;i<len;i++,p++) {
		if (((*p) >= 'A') && ((*p) <= 'Z')) (*p) += ('a' - 'A');
	}
}

/*---------------------------------------------------------------*/

static inline void ut_file_suffix(zval * path, zval * ret TSRMLS_DC)
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
		if ((*p) == '/')
			break;
	}

	if (!found) {
		ZVAL_STRINGL(ret, "", 0, 1);
	} else {
		ZVAL_STRINGL(ret, p + 1, suffix_len, 1);
		ut_tolower(Z_STRVAL_P(ret),suffix_len TSRMLS_CC);
	}
}

/*---------------------------------------------------------------*/

static void ut_unserialize_zval(const unsigned char *buffer
	, unsigned long len, zval *ret TSRMLS_DC)
{
	php_unserialize_data_t var_hash;

	if (len==0) {
		THROW_EXCEPTION("Empty buffer");
		return;
		}

	PHP_VAR_UNSERIALIZE_INIT(var_hash);

	if (!php_var_unserialize(&ret,&buffer,buffer+len,&var_hash TSRMLS_CC)) {
		zval_dtor(ret);
		THROW_EXCEPTION("Unserialize error");
		}
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
}

/*---------------------------------------------------------------*/
/* Basic (and fast) file_get_contents(). Ignores safe mode and magic quotes */

static void ut_file_get_contents(char *path, zval *ret TSRMLS_DC)
{
	php_stream *stream;
	char *contents;
	int len;

	stream=php_stream_open_wrapper(path,"rb",REPORT_ERRORS,NULL);
	if (!stream) EXCEPTION_ABORT_1("%s : Cannot open file",path);

	len=php_stream_copy_to_mem(stream,&contents,PHP_STREAM_COPY_ALL,0);
	php_stream_close(stream);

	if (len < 0) EXCEPTION_ABORT_1("%s : Cannot read file",path);

	ZVAL_STRINGL(ret,contents,len,0);
}

/*---------------------------------------------------------------*/

static char *ut_htmlspecialchars(char *src, int srclen, int *retlen TSRMLS_DC)
{
	int dummy;

	if (!retlen) retlen = &dummy;

	return php_escape_html_entities(src,srclen,retlen,0,ENT_COMPAT,NULL TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static char *ut_ucfirst(char *ptr, int len TSRMLS_DC)
{
	char *p2;

	if (!ptr) return NULL;

	p2=eduplicate(ptr,len+1);

	if (((*p2) >= 'a') && ((*p2) <= 'z')) (*p2) -= 'a' - 'A';

	return p2;
}

/*---------------------------------------------------------------*/

static void ut_repeat_printf(char c, int count TSRMLS_DC)
{
	char *p;

	if (!count) return;

	p=eallocate(NULL,count);
	memset(p,c,count);
	PHPWRITE(p,count);
	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

static void ut_printf_pad_right(char *str, int len, int size TSRMLS_DC)
{
	char *p;

	if (len >= size) {
		php_printf("%s",str);
		return;
	}

	p=eallocate(NULL,size);
	memset(p,' ',size);
	memmove(p,str,len);
	PHPWRITE(p,size);
	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

static void ut_printf_pad_both(char *str, int len, int size TSRMLS_DC)
{
	int pad;
	char *p;

	if (len >= size) {
		php_printf("%s",str);
		return;
	}

	p=eallocate(NULL,size);
	memset(p,' ',size);
	pad=(size-len)/2;
	memmove(p+pad,str,len);
	PHPWRITE(p,size);
	EALLOCATE(p,0);
}

/*---------------------------------------------------------------*/

static char *ut_absolute_dirname(char *path, int len, int *reslen, int separ TSRMLS_DC)
{
	char *dir,*res;
	int dlen;

	dir=ut_dirname(path,len,&dlen TSRMLS_CC);
	res=ut_mk_absolute_path(dir,dlen,reslen,separ TSRMLS_CC);
	EALLOCATE(dir,0);
	return res;
}

/*---------------------------------------------------------------*/

static char *ut_dirname(char *path, int len, int *reslen TSRMLS_DC)
{
	char *p;

	p=eduplicate(path,len+1);
	(*reslen)=php_dirname(p,len);
	return p;
}

/*---------------------------------------------------------------*/

static inline int ut_is_uri(char *path, int len TSRMLS_DC)
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

static char *ut_mk_absolute_path(char *path, int len, int *reslen
	, int separ TSRMLS_DC)
{
	char buf[1024];
	char *resp,*p;
	size_t clen;
	int dummy_reslen;

	if (!reslen) reslen=&dummy_reslen;

	if (ut_is_uri(path,len TSRMLS_CC)) {
		resp=eallocate(NULL,len+2);
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
		resp=eallocate(NULL,len+2);
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
	resp=eallocate(NULL,clen+len+3);
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

static int ut_rtrim(char *p TSRMLS_DC)
{
	int i;

	for (i=0;;i++,p++) {
		if ((!(*p))||((*p)==' ')||((*p)=='\t')) {
			(*p)='\0';
			break;
		}
	}
	return i;
}

/*========================================================================*/

static void ut_build_constants(TSRMLS_D)
{
	INIT_CZVAL(__construct);
	INIT_CZVAL(dl);

	INIT_HKEY(_SERVER);
	INIT_HKEY(_REQUEST);
	INIT_HKEY(PATH_INFO);
	INIT_HKEY(PHP_SELF);
	INIT_HKEY(HTTP_HOST);
}

/*---------------------------------------------------------------*/

static int MINIT_utils(TSRMLS_D)
{
	ut_build_constants(TSRMLS_C);

	return SUCCESS;
}

/*-------------*/

static int MSHUTDOWN_utils(TSRMLS_D)
{
	return SUCCESS;
}

/*-------------*/

static inline int RINIT_utils(TSRMLS_D)
{
	return SUCCESS;
}

/*-------------*/

static inline int RSHUTDOWN_utils(TSRMLS_D)
{
	return SUCCESS;
}

/*============================================================================*/
