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

#include "php.h"
#include "zend_execute.h"
#include "SAPI.h"

#include "utils.h"

/*============================================================================*/

static void ut_persistent_copy_ctor(zval **ztpp);
static int MINIT_utils(TSRMLS_D);
static int MSHUTDOWN_utils(TSRMLS_D);

static int RINIT_utils(TSRMLS_D);
static int RSHUTDOWN_utils(TSRMLS_D);

static int  ut_is_web(void);
static void ut_persistent_zval_dtor(zval *zvalue);
static void ut_persistent_zval_ptr_dtor(zval **zval_ptr);
static void ut_persistent_array_init(zval *zp);
static void ut_persist_zval(zval *zsp,zval *ztp);
static void ut_new_instance(zval **ret_pp, zval *class_name, int construct
	, int num_args, zval **args TSRMLS_DC);

static void ut_call_user_function_void  (zval *obj_zp,zval *func_zp          ,int nb_args,zval **args TSRMLS_DC);
static int  ut_call_user_function_bool  (zval *obj_zp,zval *func_zp          ,int nb_args,zval **args TSRMLS_DC);
static long ut_call_user_function_long  (zval *obj_zp,zval *func_zp          ,int nb_args,zval **args TSRMLS_DC);
static void ut_call_user_function_string(zval *obj_zp,zval *func_zp,zval *ret,int nb_args,zval **args TSRMLS_DC);
static void ut_call_user_function_array (zval *obj_zp,zval *func_zp,zval *ret,int nb_args,zval **args TSRMLS_DC);
static void ut_call_user_function       (zval *obj_zp,zval *func_zp,zval *ret,int nb_args,zval **args TSRMLS_DC);

static int  ut_extension_loaded(char *name, int len TSRMLS_DC);
static void ut_require(char *string, zval *ret TSRMLS_DC);
static int  ut_strings_are_equal(zval *zp1, zval *zp2 TSRMLS_DC);
static void ut_header(char *string TSRMLS_DC);
static void ut_http_403_fail(TSRMLS_D);
static void ut_http_404_fail(TSRMLS_D);
static void ut_exit(int status TSRMLS_DC);
static zval *_ut_SERVER_element(HKEY_STRUCT *hkey TSRMLS_DC);
static zval *_ut_REQUEST_element(HKEY_STRUCT *hkey TSRMLS_DC);
static char *ut_http_base_url(TSRMLS_D);
static void ut_http_301_redirect(zval *path, int must_free TSRMLS_DC);
static void ut_rtrim_zval(zval *zp TSRMLS_DC);
static void ut_file_suffix(zval *path, zval *ret TSRMLS_DC);

/*============================================================================*/
/* Generic arginfo structures */

static ZEND_BEGIN_ARG_INFO_EX(UT_noarg_arginfo,0,0,0)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_noarg_ref_arginfo,0,1,0)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_1arg_arginfo,0,0,1)
ZEND_ARG_INFO(0,arg1)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_1arg_ref_arginfo,0,1,1)
ZEND_ARG_INFO(0,arg1)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_2args_arginfo,0,0,2)
ZEND_ARG_INFO(0,arg1)
ZEND_ARG_INFO(0,arg2)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(UT_2args_ref_arginfo,0,1,2)
ZEND_ARG_INFO(0,arg1)
ZEND_ARG_INFO(0,arg2)
ZEND_END_ARG_INFO()

/*============================================================================*/

static int ut_is_web()
{
return strcmp(sapi_module.name,"cli");
}

/*---------*/

static void ut_persistent_zval_dtor(zval *zvalue)
{
switch (zvalue->type & ~IS_CONSTANT_INDEX)
	{
	case IS_STRING:
	case IS_CONSTANT:
		pefree(Z_STRVAL_P(zvalue),1);
		break;

	case IS_ARRAY:
	case IS_CONSTANT_ARRAY:
		zend_hash_destroy(Z_ARRVAL_P(zvalue));
		pefree(Z_ARRVAL_P(zvalue),1);
		break;
	}
}

