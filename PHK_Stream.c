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

/*============================================================================*/

static PHK_STREAM_DATA *new_dp(int show_errors)
{
	PHK_STREAM_DATA *dp;

	dp = ut_eallocate(NULL,sizeof(PHK_STREAM_DATA));

	dp->show_errors = show_errors;

	dp->offset = 0;
	ALLOC_INIT_ZVAL(dp->z_data);

	dp->parse_done = 0;
	ALLOC_INIT_ZVAL(dp->z_command);
	ALLOC_INIT_ZVAL(dp->z_params);
	ALLOC_INIT_ZVAL(dp->z_mnt);
	ALLOC_INIT_ZVAL(dp->z_path);

	return dp;
}

/*--------------------*/

static void free_dp(PHK_STREAM_DATA ** dpp)
{
	if ((!dpp) || (!(*dpp))) return;

	ut_ezval_ptr_dtor(&((*dpp)->z_command));
	ut_ezval_ptr_dtor(&((*dpp)->z_params));
	ut_ezval_ptr_dtor(&((*dpp)->z_mnt));
	ut_ezval_ptr_dtor(&((*dpp)->z_path));
	ut_ezval_ptr_dtor(&((*dpp)->z_data));

	EALLOCATE(*dpp,0);
}

/*--------------------*/
/* The 'xxx_last_cached_opcode() functions allow to avoid storing data
* in the user cache when it is requested through the opcode cache. This is just
* for performance reason as the data we would put in the user cache would
* probably never be used.
*/

static void clear_last_cached_opcode(TSRMLS_D)
{
	last_cached_opcode_len = 0;
}

/*--------------------*/

static void set_last_cached_opcode(const char *path, int len TSRMLS_DC)
{
	if (len > UT_PATH_MAX) {	/* Ignore path if too long */
		clear_last_cached_opcode(TSRMLS_C);
		return;
	}

	memmove(last_cached_opcode_string, path,
			(last_cached_opcode_len = len) + 1);
}

/*--------------------*/

static int is_last_cached_opcode(const char *path, int len TSRMLS_DC)
{
	return (last_cached_opcode_len && (last_cached_opcode_len == len)
			&& (!memcmp(last_cached_opcode_string, path, len)));
}

/*--------------------*/
/* PHK_Stream::get_file() - C API */

#define INIT_PHK_STREAM_GET_FILE() \
	{ \
	ut_ezval_dtor(ret_p); \
	ALLOC_INIT_ZVAL(key); \
	ALLOC_INIT_ZVAL(can_cache); \
	ALLOC_INIT_ZVAL(tmp); \
	}

#define CLEANUP_PHK_STREAM_GET_FILE() \
	{ \
	ut_ezval_ptr_dtor(&key); \
	ut_ezval_ptr_dtor(&can_cache); \
	ut_ezval_ptr_dtor(&tmp); \
	}

#define ABORT_PHK_STREAM_GET_FILE() \
	{ \
	CLEANUP_PHK_STREAM_GET_FILE(); \
	ut_ezval_dtor(ret_p); \
	return; \
	}

static void PHK_Stream_get_file(int dir, zval * ret_p, zval * uri_p,
								zval * mnt_p, zval * command_p,
								zval * params_p, zval * path_p,
								zval * cache_p TSRMLS_DC)
{
	zval *key, *can_cache, *tmp, *args[5];
	int do_cache;

	INIT_PHK_STREAM_GET_FILE();

	/*-- Compute cache key */

	PHK_Cache_cache_id("node", 4, Z_STRVAL_P(uri_p), Z_STRLEN_P(uri_p),
					   key TSRMLS_CC);

	/*-- Search in cache */

	PHK_Cache_get(key, ret_p TSRMLS_CC);
	if (Z_TYPE_P(ret_p) == IS_NULL) {	/* Cache miss - slow path */
		PHK_need_php_runtime(TSRMLS_C);

		args[0] = mnt_p;
		args[1] = command_p;
		args[2] = params_p;
		args[3] = path_p;
		ZVAL_TRUE(can_cache);
		args[4] = can_cache;
		if (dir) {
			ut_call_user_function(NULL,ZEND_STRL("PHK_Stream_Backend::get_dir_data")
				, ret_p, 5, args TSRMLS_CC);
		} else {
			ut_call_user_function(NULL,ZEND_STRL("PHK_Stream_Backend::get_file_data")
				, ret_p, 5, args TSRMLS_CC);
		}
		if (EG(exception) || ZVAL_IS_NULL(ret_p))
			ABORT_PHK_STREAM_GET_FILE();	/* Forward the exception */

		if (zend_is_true(can_cache)) {
			if (!is_last_cached_opcode(Z_STRVAL_P(uri_p), Z_STRLEN_P(uri_p)
									  TSRMLS_CC)) {
				do_cache = (cache_p && (Z_TYPE_P(cache_p) == IS_BOOL)) ?
					zend_is_true(cache_p)
					: PHK_Mgr_cache_enabled(mnt_p, 0, command_p, params_p,
											path_p TSRMLS_CC);
				if (do_cache) PHK_Cache_set(key, ret_p TSRMLS_CC);
			}
		}
	}

	/*-- Reject wrong type */

	if ((dir && Z_TYPE_P(ret_p) != IS_ARRAY)
		|| ((!dir) && Z_TYPE_P(ret_p) != IS_STRING)) {
		ABORT_PHK_STREAM_GET_FILE();
	}

	CLEANUP_PHK_STREAM_GET_FILE();
}

