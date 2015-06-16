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
/* Starting with version 3.0, Automap is fully case-sensitive. This allows for
   higher performance and cleaner code */
/* This function a newly-allocated zend_string (which must be released) */

static zend_string *Automap_key(char type, zend_string *symbol TSRMLS_DC)
{
	char *p,*start;
	int len;
	zend_string *ret;
	
	len=ZSTR_LEN(symbol);
	start=ZSTR_VAL(symbol);
	while((*start)=='\\') {
		start++;
		len--;
	}

	ret=zend_string_alloc(len+1, 0);
	p=ZSTR_VAL(ret);
	*(p++)=type;
	memcpy(p,start,len+1);

	return ret;
}

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