/*---------*/

static void ut_persistent_zval_ptr_dtor(zval **zval_ptr)
{
(*zval_ptr)->refcount--;
if ((*zval_ptr)->refcount==0)
	{
	ut_persistent_zval_dtor(*zval_ptr);
	pefree(*zval_ptr,1);
	}
else
	{
	if ((*zval_ptr)->refcount == 1) (*zval_ptr)->is_ref = 0;
	}
}

/*---------*/

static void ut_persistent_array_init(zval *zp)
{
HashTable *htp;

INIT_PZVAL(zp);

htp=pemalloc(sizeof(HashTable),1);
(void)zend_hash_init(htp,0,NULL,(dtor_func_t)ut_persistent_zval_ptr_dtor,1);
ZVAL_ARRAY_P(zp,htp);
}

/*---------*/

static void ut_persistent_copy_ctor(zval **ztpp)
{
zval *zsp;

zsp=*ztpp;

(*ztpp)=pemalloc(sizeof(zval),1);

ut_persist_zval(zsp,*ztpp);
}

/*---------*/
/* Duplicates a zval and all its descendants to persistent storage */
/* Does not support objects and resources */

static void ut_persist_zval(zval *zsp,zval *ztp)
{
int type,len;
char *p;

(*ztp)=(*zsp);
INIT_PZVAL(ztp);

switch(type=Z_TYPE_P(ztp)) /* Default: do nothing (when no additional data) */
	{
	case IS_STRING:
	case IS_CONSTANT:
		len=Z_STRLEN_P(zsp);
		p=pemalloc(len+1,1);
		memmove(p,Z_STRVAL_P(zsp),len+1);
		ZVAL_STRINGL(ztp,p,len,0);
		break;

	case IS_ARRAY:
	case IS_CONSTANT_ARRAY:
		ut_persistent_array_init(ztp);
		zend_hash_copy(Z_ARRVAL_P(ztp),Z_ARRVAL_P(zsp)
			,(copy_ctor_func_t)ut_persistent_copy_ctor,NULL,sizeof(zval *));
		Z_TYPE_P(ztp)=type;
		break;

	case IS_OBJECT:
	case IS_RESOURCE:
		{
		TSRMLS_FETCH();
		EXCEPTION_ABORT_1("Cannot make resources/objects persistent",NULL);
		}
	}
}

/*---------------------------------------------------------------*/

static void ut_new_instance(zval **ret_pp, zval *class_name, int construct
	, int nb_args, zval **args TSRMLS_DC)
{
zend_class_entry **ce;

if (zend_lookup_class_ex(Z_STRVAL_P(class_name),Z_STRLEN_P(class_name)
	,0,&ce TSRMLS_CC)==FAILURE)
	{
	EXCEPTION_ABORT_1("%s: class does not exist",Z_STRVAL_P(class_name));
	}

MAKE_STD_ZVAL(*ret_pp);
object_init_ex(*ret_pp,*ce);

if (construct)
	{
	ut_call_user_function_void(*ret_pp,&CZVAL(__construct),nb_args,args
		TSRMLS_CC);
	}
}

/*---------------------------------------------------------------*/

static void ut_call_user_function_void(zval *obj_zp,zval *func_zp,int nb_args
	,zval **args TSRMLS_DC)
{
zval dummy_ret;

ut_call_user_function(obj_zp,func_zp,&dummy_ret,nb_args,args TSRMLS_CC);
zval_dtor(&dummy_ret); /* Discard return value */
}

/*---------------------------------------------------------------*/

static int ut_call_user_function_bool(zval *obj_zp,zval *func_zp,int nb_args
	,zval **args TSRMLS_DC)
{
zval dummy_ret;
int result;

ut_call_user_function(obj_zp,func_zp,&dummy_ret,nb_args,args TSRMLS_CC);
result=zend_is_true(&dummy_ret);
zval_dtor(&dummy_ret);

return result;
}

/*---------------------------------------------------------------*/