/*--------------------*/
/* {{{ proto string PHK_Stream::get_file(bool dir, string uri, string mnt, string command, mixed params, string path [, bool cache ]) */

static PHP_METHOD(PHK_Stream, get_file)
{
	int dir;
	zval *z_uri_p, *z_mnt_p, *z_command_p, *z_params_p, *z_path_p,
		*z_cache_p;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "bzz!z!z!z|z!", &dir, &z_uri_p,
		 &z_mnt_p, &z_command_p, &z_params_p, &z_path_p,
		 &z_cache_p) == FAILURE)
		EXCEPTION_ABORT("PHK_Stream::get_file: Cannot parse parameters");

	PHK_Stream_get_file(0, return_value, z_uri_p, z_mnt_p, z_command_p,
						z_params_p, z_path_p, z_cache_p TSRMLS_CC);
}

/* }}} */
/*--------------------*/
/* Dummy write() */

static size_t PHK_Stream_write(php_stream * stream, const char *buf,
							   size_t count TSRMLS_DC)
{
	return count;
}

/*--------------------*/

static size_t PHK_Stream_read(php_stream * stream, char *buf,
							  size_t count TSRMLS_DC)
{
	PHK_STREAM_DATA *dp = stream->abstract;
	int max;

	max = Z_STRLEN_P(dp->z_data) - (dp->offset);
	if (max < 0) max = 0;	/* Should not happen */
	if (count > (size_t) max) count = (size_t) max;

	if (count) memmove(buf, Z_STRVAL_P(dp->z_data) + dp->offset, count);

	dp->offset += count;
	if (dp->offset == Z_STRLEN_P(dp->z_data)) stream->eof = 1;

	return count;
}

/*--------------------*/

static int PHK_Stream_close(php_stream * stream,
							int close_handle TSRMLS_DC)
{
	PHK_STREAM_DATA *dp = stream->abstract;

	free_dp(&dp);

	return 0;
}

/*--------------------*/
/* Dummy flush() */

static int PHK_Stream_flush(php_stream * stream TSRMLS_DC)
{
	return 0;
}

/*--------------------*/

static int PHK_Stream_seek(php_stream * stream, off_t offset, int whence,
						   off_t * newoffset TSRMLS_DC)
{
	PHK_STREAM_DATA *dp = stream->abstract;

	switch (whence) {
	  case SEEK_SET:
		  dp->offset = offset;
		  break;

	  case SEEK_CUR:
		  dp->offset = dp->offset + offset;
		  break;

	  case SEEK_END:
		  dp->offset = Z_STRLEN_P(dp->z_data) + offset;
		  break;
	}

	if (dp->offset > Z_STRLEN_P(dp->z_data)) dp->offset = Z_STRLEN_P(dp->z_data);
	if (dp->offset < 0) dp->offset = 0;

	dp->offset = dp->offset;
	if (newoffset) (*newoffset) = dp->offset;
	if (dp->offset == Z_STRLEN_P(dp->z_data)) stream->eof = 1;

	return 0;
}

/*--------------------*/
/* Here, we cache positive AND negative hits */

