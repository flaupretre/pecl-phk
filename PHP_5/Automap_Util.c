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
/* In case of error, free mem allocated by ut_pathUniqueID() */

static void Automap_ufid(zval *path, zval **zufidpp TSRMLS_DC)
{
	ut_pathUniqueID('m', path, zufidpp, NULL TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static int Automap_symbol_is_defined(char type, char *symbol
	, unsigned int slen TSRMLS_DC)
{
	char *p;
	int status;
	zval dummy_zp;

	if (!slen) return 0;

	if (type==AUTOMAP_T_CONSTANT) p=NULL;
	else p=zend_str_tolower_dup(symbol,slen);

	status=0;
	switch(type) {
		case AUTOMAP_T_CONSTANT:
			status=zend_get_constant(symbol,slen,&dummy_zp TSRMLS_CC);
			if (status) zval_dtor(&dummy_zp);
			break;

		case AUTOMAP_T_FUNCTION:
			status=zend_hash_exists(EG(function_table),p,slen+1);
			break;

		case AUTOMAP_T_CLASS: /* Also works for interfaces and traits */
			status=zend_hash_exists(EG(class_table),p,slen+1);
			break;

		case AUTOMAP_T_EXTENSION:
			status=ut_extension_loaded(p,slen TSRMLS_CC);
			break;


	}

	if (p) efree(p);

	return status;
}

/*---------------------------------------------------------------*/
/* {{{ proto void \Automap\Mgr::symbolIsDefined(string type, string symbol) */

static PHP_METHOD(Automap, symbolIsDefined)
{
	char *symbol,*type;
	int slen,tlen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ss",&symbol, &slen
		,&type,&tlen)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	RETURN_BOOL(Automap_symbol_is_defined(*type, symbol, slen TSRMLS_CC)==SUCCESS);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto bool PHK::using accelerator() */

static PHP_METHOD(Automap, usingAccelerator)
{
	RETVAL_BOOL(PHK_G(ext_is_enabled));
}

/* }}} */
/*===============================================================*/

static int MINIT_Automap_Util(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Util(TSRMLS_D)
{
return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Util(TSRMLS_D)
{
	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Util(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
