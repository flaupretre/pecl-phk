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

static PHP_METHOD(Automap, min_map_version)
{
	RETVAL_STRING(AUTOMAP_MIN_MAP_VERSION,1);
}

/*---------------------------------------------------------------*/
/* In case of error, free mem allocated by ut_path_unique_id() */

static void Automap_path_id(zval * path, zval **id_zp TSRMLS_DC)
{
	ut_path_unique_id('m', path, id_zp, NULL TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::path_id(string path) */

static PHP_METHOD(Automap, path_id)
{
	zval *path;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	Automap_path_id(path, &return_value TSRMLS_CC);
}

/* }}} */
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
/* {{{ proto bool PHK::using accelerator() */

static PHP_METHOD(Automap, using_accelerator)
{
	RETVAL_TRUE;
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto void Automap::accel_techinfo() */

static PHP_METHOD(Automap, accel_techinfo)
{
	if (sapi_module.phpinfo_as_text) {
		php_printf("Using Automap Accelerator: Yes\n");
		php_printf("Accelerator Version: %s\n", PHP_AUTOMAP_VERSION);
	} else {
		php_printf("<table border=0>");
		php_printf("<tr><td>Using Automap Accelerator:&nbsp;</td><td>Yes</td></tr>");
		php_printf("<tr><td>Accelerator Version:&nbsp;</td><td>%s</td></tr>",
			 PHP_AUTOMAP_VERSION);
		php_printf("</table>");
	}
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
