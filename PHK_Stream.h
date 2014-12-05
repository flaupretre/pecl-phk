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

#ifndef __PHK_STREAM_H
#define __PHK_STREAM_H

/*============================================================================*/

typedef struct {
	int offset;
	int show_errors;
	zval *z_data;
	int parse_done;
	zval *z_mnt;
	zval *z_command;
	zval *z_params;
	zval *z_path;
} PHK_STREAM_DATA;

static char last_cached_opcode_string[UT_PATH_MAX + 1];
static int last_cached_opcode_len;

/*============================================================================*/

static PHK_STREAM_DATA *new_dp(int show_errors);
static void free_dp(PHK_STREAM_DATA ** dpp);
static void clear_last_cached_opcode(TSRMLS_D);
static void set_last_cached_opcode(const char *path, int len TSRMLS_DC);
static int is_last_cached_opcode(const char *path, int len TSRMLS_DC);
static void PHK_Stream_get_file(int dir, zval * z_ret_p, zval * z_uri_p,
								zval * z_mnt_p, zval * z_command_p,
								zval * z_params_p, zval * z_path_p,
								zval * z_cache_p TSRMLS_DC);
static PHP_METHOD(PHK_Stream, get_file);
static size_t PHK_Stream_write(php_stream * stream, const char *buf,
							   size_t count TSRMLS_DC);
static size_t PHK_Stream_read(php_stream * stream, char *buf,
							  size_t count TSRMLS_DC);
static int PHK_Stream_close(php_stream * stream,
							int close_handle TSRMLS_DC);
static int PHK_Stream_flush(php_stream * stream TSRMLS_DC);
static int PHK_Stream_seek(php_stream * stream, off_t offset, int whence,
						   off_t * newoffset TSRMLS_DC);
static int do_stat(php_stream_wrapper * wrapper, char *uri,
				   PHK_STREAM_DATA * dp,
				   php_stream_statbuf * ssb TSRMLS_DC);
static int PHK_Stream_fstat(php_stream * stream, php_stream_statbuf * ssb
							TSRMLS_DC);
static size_t PHK_Stream_readdir(php_stream * stream, char *buf,
								 size_t count TSRMLS_DC);
static int PHK_Stream_seekdir(php_stream * stream, off_t offset,
							  int whence, off_t * newoffset TSRMLS_DC);
static php_stream *PHK_Stream_generic_open(int dir,
										   php_stream_wrapper * wrapper,
										   char *uri, char *mode,
										   int options, char **opened_path,
										   php_stream_context *
										   context STREAMS_DC TSRMLS_DC);
static php_stream *PHK_Stream_openfile(php_stream_wrapper * wrapper,
									   char *uri, char *mode, int options,
									   char **opened_path,
									   php_stream_context *
									   context STREAMS_DC TSRMLS_DC);
static int PHK_Stream_url_stat(php_stream_wrapper * wrapper, char *uri,
							   int flags, php_stream_statbuf * ssb,
							   php_stream_context * context TSRMLS_DC);
static php_stream *PHK_Stream_opendir(php_stream_wrapper * wrapper,
									  char *uri, char *mode, int options,
									  char **opened_path,
									  php_stream_context *
									  context STREAMS_DC TSRMLS_DC);
static void PHK_Stream_parse_uri(zval * uri, zval * z_command,
								 zval * z_params, zval * z_mnt,
								 zval * z_path TSRMLS_DC);
static char *PHK_Stream_cache_key(php_stream_wrapper * wrapper,
								  const char *uri, int uri_len,
								  int *key_len TSRMLS_DC);

static int MINIT_PHK_Stream(TSRMLS_D);
static int MSHUTDOWN_PHK_Stream(TSRMLS_D);
static inline int RINIT_PHK_Stream(TSRMLS_D);
static inline int RSHUTDOWN_PHK_Stream(TSRMLS_D);

/*============================================================================*/
#endif
