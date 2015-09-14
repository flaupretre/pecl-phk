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

#ifndef __AUTOMAP_PARSER_H
#define __AUTOMAP_PARSER_H

#include "Zend/zend_language_parser.h"

/*---------------------------------------------------------------*/

/*-- For PHP version < 5.3.0 --*/

#ifndef T_NAMESPACE
#define T_NAMESPACE (-2)
#endif

#ifndef T_NS_SEPARATOR
#define T_NS_SEPARATOR (-3)
#endif

#ifndef T_CONST
#define T_CONST (-4)
#endif

#ifndef T_TRAIT
#define T_TRAIT (-5)
#endif

/* Parser states */

#define AUTOMAP_ST_OUT						((char)1)
#define AUTOMAP_ST_FUNCTION_FOUND			((char)AUTOMAP_T_FUNCTION)
#define AUTOMAP_ST_SKIPPING_BLOCK_NOSTRING	((char)3)
#define AUTOMAP_ST_SKIPPING_BLOCK_STRING	((char)4)
#define AUTOMAP_ST_CLASS_FOUND				((char)AUTOMAP_T_CLASS)
#define AUTOMAP_ST_DEFINE_FOUND				((char)6)
#define AUTOMAP_ST_DEFINE_2					((char)7)
#define AUTOMAP_ST_SKIPPING_TO_EOL			((char)8)
#define AUTOMAP_ST_NAMESPACE_FOUND			((char)9)
#define AUTOMAP_ST_NAMESPACE_2				((char)10)
#define AUTOMAP_ST_CONST_FOUND				((char)11)

#define AUTOMAP_SYMBOL_MAX		1024	// Max size of a symbol

/*============================================================================*/

static PHP_NAMED_FUNCTION(Automap_Ext_file_get_contents);
static PHP_NAMED_FUNCTION(Automap_Ext_parseTokens);
static void Automap_Parser_addSymbol(zval *arr,char type,char *ns,int nslen
	,char *name,int nalen);
static void Automap_parseTokens(zval *zbuf, int skip_blocks, zval *ret TSRMLS_DC);

static int MINIT_Automap_Parser(TSRMLS_D);
static int MSHUTDOWN_Automap_Parser(TSRMLS_D);
static int RINIT_Automap_Parser(TSRMLS_D);
static int RSHUTDOWN_Automap_Parser(TSRMLS_D);

/*============================================================================*/
#endif
