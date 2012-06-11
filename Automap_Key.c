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

static void Automap_key(char type, char *symbol, unsigned long len
	, zval *ret TSRMLS_DC)
{
	char *p,*p2;
	int found;

	while((*symbol)=='\\') {
		symbol++;
		len--;
	}

	p=ut_eallocate(NULL,len+2);
	p[0]=type;
	memmove(p+1,symbol,len+1);

	if ((type == AUTOMAP_T_EXTENSION)
		|| (type == AUTOMAP_T_FUNCTION)
		|| (type == AUTOMAP_T_CLASS)) {
		ut_tolower(p+1,len TSRMLS_CC);
	}
	else { /* lowercase namespace only */
		p2=p+len;
		found=0;
		while (p2>p) {
			if (found) {
				if (((*p2)>='A')&&((*p2)<='Z')) (*p2) += ('a'-'A');
			}
			else {
			if ((*p2)=='\\') found=1;
			}
			p2--;
		}
	}
	INIT_ZVAL(*ret);
	ZVAL_STRINGL(ret,p,len+1,0);
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::key(string type, string symbol) */

static PHP_METHOD(Automap, key)
{
	char *type,*symbol;
	int tlen,slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ss", &type,&tlen
		,&symbol,&slen)==FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	Automap_key(*type,symbol,slen,return_value TSRMLS_CC);
}

/* }}} */
/*===============================================================*/

static int MINIT_Automap_Key(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Key(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Key(TSRMLS_D)
{
	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Key(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