#define INIT_PHK_STREAM_DO_STAT() \
	{ \
	ALLOC_INIT_ZVAL(z_key); \
	ALLOC_INIT_ZVAL(z_cache); \
	ALLOC_INIT_ZVAL(z_tmp); \
	ALLOC_INIT_ZVAL(z_tmp_a); \
	ALLOC_INIT_ZVAL(z_ssb); \
	ALLOC_INIT_ZVAL(z_uri); \
	ALLOC_INIT_ZVAL(z_mode); \
	ALLOC_INIT_ZVAL(z_size); \
	ALLOC_INIT_ZVAL(z_mtime); \
	}

#define CLEANUP_PHK_STREAM_DO_STAT() \
	{ \
	ut_ezval_ptr_dtor(&z_key); \
	ut_ezval_ptr_dtor(&z_cache); \
	ut_ezval_ptr_dtor(&z_tmp); \
	ut_ezval_ptr_dtor(&z_tmp_a); \
	ut_ezval_ptr_dtor(&z_ssb); \
	ut_ezval_ptr_dtor(&z_uri); \
	ut_ezval_ptr_dtor(&z_mode); \
	ut_ezval_ptr_dtor(&z_size); \
	ut_ezval_ptr_dtor(&z_mtime); \
	}

#define ABORT_PHK_STREAM_DO_STAT() \
	{ \
	DBG_MSG("Aborting do_stat()"); \
	zend_clear_exception(TSRMLS_C); \
	CLEANUP_PHK_STREAM_DO_STAT(); \
	return -1; \
	}

static int do_stat(php_stream_wrapper * wrapper, char *uri,
	PHK_STREAM_DATA * dp, php_stream_statbuf * ssb TSRMLS_DC)
{
	zval *z_key, *z_cache, *z_tmp, *z_tmp_a, *z_ssb, *args[8], *z_uri, *z_mode,
		*z_size, *z_mtime;
	int uri_len, found, ssb_len;
	php_stream_statbuf *tssb;

	DBG_MSG1("Starting do_stat(%s)", uri);

	INIT_PHK_STREAM_DO_STAT();

	uri_len = strlen(uri);

	/*-- Parse the URI */

	if (!dp->parse_done) {
		DBG_MSG("do_stat: Parsing uri");
		ZVAL_STRINGL(z_uri, uri, uri_len, 1);
		PHK_Stream_parse_uri(z_uri, dp->z_command
							   , dp->z_params, dp->z_mnt,
							   dp->z_path TSRMLS_CC);
		if (EG(exception)) {
			DBG_MSG("do_stat:Invalid uri");
			php_stream_wrapper_log_error(wrapper,
										 dp->show_errors TSRMLS_CC,
										 "%s: Invalid PHK URI", uri);
			ABORT_PHK_STREAM_DO_STAT();
		}

		dp->parse_done = 1;

	/*-- Validate the mount point (because data can be cached) */

		if (Z_TYPE_P(dp->z_mnt) != IS_NULL) {
			(void) PHK_Mgr_get_mnt(dp->z_mnt, 0, 1 TSRMLS_CC);
			if (EG(exception)) {
				DBG_MSG1("do_stat: Invalid mount point (%s)",
						 Z_STRVAL_P(dp->z_mnt));
				ABORT_PHK_STREAM_DO_STAT();
			}
		}
	}

	/*-- Compute cache key */

	PHK_Cache_cache_id("stat", 4, uri, uri_len, z_key TSRMLS_CC);

	/*-- Search in cache */

	found = 1;
	PHK_Cache_get(z_key, z_ssb TSRMLS_CC);
	if (Z_TYPE_P(z_ssb) != IS_STRING) {	/* Cache miss - slow path */
		DBG_MSG1("do_stat(%s): cache miss", uri);

		PHK_need_php_runtime(TSRMLS_C);

		ZVAL_TRUE(z_cache);
		args[0] = dp->z_mnt;
		args[1] = dp->z_command;
		args[2] = dp->z_params;
		args[3] = dp->z_path;
		args[4] = z_cache;
		args[5] = z_mode;
		args[6] = z_size;
		args[7] = z_mtime;
		ut_call_user_function_void(NULL, ZEND_STRL("PHK_Stream_Backend::get_stat_data")
			, 8, args TSRMLS_CC);
		if (EG(exception)) {
			DBG_MSG1("do_stat: file not found (%s)", uri);
			zend_clear_exception(TSRMLS_C);
			found = 0;
		}

		if (found) {
			ENSURE_LONG(z_mode);
			ENSURE_LONG(z_size);
			ENSURE_LONG(z_mtime);

			tssb = (php_stream_statbuf *)ut_eallocate(NULL,(ssb_len=sizeof(*tssb))+1);
			memset(tssb, 0, ssb_len+1);

			tssb->sb.st_size = (off_t) Z_LVAL_P(z_size);
			tssb->sb.st_mode = (mode_t) Z_LVAL_P(z_mode);
#ifdef NETWARE
			tssb->sb.st_mtime.tv_sec = Z_LVAL_P(z_mtime);
			tssb->sb.st_atime.tv_sec = Z_LVAL_P(z_mtime);
			tssb->sb.st_ctime.tv_sec = Z_LVAL_P(z_mtime);
#else
			tssb->sb.st_mtime = (time_t) Z_LVAL_P(z_mtime);
			tssb->sb.st_atime = (time_t) Z_LVAL_P(z_mtime);
			tssb->sb.st_ctime = (time_t) Z_LVAL_P(z_mtime);
#endif
			tssb->sb.st_nlink = 1;
#ifdef HAVE_ST_RDEV
			tssb->sb.st_rdev = -1;
#endif
#ifdef HAVE_ST_BLKSIZE
			tssb->sb.st_blksize = -1;
#endif
#ifdef HAVE_ST_BLOCKS
			tssb->sb.st_blocks = -1;
#endif
		} else {
			tssb = ut_eallocate(NULL,1);
			*((char *) tssb) = '\0';
			ssb_len = 0;
		}
		ZVAL_STRINGL(z_ssb, ((char *) tssb), ssb_len, 0);

		if (zend_is_true(z_cache)
			&& (!is_last_cached_opcode(Z_STRVAL_P(z_uri), Z_STRLEN_P(z_uri) TSRMLS_CC))
			&& PHK_Mgr_cache_enabled(dp->z_mnt, 0, dp->z_command
				, dp->z_params, dp->z_path TSRMLS_CC))
			PHK_Cache_set(z_key, z_ssb TSRMLS_CC);
	} else {
		if (Z_STRLEN_P(z_ssb) == 0) found = 0;	/* Negative hit */
	}

	if (found) memmove(ssb, Z_STRVAL_P(z_ssb), sizeof(*ssb));
	else {
		php_stream_wrapper_log_error(wrapper, dp->show_errors TSRMLS_CC,
									 "%s: File not found",
									 Z_STRVAL_P(dp->z_path));
		ABORT_PHK_STREAM_DO_STAT();
	}

	CLEANUP_PHK_STREAM_DO_STAT();
	DBG_MSG("Exiting do_stat()");

	return 0;
}

