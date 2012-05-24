/*
  +----------------------------------------------------------------------+
  | Automap extension <http://automap.tekwire.net>                       |
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

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::register_failure_handler(callable callback) */

static PHP_METHOD(Automap, register_failure_handler)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp)
		==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	ENSURE_STRING(zp);
	EALLOCATE(AUTOMAP_G(failure_handlers),(AUTOMAP_G(fh_count)+1)*sizeof(zp));
	AUTOMAP_G(failure_handlers)[AUTOMAP_G(fh_count)++]=zp;
	Z_ADDREF_P(zp);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_call_failure_handlers(char type, char *symbol, int slen TSRMLS_DC)
{
	zval *args[2],*ztype,*zsymbol;
	char str[2];
	int i;

	if (AUTOMAP_G(fh_count)) {
		str[0]=type;
		str[1]='\0';
		MAKE_STD_ZVAL(ztype);
		ZVAL_STRINGL(ztype,str,1,1);
		MAKE_STD_ZVAL(zsymbol);
		ZVAL_STRINGL(zsymbol,symbol,slen,1);
		args[0]=ztype;
		args[1]=zsymbol;
		for (i=0;i<AUTOMAP_G(fh_count);i++) {
			ut_call_user_function_void(NULL
			,Z_STRVAL_P(AUTOMAP_G(failure_handlers)[i])
			,Z_STRLEN_P(AUTOMAP_G(failure_handlers)[i])
			,2 ,args TSRMLS_CC);
		}
		ut_ezval_ptr_dtor(&ztype);
		ut_ezval_ptr_dtor(&zsymbol);
	}
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::register_success_handler(callable callback) */

static PHP_METHOD(Automap, register_success_handler)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp)
		==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	ENSURE_STRING(zp);
	EALLOCATE(AUTOMAP_G(success_handlers),(AUTOMAP_G(sh_count)+1)*sizeof(zp));
	AUTOMAP_G(success_handlers)[AUTOMAP_G(sh_count)++]=zp;
	Z_ADDREF_P(zp);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Success handler API: (Automap $map, string $stype, string $symbol, string $ptype, string $path) */

static void Automap_call_success_handlers(Automap_Mnt *mp
	,Automap_Pmap_Entry *pep TSRMLS_DC)
{
	zval *args[3],*zstype,*zsname;
	char str[2];
	int i;

	if (AUTOMAP_G(sh_count)) {
		str[0]=pep->stype;
		str[1]='\0';
		MAKE_STD_ZVAL(zstype);
		ZVAL_STRINGL(zstype,str,1,1);
		Automap_instance_by_mp(mp TSRMLS_CC);
		MAKE_STD_ZVAL(zsname);
		ZVAL_COPY_VALUE(zsname,&(pep->zsname));
		zval_copy_ctor(zsname);
		args[0]=zstype;
		args[1]=zsname;
		args[2]=mp->instance;
		for (i=0;i<AUTOMAP_G(sh_count);i++) {
			ut_call_user_function_void(NULL
			,Z_STRVAL_P(AUTOMAP_G(success_handlers)[i])
			,Z_STRLEN_P(AUTOMAP_G(success_handlers)[i])
			,3,args TSRMLS_CC);
		}
		ut_ezval_ptr_dtor(&zstype);
		ut_ezval_ptr_dtor(&zsname);
	}
}

/*===============================================================*/

static int MINIT_Automap_Handlers(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Handlers(TSRMLS_D)
{
return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Handlers(TSRMLS_D)
{
	AUTOMAP_G(fh_count)=AUTOMAP_G(sh_count)=0;
	AUTOMAP_G(failure_handlers)=NULL;
	AUTOMAP_G(success_handlers)=NULL;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Handlers(TSRMLS_D)
{
	int i;

	if (AUTOMAP_G(fh_count)) {
		for (i=0;i<AUTOMAP_G(fh_count);i++) {
			ut_ezval_ptr_dtor(AUTOMAP_G(failure_handlers)+i);
		}
		EALLOCATE(AUTOMAP_G(failure_handlers),0);
		AUTOMAP_G(fh_count)=0;
	}

	if (AUTOMAP_G(sh_count)) {
		for (i=0;i<AUTOMAP_G(sh_count);i++) {
			ut_ezval_ptr_dtor(AUTOMAP_G(success_handlers)+i);
		}
		EALLOCATE(AUTOMAP_G(success_handlers),0);
		AUTOMAP_G(sh_count)=0;
	}

	return SUCCESS;
}

/*===============================================================*/
