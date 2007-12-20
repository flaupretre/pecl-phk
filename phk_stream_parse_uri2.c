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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "php.h"
#include "SAPI.h"
#include "php_variables.h"

#include "php_phk.h"
#include "phk_stream_parse_uri2.h"
#include "PHK_Mgr.h"

/*---------------------------------------------------------------*/
/* Parse a PHK URI and returns its components                       */

PHP_FUNCTION(_phk_stream_parse_uri2)
{
	zval *uri;
	zval *z_command, *z_params, *z_mnt, *z_path;

/*-- Get parameter */

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zzzzz", &uri, &z_command, &z_params,
		 &z_mnt, &z_path) == FAILURE)
		RETURN_NULL();

	_phk_stream_parse_uri2(uri, z_command, z_params, z_mnt,
						   z_path TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* C API */

static void _phk_stream_parse_uri2(zval * uri, zval * z_command,
								   zval * z_params, zval * z_mnt,
								   zval * z_path TSRMLS_DC)
{
	char *cmdp, *ampp, *path, *p, *res, *urip;
	int cmd_len, path_len, mnt_len, slash1, uri_len;

	DBG_MSG("Entering _phk_stream_parse_uri2");
	/*DBG_MSG1("parse_uri2: on entry, uri=<%s>",Z_STRVAL_P(uri));*/

	/* Check that it is a phk uri (should always succed) */

	if (!PHK_Mgr_is_a_phk_uri(uri TSRMLS_CC)) {
		EXCEPTION_ABORT_1("%s: Not a PHK URI", Z_STRVAL_P(uri));
	}

	urip = Z_STRVAL_P(uri) + 6;	/* Remove 'phk://' */
	uri_len = Z_STRLEN_P(uri) - 6;

	while ((*urip) == '/') {	/* Suppress leading '/' chars */
		urip++;
		uri_len--;
	}

	/*DBG_MSG2("parse_uri2: after stripping, uri=<%s> - uri_len=%d",urip,uri_len);*/

	for (p = urip, cmdp = ampp = NULL, mnt_len = uri_len, slash1 =
		 1, cmd_len = 0;; p++) {
		if ((*p) == '\0')
			break;
		switch (*p) {
		  case '\\':
			  (*p) = '/';
		  case '/':
			  if (slash1) {
				  slash1 = 0;
				  mnt_len = p - urip;
			  }
			  break;

		  case '?':
			  if (slash1) {
				  slash1 = 0;
				  mnt_len = p - urip;
			  }
			  uri_len = p - urip;
			  cmdp = p + 1;
			  if ((*cmdp) == '\0') {
				  EXCEPTION_ABORT_1("%s: Empty command", Z_STRVAL_P(uri));
			  }
			  break;

		  case '&':
			  if (!cmdp) {		/* Parameters before command */
				  EXCEPTION_ABORT_1("%s: Parameters before command",
									Z_STRVAL_P(uri));
			  }
			  cmd_len = p - cmdp;
			  ampp = p + 1;

			  if (z_params) {
				  zval_dtor(z_params);
				  res = estrdup(ampp);	/* Leaked ? */
				  array_init(z_params);
				  DBG_MSG1
					  ("parse_uri2: calling sapi_module.treat_data, ampp=<%s>",
					   ampp);
				  sapi_module.treat_data(PARSE_STRING, res,
										 z_params TSRMLS_CC);
			  }
			  break;
		}
	}

	if (cmdp && (cmd_len == 0))
		cmd_len = p - cmdp;		/* Command without argument */

	p = &(urip[uri_len - 1]);
	while (uri_len && (*(p--) == '/'))
		uri_len--;				/* Suppress trailing '/' */

	/*DBG_MSG2("parse_uri2: after stripping2, uri=<%s> - uri_len=%d",urip,uri_len);*/
	/*DBG_MSG1("parse_uri2: after stripping2, mnt_len=%d",mnt_len);*/
	/*DBG_MSG1("parse_uri2: after stripping2, cmd_len=%d",cmd_len);*/

	path_len = 0;
	path = "";
	if ((mnt_len = MIN(uri_len, mnt_len)) != 0) {	/* Not a global command */
		if (uri_len > (mnt_len + 1)) {
			path_len = uri_len - mnt_len - 1;
			path = &(urip[mnt_len + 1]);
		}
	}

	if ((!cmdp) && (!mnt_len))
		THROW_EXCEPTION_1("Empty URI", NULL);

/* Return values */

	if (z_command) {
		zval_dtor(z_command);
		if (cmdp) {
			ZVAL_STRINGL(z_command, cmdp, cmd_len, 1);
		} else {
			ZVAL_NULL(z_command);
		}
	}

	if (z_mnt) {
		zval_dtor(z_mnt);
		if (mnt_len) {
			ZVAL_STRINGL(z_mnt, urip, mnt_len, 1);
		} else {
			ZVAL_NULL(z_mnt);
		}
	}

	if (z_path) {
		zval_dtor(z_path);
		if (path_len) {
			ZVAL_STRINGL(z_path, path, path_len, 1);
		} else {
			ZVAL_NULL(z_path);
		}
	}
}

/*---------------------------------------------------------------*/