/*--------------------*/

static int PHK_Stream_fstat(php_stream * stream, php_stream_statbuf * ssb
							TSRMLS_DC)
{
	PHK_STREAM_DATA *dp = stream->abstract;

	if (!ssb) return -1;

	return do_stat(stream->wrapper, stream->orig_path, dp, ssb TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static size_t PHK_Stream_readdir(php_stream * stream, char *buf,
								 size_t count TSRMLS_DC)
{
	php_stream_dirent *ent = (php_stream_dirent *) buf;
	PHK_STREAM_DATA *dp = stream->abstract;
	HashTable *ht;
	zval **z_tmp_pp;

	ht = Z_ARRVAL_P(dp->z_data);

	if (zend_hash_get_current_data(ht, (void **) (&z_tmp_pp)) == FAILURE) {
		stream->eof = 1;
		return 0;
	}

	count =	MIN(sizeof(ent->d_name) - 1, (unsigned int) Z_STRLEN_PP(z_tmp_pp));
	memmove(ent->d_name, Z_STRVAL_PP(z_tmp_pp), count + 1);

	zend_hash_move_forward(ht);
	stream->eof = (zend_hash_has_more_elements(ht) == SUCCESS);

	return sizeof(*ent);
}

/*--------------------*/

static int PHK_Stream_seekdir(php_stream * stream, off_t offset,
							  int whence, off_t * newoffset TSRMLS_DC)
{
	PHK_STREAM_DATA *dp = stream->abstract;
	HashTable *ht;

	ht = Z_ARRVAL_P(dp->z_data);
	if ((whence == SEEK_SET) && (offset == 0)) {	/* rewinddir */
		zend_hash_internal_pointer_reset(ht);
		stream->eof = (zend_hash_has_more_elements(ht) == SUCCESS);
		if (newoffset) (*newoffset) = 0;
		return 0;
	}

	return -1;
}

/*---------------------------------------------------------------*/

static php_stream_ops phk_ops = {
	PHK_Stream_write,
	PHK_Stream_read,
	PHK_Stream_close,
	PHK_Stream_flush,
	"phk",
	PHK_Stream_seek,
	NULL,
	PHK_Stream_fstat,
	NULL
};

static php_stream_ops phk_dirops = {
	NULL,
	PHK_Stream_readdir,
	PHK_Stream_close,
	NULL,
	"phk",
	PHK_Stream_seekdir,
	NULL,
	NULL,
	NULL
};

/*---------------------------------------------------------------*/

#define INIT_PHK_STREAM_OPEN() \
	{ \
	ALLOC_INIT_ZVAL(z_key); \
	ALLOC_INIT_ZVAL(z_cache); \
	ALLOC_INIT_ZVAL(z_tmp); \
	ALLOC_INIT_ZVAL(z_uri); \
	}

#define CLEANUP_PHK_STREAM_OPEN() \
	{ \
	ut_ezval_ptr_dtor(&z_key); \
	ut_ezval_ptr_dtor(&z_cache); \
	ut_ezval_ptr_dtor(&z_tmp); \
	ut_ezval_ptr_dtor(&z_uri); \
	}

#define ABORT_PHK_STREAM_OPEN() \
	{ \
	DBG_MSG("Aborting generic_open()"); \
	zend_clear_exception(TSRMLS_C); \
	CLEANUP_PHK_STREAM_OPEN(); \
	free_dp(&dp); \
	return NULL; \
	}

static php_stream *PHK_Stream_generic_open(int dir
	, php_stream_wrapper * wrapper, char *uri, char *mode, int options
	, char **opened_path, php_stream_context *context STREAMS_DC TSRMLS_DC)
{
	PHK_STREAM_DATA *dp = NULL;
	int uri_len;
	zval *z_key, *z_cache, *z_tmp, *z_uri;

	DBG_MSG2("Starting generic_open(%s,%s)", (dir ? "dir" : "file"), uri);

	INIT_PHK_STREAM_OPEN();

	uri_len = strlen(uri);
	ZVAL_STRINGL(z_uri, uri, uri_len, 1);

	/*-- Persitent open not supported */

	if (options & STREAM_OPEN_PERSISTENT) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Unable to open %s persistently", uri);
		ABORT_PHK_STREAM_OPEN();
	}

	/*-- Support only read-only mode (file open only) */

	if ((!dir) && ((mode[0] != 'r') || (mode[1] && mode[1] != 'b'))) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"`%s' mode not supported (read-only)", mode);
		ABORT_PHK_STREAM_OPEN();
	}

	/*-- Allocate the private data */

	dp = new_dp(options & REPORT_ERRORS);

	/*-- Parse the URI */

	PHK_Stream_parse_uri(z_uri, dp->z_command, dp->z_params
						   , dp->z_mnt, dp->z_path TSRMLS_CC);
	if (EG(exception)) {
		DBG_MSG("generic_open:Invalid uri");
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
									 "%s: Invalid PHK URI", uri);
		ABORT_PHK_STREAM_OPEN();
	}

	dp->parse_done = 1;

	/*-- Validate the mount point (because data can be cached) */

	if (Z_TYPE_P(dp->z_mnt) != IS_NULL) {
		(void) PHK_Mgr_get_mnt(dp->z_mnt, 0, 1 TSRMLS_CC);
		if (EG(exception)) {
			DBG_MSG1("generic_open: Invalid mount point (%s)", Z_STRVAL_P(dp->z_mnt));
			ABORT_PHK_STREAM_OPEN();
		}
	}

	PHK_Stream_get_file(dir, dp->z_data, z_uri, dp->z_mnt,
						dp->z_command
						, dp->z_params, dp->z_path, NULL TSRMLS_CC);

	if (EG(exception) || ZVAL_IS_NULL(dp->z_data)) {
		DBG_MSG1("generic_open(%s): file not found", uri);
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"%s: File not found", Z_STRVAL_P(dp->z_path));
		ABORT_PHK_STREAM_OPEN();
	}

	if (!dir) dp->offset = 0;	/*-- Initialize offset */
	else {
		/*DBG_MSG1("Nb entries: %d",zend_hash_num_elements(Z_ARRVAL(dp->z_data))); */
		zend_hash_internal_pointer_reset(Z_ARRVAL_P(dp->z_data));
	}

	if (opened_path) (*opened_path) = estrdup(uri);

	CLEANUP_PHK_STREAM_OPEN();
							/*-- Cleanup */
	DBG_MSG("Exiting generic_open()");

	return php_stream_alloc((dir ? &phk_dirops : &phk_ops), dp, NULL, mode);
}

