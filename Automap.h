/*
  +----------------------------------------------------------------------+
  | Automap extension <http://automap.tekwire.net>                     |
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

#ifndef AUTOMAP_H
#define AUTOMAP_H 1
/*---------------------------------------------------------------*/

#define AUTOMAP_MAGIC "AUTOMAP  M\024\010\6\3"

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
#endif							/*AUTOMAP_H */
