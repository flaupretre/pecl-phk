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

#ifndef __AUTOMAP_KEY_H
#define __AUTOMAP_KEY_H

/*============================================================================*/

static void Automap_key(char type, char *symbol, unsigned long len, zval *ret TSRMLS_DC);
static PHP_METHOD(Automap, key);

static int MINIT_Automap_Key(TSRMLS_D);
static int MSHUTDOWN_Automap_Key(TSRMLS_D);
static int RINIT_Automap_Key(TSRMLS_D);
static int RSHUTDOWN_Automap_Key(TSRMLS_D);

/*============================================================================*/
#endif