static long ut_call_user_function_long(zval *obj_zp,zval *func_zp,int nb_args
	,zval **args TSRMLS_DC)
{
zval ret;

ut_call_user_function(obj_zp,func_zp,&ret,nb_args,args TSRMLS_CC);
if (EG(exception)) return 0;

if (Z_TYPE(ret)!=IS_LONG) convert_to_long((&ret));
return Z_LVAL(ret);
}

/*---------------------------------------------------------------*/

static void ut_call_user_function_string(zval *obj_zp,zval *func_zp,zval *ret
	,int nb_args,zval **args TSRMLS_DC)
{

ut_call_user_function(obj_zp,func_zp,ret,nb_args,args TSRMLS_CC);
if (EG(exception)) return;

if (Z_TYPE_P(ret)!=IS_STRING) convert_to_string(ret);
}

/*---------------------------------------------------------------*/

static void ut_call_user_function_array(zval *obj_zp,zval *func_zp,zval *ret
	,int nb_args,zval **args TSRMLS_DC)
{

ut_call_user_function(obj_zp,func_zp,ret,nb_args,args TSRMLS_CC);
if (EG(exception)) return;

if (Z_TYPE_P(ret)!=IS_ARRAY)
	{
	THROW_EXCEPTION_2("%s method should return a string (type=%d)"
		,Z_STRVAL_P(func_zp),Z_TYPE_P(ret));
	}
}

/*---------------------------------------------------------------*/

