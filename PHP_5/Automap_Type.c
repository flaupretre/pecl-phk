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

static char *Automap_typeToString(char type TSRMLS_DC)
{
	automap_type_string *sp;

	for (sp=automap_type_strings;sp->type;sp++) {
		if (sp->type == type) return sp->string;
	}

	THROW_EXCEPTION_1("%c : Invalid type", type);
	return NULL;
}

/*---------------------------------------------------------------*/
/* {{{ proto string \Automap\Mgr::typeToString(string type) */

static PHP_METHOD(Automap, typeToString)
{
	char *type,*p;
	int tlen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &type,&tlen) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	if (!(p=Automap_typeToString(*type TSRMLS_CC))) return;
	RETURN_STRING(p,1);
}

/* }}} */
/*---------------------------------------------------------------*/

static char Automap_stringToType(char *string TSRMLS_DC)
{
	automap_type_string *sp;

	for (sp=automap_type_strings;sp->type;sp++) {
		if (!strcmp(sp->string,string)) return sp->type;
	}

	THROW_EXCEPTION_1("%s : Invalid type", string);
	return '\0';
}

/*---------------------------------------------------------------*/
/* {{{ proto string \Automap\Mgr::stringToType(string str) */

static PHP_METHOD(Automap, stringToType)
{
	char *string,c[2];
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &string,&slen) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	if (!(c[0]=Automap_stringToType(string TSRMLS_CC))) return;
	c[1]='\0';
	RETURN_STRINGL(c,1,1);
}

/* }}} */
/*===============================================================*/

static int MINIT_Automap_Type(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Type(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Type(TSRMLS_D)
{
	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Type(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
