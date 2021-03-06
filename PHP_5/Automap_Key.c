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

static void Automap_key(char type, char *symbol, unsigned long len
	, zval *ret TSRMLS_DC)
{
	char *p;

	while((*symbol)=='\\') {
		symbol++;
		len--;
	}

	p=ut_eallocate(NULL,len+2);
	p[0]=type;
	memmove(p+1,symbol,len+1);

	INIT_ZVAL(*ret);
	ZVAL_STRINGL(ret,p,len+1,0);
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