static void ut_call_user_function(zval *obj_zp,zval *func_zp,zval *ret
	,int nb_args,zval **args TSRMLS_DC)
{
call_user_function(EG(function_table),&obj_zp,func_zp,ret,nb_args
	,args TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static int ut_extension_loaded(char *name, int len TSRMLS_DC)
{
return zend_hash_exists(&module_registry,name,len+1);
}

/*---------------------------------------------------------------*/

static void ut_require(char *string, zval *ret TSRMLS_DC)
{
char *p;

spprintf(&p,1024,"require '%s';",string);

zend_eval_string(p,ret,"eval" TSRMLS_CC);
efree(p);
}

/*---------------------------------------------------------------*/

static int ut_strings_are_equal(zval *zp1, zval *zp2 TSRMLS_DC)
{
if ((!zp1)||(!zp2)) return 0;

if (Z_TYPE_P(zp1) != IS_STRING) convert_to_string(zp1);
if (Z_TYPE_P(zp2) != IS_STRING) convert_to_string(zp2);

if (Z_STRLEN_P(zp1) != Z_STRLEN_P(zp2)) return 0;

return (!memcmp(Z_STRVAL_P(zp1),Z_STRVAL_P(zp1),Z_STRLEN_P(zp1)));
}

/*---------------------------------------------------------------*/

static void ut_header(char *string TSRMLS_DC)
{
sapi_header_line ctr;

ctr.line=string;
ctr.line_len=strlen(string);

sapi_header_op(SAPI_HEADER_REPLACE,&ctr TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void ut_http_403_fail(TSRMLS_D)
{
ut_header("HTTP/1.0 403 Forbidden" TSRMLS_CC);

ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void ut_http_404_fail(TSRMLS_D)
{
ut_header("HTTP/1.0 404 Not Found" TSRMLS_CC);

ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void ut_exit(int status TSRMLS_DC)
{
EG(exit_status)=status;
zend_bailout();
}

/*---------------------------------------------------------------*/

static zval *_ut_SERVER_element(HKEY_STRUCT *hkey TSRMLS_DC)
{
zval **array,**token;

if ((FIND_HKEY(&EG(symbol_table),_SERVER,&array) == SUCCESS)
	&& (Z_TYPE_PP(array) == IS_ARRAY)
	&& (zend_hash_quick_find(Z_ARRVAL_PP(array),hkey->string,hkey->len
		,hkey->hash,(void **)(&token)) == SUCCESS))
	{
	return *token;
	}
return NULL;
}

/*---------------------------------------------------------------*/

static zval *_ut_REQUEST_element(HKEY_STRUCT *hkey TSRMLS_DC)
{
zval **array,**token;

if ((FIND_HKEY(&EG(symbol_table),_REQUEST,&array) == SUCCESS)
	&& (Z_TYPE_PP(array) == IS_ARRAY)
	&& (zend_hash_quick_find(Z_ARRVAL_PP(array),hkey->string,hkey->len
		,hkey->hash,(void **)(&token)) == SUCCESS))
	{
	return *token;
	}
return NULL;
}

/*---------------------------------------------------------------*/

static char *ut_http_base_url(TSRMLS_D)
{
zval *pathinfo,*php_self;
int ilen,slen,nslen;
static char buffer[UT_PATH_MAX];

if (!ut_is_web()) return "";

if (!(pathinfo=SERVER_ELEMENT(PATH_INFO)))
	return Z_STRVAL_P(SERVER_ELEMENT(PHP_SELF));

ilen=Z_STRLEN_P(pathinfo);

php_self=SERVER_ELEMENT(PHP_SELF);
slen=Z_STRLEN_P(php_self);

nslen=slen-ilen;
if ((nslen > 0)
	&& (!memcmp(Z_STRVAL_P(pathinfo),Z_STRVAL_P(php_self)+nslen,ilen)))
	{
	if (nslen>=UT_PATH_MAX) nslen=UT_PATH_MAX-1;
	memmove(buffer,Z_STRVAL_P(php_self),nslen);
	buffer[nslen]='\0';
	return buffer;
	}

return Z_STRVAL_P(php_self);
}

/*---------------------------------------------------------------*/

static void ut_http_301_redirect(zval *path, int must_free TSRMLS_DC)
{
char *p;

spprintf(&p,UT_PATH_MAX,"Location: http://%s%s%s"
	,Z_STRVAL_P(SERVER_ELEMENT(HTTP_HOST)),ut_http_base_url(TSRMLS_C)
	,Z_STRVAL_P(path));

ut_header(p TSRMLS_CC);
efree(p);

ut_header("HTTP/1.1 301 Moved Permanently" TSRMLS_CC);

if (must_free) zval_dtor(path);
ut_exit(0 TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void ut_rtrim_zval(zval *zp TSRMLS_DC)
{
char *p;

p=Z_STRVAL_P(zp)+Z_STRLEN_P(zp)-1;
while (Z_STRLEN_P(zp))
	{
	if ((*p)!='/') break;
	(*p)='\0';
	Z_STRLEN_P(zp)--;
	p--;
	}
}

/*---------------------------------------------------------------*/

static void ut_file_suffix(zval *path, zval *ret TSRMLS_DC)
{
int found,suffix_len;
char *p;

if (Z_STRLEN_P(path) < 2)
	{
	ZVAL_STRINGL(ret,"",0,1);
	return;
	}

for (suffix_len=found=0,p=Z_STRVAL_P(path)+Z_STRLEN_P(path)-1
	;p>=Z_STRVAL_P(path);p--,suffix_len++)
	{
	if ((*p)=='.')
		{
		found=1;
		break;
		}
	if ((*p)=='/') break;
	}

if (!found)
	{
	ZVAL_STRINGL(ret,"",0,1);
	}
else
	{
	ZVAL_STRINGL(ret,p+1,suffix_len,1);

	p=Z_STRVAL_P(ret);
	while(*p) // strtolower
		{
		if (((*p)>='A') && ((*p)<='Z')) (*p) += ('a' - 'A');
		p++;
		}
	}
}

/*---------------------------------------------------------------*/

static int MINIT_utils(TSRMLS_D)
{
INIT_CZVAL(__construct);

return SUCCESS;
}

/*-------------*/

static int MSHUTDOWN_utils(TSRMLS_D)
{
return SUCCESS;
}

/*-------------*/

static int RINIT_utils(TSRMLS_D)
{
return SUCCESS;
}

/*-------------*/

static int RSHUTDOWN_utils(TSRMLS_D)
{
return SUCCESS;
}

/*============================================================================*/
