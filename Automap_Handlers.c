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

/*---------------------------------------------------------------*/
/* {{{ proto string \Automap\Mgr::registerFailureHandler(callable callback) */

static PHP_METHOD(Automap, registerFailureHandler)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp)
		==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	ENSURE_STRING(zp);
	EALLOCATE(PHK_G(automap_failureHandlers),(PHK_G(automap_fh_count)+1)*sizeof(zp));
	PHK_G(automap_failureHandlers)[PHK_G(automap_fh_count)++]=zp;
	Z_ADDREF_P(zp);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Failure handler receives 2 args: type and symbol */

static void Automap_callFailureHandlers(char type, char *symbol, int slen TSRMLS_DC)
{
	zval *args[2],*ztype,*zsymbol;
	char str[2];
	int i;

	if (PHK_G(automap_fh_count)) {
		str[0]=type;
		str[1]='\0';
		MAKE_STD_ZVAL(ztype);
		ZVAL_STRINGL(ztype,str,1,1);
		MAKE_STD_ZVAL(zsymbol);
		ZVAL_STRINGL(zsymbol,symbol,slen,1);
		args[0]=ztype;
		args[1]=zsymbol;
		for (i=0;i<PHK_G(automap_fh_count);i++) {
			ut_call_user_function_void(NULL
			,Z_STRVAL_P(PHK_G(automap_failureHandlers)[i])
			,Z_STRLEN_P(PHK_G(automap_failureHandlers)[i])
			,2 ,args TSRMLS_CC);
		}
		ut_ezval_ptr_dtor(&ztype);
		ut_ezval_ptr_dtor(&zsymbol);
	}
}

/*---------------------------------------------------------------*/
/* {{{ proto string \Automap\Mgr::registerSuccessHandler(callable callback) */

static PHP_METHOD(Automap, registerSuccessHandler)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp)
		==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	ENSURE_STRING(zp);
	EALLOCATE(PHK_G(automap_successHandlers),(PHK_G(automap_sh_count)+1)*sizeof(zp));
	PHK_G(automap_successHandlers)[PHK_G(automap_sh_count)++]=zp;
	Z_ADDREF_P(zp);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Success handler receives 2 args : An export of the successful entry, and
*  the load ID of the map where the symbol was found. */

static void Automap_callSuccessHandlers(Automap_Mnt *mp
	,Automap_Pmap_Entry *pep TSRMLS_DC)
{
	zval *args[2],*entry_zp,*id_zp;
	int i;

	if (PHK_G(automap_sh_count)) {
		ALLOC_INIT_ZVAL(entry_zp);
		Automap_Pmap_exportEntry(pep,entry_zp TSRMLS_CC);
		args[0]=entry_zp;
		ALLOC_INIT_ZVAL(id_zp);
		ZVAL_LONG(id_zp,mp->id);
		args[1]=id_zp;
		for (i=0;i<PHK_G(automap_sh_count);i++) {
			DBG_MSG1("Calling success handler #%d",i);
			ut_call_user_function_void(NULL
				,Z_STRVAL_P(PHK_G(automap_successHandlers)[i])
				,Z_STRLEN_P(PHK_G(automap_successHandlers)[i])
				,2,args TSRMLS_CC);
		}
		ut_ezval_ptr_dtor(&entry_zp);
		ut_ezval_ptr_dtor(&id_zp);
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
	PHK_G(automap_fh_count)=PHK_G(automap_sh_count)=0;
	PHK_G(automap_failureHandlers)=NULL;
	PHK_G(automap_successHandlers)=NULL;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Handlers(TSRMLS_D)
{
	int i;

	if (PHK_G(automap_fh_count)) {
		for (i=0;i<PHK_G(automap_fh_count);i++) {
			ut_ezval_ptr_dtor(PHK_G(automap_failureHandlers)+i);
		}
		EALLOCATE(PHK_G(automap_failureHandlers),0);
		PHK_G(automap_fh_count)=0;
	}

	if (PHK_G(automap_sh_count)) {
		for (i=0;i<PHK_G(automap_sh_count);i++) {
			ut_ezval_ptr_dtor(PHK_G(automap_successHandlers)+i);
		}
		EALLOCATE(PHK_G(automap_successHandlers),0);
		PHK_G(automap_sh_count)=0;
	}

	return SUCCESS;
}

/*===============================================================*/
