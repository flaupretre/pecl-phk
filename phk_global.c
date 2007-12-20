/*
  +----------------------------------------------------------------------+
  | PHK accelerator extension <http://phk.tekwire.net>                   |
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

#include "phk_global.h"

#include "php_phk.h"

/*---------------------------------------------------------------*/

static void phk_require_extensions(zval * extensions TSRMLS_DC)
{
	HashTable *ht;
	HashPosition pos;
	zval **zpp;

	ht = Z_ARRVAL_P(extensions);

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (zend_hash_get_current_data_ex(ht, (void **) (&zpp), &pos) ==
		   SUCCESS) {
		if (ZVAL_IS_STRING(*zpp) && (!ut_extension_loaded(Z_STRVAL_PP(zpp)
														  ,
														  Z_STRLEN_PP(zpp)
														  TSRMLS_CC))) {
			PHK_need_php_runtime(TSRMLS_C);
			ut_call_user_function_void(&CZVAL(PHK_Util)
									   , &CZVAL(require_extensions), 1,
									   &extensions TSRMLS_CC);
			return;
		}
		zend_hash_move_forward_ex(ht, &pos);
	}
}

/*---------------------------------------------------------------*/