/*--------------------*/

static php_stream *PHK_Stream_openfile(php_stream_wrapper * wrapper,
	char *uri, char *mode, int options, char **opened_path,
	php_stream_context *context STREAMS_DC TSRMLS_DC)
{
	return PHK_Stream_generic_open(0, wrapper, uri, mode, options,
		 opened_path, context STREAMS_CC TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static int PHK_Stream_url_stat(php_stream_wrapper *wrapper, char *uri,
	int flags, php_stream_statbuf * ssb, php_stream_context * context TSRMLS_DC)
{
	PHK_STREAM_DATA *dp;
	int retval;

	dp = new_dp((flags & PHP_STREAM_URL_STAT_QUIET) ? 0 : REPORT_ERRORS);
	retval = do_stat(wrapper, uri, dp, ssb TSRMLS_CC);
	free_dp(&dp);

	return retval;
}

/*--------------------*/

static php_stream *PHK_Stream_opendir(php_stream_wrapper * wrapper
	,char *uri, char *mode, int options, char **opened_path
	, php_stream_context *context STREAMS_DC TSRMLS_DC)
{
	return PHK_Stream_generic_open(1, wrapper, uri, mode, options,
		opened_path, context STREAMS_CC TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void PHK_Stream_parse_uri(zval * uri, zval * z_command
	, zval * z_params, zval * z_mnt, zval * z_path TSRMLS_DC)
{
	char *cmdp, *ampp, *path, *p, *res, *urip;
	int cmd_len, path_len, mnt_len, slash1, uri_len;

	DBG_MSG("Entering PHK_Stream_parse_uri");
	DBG_MSG1("parse_uri: on entry, uri=<%s>",Z_STRVAL_P(uri));

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

	/*DBG_MSG2("parse_uri: after stripping, uri=<%s> - uri_len=%d",urip,uri_len);*/

	for (p=urip, cmdp=ampp=NULL, mnt_len=uri_len, slash1=1, cmd_len=0;; p++) {
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
				  DBG_MSG1("parse_uri: calling sapi_module.treat_data, ampp=<%s>",ampp);
				  sapi_module.treat_data(PARSE_STRING, res,z_params TSRMLS_CC);
			  }
			  break;
		}
	}

	if (cmdp && (cmd_len == 0))
		cmd_len = p - cmdp;		/* Command without argument */

	p = &(urip[uri_len - 1]);
	while (uri_len && (*(p--) == '/')) uri_len--; /* Suppress trailing '/' */

	/*DBG_MSG2("parse_uri: after stripping2, uri=<%s> - uri_len=%d",urip,uri_len);*/
	/*DBG_MSG1("parse_uri: after stripping2, mnt_len=%d",mnt_len);*/
	/*DBG_MSG1("parse_uri: after stripping2, cmd_len=%d",cmd_len);*/

	path_len = 0;
	path = "";
	if ((mnt_len = MIN(uri_len, mnt_len)) != 0) {	/* Not a global command */
		if (uri_len > (mnt_len + 1)) {
			path_len = uri_len - mnt_len - 1;
			path = &(urip[mnt_len + 1]);
		}
	}

	if ((!cmdp) && (!mnt_len)) THROW_EXCEPTION("Empty URI");

/* Return values */

	if (z_command) {
		zval_dtor(z_command);
		if (cmdp) {
			ZVAL_STRINGL(z_command, cmdp, cmd_len, 1);
		} else {
			ZVAL_NULL(z_command);
		}
		DBG_MSG2("Exiting parse_uri (%s): command=|%s|",Z_STRVAL_P(uri)
			,(ZVAL_IS_NULL(z_command) ? "<null>" : Z_STRVAL_P(z_command)));
	}

	if (z_mnt) {
		zval_dtor(z_mnt);
		if (mnt_len) {
			ZVAL_STRINGL(z_mnt, urip, mnt_len, 1);
		} else {
			ZVAL_NULL(z_mnt);
		}
		DBG_MSG2("Exiting parse_uri (%s): mnt=|%s|",Z_STRVAL_P(uri)
			,(ZVAL_IS_NULL(z_mnt) ? "<null>" : Z_STRVAL_P(z_mnt)));
	}

	if (z_path) {
		zval_dtor(z_path);
		if (path_len) {
			ZVAL_STRINGL(z_path, path, path_len, 1);
		} else {
			ZVAL_NULL(z_path);
		}
		DBG_MSG2("Exiting parse_uri (%s): path=|%s|",Z_STRVAL_P(uri)
			,(ZVAL_IS_NULL(z_path) ? "<null>" : Z_STRVAL_P(z_path)));
	}
}

/*--------------------*/
/* A PHK URI is opcode-cacheable if :
	- it corresponds to a currently mounted package,
	- and it does not contain any '?' character,
	- and the corresponding package does not set the 'no_opcode_cache' option.

  As the URI is unique, we can use it as key */

static char *PHK_Stream_cache_key(php_stream_wrapper * wrapper,
								  const char *uri, int uri_len,
								  int *key_len TSRMLS_DC)
{
	char *p, *mnt_start, *mnt_end;
	zval *mnt;
	PHK_Mnt *mp;

	if (uri_len < 6) return NULL;

	mnt_start = p = (char *) (uri + 6);
	mnt_end = NULL;

	while (*p) {
		if (((*p) == '/') && (!mnt_end)) mnt_end = p;
		if (*(p++) == '?') return NULL;
	}
	if (!mnt_end) return NULL;

	(*mnt_end) = '\0';
	ALLOC_INIT_ZVAL(mnt);
	ZVAL_STRINGL(mnt, mnt_start, mnt_end - mnt_start, 1);
	(*mnt_end) = '/';

	mp = PHK_Mgr_get_mnt(mnt, 0, 0 TSRMLS_CC);
	ut_ezval_ptr_dtor(&mnt);
	if ((!mp) || mp->no_opcode_cache) return NULL;

	set_last_cached_opcode(uri, uri_len TSRMLS_CC);

	(*key_len) = uri_len;
	return (char *) uri;
}

/*---------------------------------------------------------------*/

static php_stream_wrapper_ops phk_stream_wops = {
	PHK_Stream_openfile,		/* open */
	NULL,						/* close */
	NULL,						/* stat, */
	PHK_Stream_url_stat,		/* stat_url */
	PHK_Stream_opendir,			/* opendir */
	"phk",
	NULL,						/* unlink */
	NULL,						/* rename */
	NULL,						/* mkdir */
	NULL						/* rmdir */
#ifdef STREAMS_SUPPORT_CACHE_KEY
	, PHK_Stream_cache_key		/* cache_key */
#endif
};

static php_stream_wrapper php_stream_phk_wrapper = {
	&phk_stream_wops,
	NULL,
	0,							/* is_url */
	0, NULL
};

/*---------------------------------------------------------------*/

ZEND_BEGIN_ARG_INFO_EX(PHK_Stream_get_file_arginfo, 0, 0, 6)
ZEND_ARG_INFO(0, dir)
ZEND_ARG_INFO(0, uri)
ZEND_ARG_INFO(0, mnt)
ZEND_ARG_INFO(0, command)
ZEND_ARG_ARRAY_INFO(0, params, 1)
ZEND_ARG_INFO(0, path)
ZEND_ARG_INFO(0, cache)
ZEND_END_ARG_INFO()

static zend_function_entry PHK_Stream_functions[] = {
	PHP_ME(PHK_Stream, get_file, PHK_Stream_get_file_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_PHK_Stream(TSRMLS_D)
{
	zend_class_entry ce;

	php_register_url_stream_wrapper("phk", &php_stream_phk_wrapper TSRMLS_CC);

	/*-- Define PHK_Stream class */

	INIT_CLASS_ENTRY(ce, "PHK_Stream", PHK_Stream_functions);
	zend_register_internal_class(&ce TSRMLS_CC);

	return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Module shutdown                                               */

static int MSHUTDOWN_PHK_Stream(TSRMLS_D)
{
	php_unregister_url_stream_wrapper("phk" TSRMLS_CC);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RINIT_PHK_Stream(TSRMLS_D)
{
	clear_last_cached_opcode(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RSHUTDOWN_PHK_Stream(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/
