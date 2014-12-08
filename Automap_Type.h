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

#ifndef __AUTOMAP_TYPE_H
#define __AUTOMAP_TYPE_H

/*---------------------------------------------------------------*/
/* Key types */

#define AUTOMAP_T_FUNCTION	'F'
#define AUTOMAP_T_CONSTANT	'C'
#define AUTOMAP_T_CLASS		'L'
#define AUTOMAP_T_EXTENSION	'E'

#define AUTOMAP_F_SCRIPT	'S'
#define AUTOMAP_F_EXTENSION	'X'
#define AUTOMAP_F_PACKAGE	'P'

typedef struct {
	char type;
	char *string;
} automap_type_string;

static automap_type_string automap_type_strings[]={
	{ AUTOMAP_T_FUNCTION,	"function" },
	{ AUTOMAP_T_CONSTANT,	"constant" },
	{ AUTOMAP_T_CLASS, 		"class" },
	{ AUTOMAP_T_EXTENSION,	"extension" },
	{ AUTOMAP_F_SCRIPT,		"script" },
	{ AUTOMAP_F_EXTENSION,	"extension file" },
	{ AUTOMAP_F_PACKAGE,	"package" },
	{ '\0', NULL }
};

/*---------------------------------------------------------------*/

ZEND_DLEXPORT char *Automap_type_to_string(char type TSRMLS_DC);
static PHP_METHOD(Automap, type_to_string);
ZEND_DLEXPORT char Automap_string_to_type(char *string TSRMLS_DC);
static PHP_METHOD(Automap, string_to_type);

static int MINIT_Automap_Type(TSRMLS_D);
static int MSHUTDOWN_Automap_Type(TSRMLS_D);
static int RINIT_Automap_Type(TSRMLS_D);
static int RSHUTDOWN_Automap_Type(TSRMLS_D);

/*---------------------------------------------------------------*/
#endif
