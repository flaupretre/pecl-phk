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

/*---------------------------------------------------------------*/

static void PHK_set_mp_property(zval * obj, int order TSRMLS_DC)
{
	zend_update_property_long(Z_OBJCE_P(obj),obj,"m",1,(long)order TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* Slow path */

static void PHK_need_php_runtime(TSRMLS_D)
{
	FILE *fp;
	int size, offset,nb_read;
	char buf1[242], *buf=NULL;

	if (PHK_G(php_runtime_is_loaded)) return;

	if (HKEY_EXISTS(EG(class_table), phk_stream_backend)) {
		PHK_G(php_runtime_is_loaded) = 1;
		return;
	}

	if (!PHK_G(root_package)[0])
		EXCEPTION_ABORT
			("Internal error - Cannot load PHP runtime code because root_package is not set");

	DBG_MSG1("Loading PHP runtime code from %s", PHK_G(root_package));

	if (!(fp = fopen(PHK_G(root_package), "rb")))
		EXCEPTION_ABORT_1
			("Cannot load PHP runtime code - Unable to open file %s",
			 PHK_G(root_package));

	nb_read=fread(buf1, 1, 241, fp);
	if (nb_read != 241)	EXCEPTION_ABORT("Cannot load PHP runtime code - Cannot get offset/size");
	buf1[224]='\0'; /* Avoids valgrind warning */
	sscanf(buf1 + 212, "%d", &offset);
	buf1[239]='\0'; /* Avoids valgrind warning */
	sscanf(buf1 + 227, "%d", &size);

	EALLOCATE(buf, size + 1);

	fseek(fp, offset, SEEK_SET);
	nb_read=fread(buf, 1, size, fp);
	if (nb_read != size) EXCEPTION_ABORT("Cannot load PHP runtime code - Cannot get code");
	fclose(fp);
	buf[size] = '\0';
	zend_eval_string(buf, NULL, "PHK runtime code (PHP)" TSRMLS_CC);

	EALLOCATE(buf,0);

	PHK_G(php_runtime_is_loaded) = 1;
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK::need_php_runtime() */

/* Undocumented - Used by PHK related software like PHK_Signer. Allows */
/* to use PHK_Proxy, PHK_Util... even if the extension did not need them */

static PHP_METHOD(PHK, need_php_runtime)
{
	PHK_need_php_runtime(TSRMLS_C);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Here, we don't check for required extensions, as it is checked only once
   when the package info is read in persistent storage */

static void PHK_init(PHK_Mnt * mp TSRMLS_DC)
{
	Automap_Mnt *automap_mp;

	if (!mp->pdata->init_done) {
		/* Always check CRC, but only once */
		PHK_crc_check(mp TSRMLS_CC);
		if (EG(exception)) return;

		mp->pdata->init_done=1;
	}

	if (mp->automap_uri) {	/* Load map */
		automap_mp=Automap_Mnt_load_extended(mp->automap_uri,mp->mnt,mp->hash
			,mp->base_uri TSRMLS_CC);
		if (EG(exception)) return;
		mp->automap_id=automap_mp->id;
	}

	if (mp->mount_script_uri
		&& (!(Z_LVAL_P(mp->flags) & PHK_F_NO_MOUNT_SCRIPT))) {
		ut_require(Z_STRVAL_P(mp->mount_script_uri), NULL TSRMLS_CC);
		if (EG(exception)) return;
	}

	if (mp->plugin_class) {
		mp->plugin=ut_new_instance(Z_STRVAL_P(mp->plugin_class)
			,Z_STRLEN_P(mp->plugin_class),1,1,&(mp->mnt) TSRMLS_CC);
		if (EG(exception)) return;
	}
}

/*---------------------------------------------------------------*/
/* {{{ proto bool PHK::map_defined() */

static PHP_METHOD(PHK, map_defined)
{
	PHK_GET_INSTANCE_DATA(map_defined)

	RETVAL_BOOL(mp->automap_uri);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto void PHK::set_cache(bool|null toggle) */

static PHP_METHOD(PHK, set_cache)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(set_cache)

		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
			FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	SEPARATE_ARG_IF_REF(zp);
	ut_ezval_ptr_dtor(&(mp->caching));
	mp->caching = zp;
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto bool PHK::file_is_package(string path) */

static PHP_METHOD(PHK, file_is_package)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_need_php_runtime(TSRMLS_C);
	RETVAL_BOOL(ut_call_user_function_bool(NULL
		, ZEND_STRL("PHK_Proxy::file_is_package"), 1, &zp TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto bool PHK::data_is_package(string data) */

static PHP_METHOD(PHK, data_is_package)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_need_php_runtime(TSRMLS_C);
	RETVAL_BOOL(ut_call_user_function_bool(NULL
	   , ZEND_STRL("PHK_Proxy::data_is_package"), 1, &zp TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/

static int PHK_cache_enabled(PHK_Mnt * mp, zval * command,
							 zval * params, zval * path TSRMLS_DC)
{
	if (mp->no_cache || (Z_LVAL_P(mp->flags) & PHK_F_CREATOR)
		|| (!PHK_Cache_cache_present(TSRMLS_C))) return 0;

	if (!ZVAL_IS_NULL(mp->caching)) return Z_BVAL_P(mp->caching);

	return 1;
}

/*---------------------------------------------------------------*/

static void PHK_umount(PHK_Mnt * mp TSRMLS_DC)
{
	if (mp->plugin) ut_ezval_ptr_dtor(&(mp->plugin));

	if (mp->umount_script_uri
		&& (!(Z_LVAL_P(mp->flags) & PHK_F_NO_MOUNT_SCRIPT))) {
		ut_require(Z_STRVAL_P(mp->umount_script_uri), NULL TSRMLS_CC);
		/* Don't catch exception here. Umount automap first */
	}

	if (mp->automap_uri) {
		Automap_unload(mp->automap_id);
	}
}

/*---------------------------------------------------------------*/

#define PHK_GET_PROPERTY_METHOD_OR_NULL(_field) \
	static PHP_METHOD(PHK, _field) \
	{ \
		PHK_GET_INSTANCE_DATA(_field) \
		if (mp->_field) RETVAL_BY_REF(mp->_field); \
	}

/* {{{ proto string PHK::mnt() */
PHK_GET_PROPERTY_METHOD_OR_NULL(mnt)
/* }}} */

/* {{{ proto string PHK::path() */
PHK_GET_PROPERTY_METHOD_OR_NULL(path)
/* }}} */

/* {{{ proto int PHK::flags() */
PHK_GET_PROPERTY_METHOD_OR_NULL(flags)
/* }}} */

/* {{{ proto int PHK::mtime() */
PHK_GET_PROPERTY_METHOD_OR_NULL(mtime)
/* }}} */

/* {{{ proto array PHK::options() */
PHK_GET_PROPERTY_METHOD_OR_NULL(options)
/* }}} */

/* {{{ proto string PHK::base_uri() */
PHK_GET_PROPERTY_METHOD_OR_NULL(base_uri)
/* }}} */

/* {{{ proto string|null PHK::automap_uri() */
PHK_GET_PROPERTY_METHOD_OR_NULL(automap_uri)
/* }}} */

/* {{{ proto object|null PHK::plugin() */
PHK_GET_PROPERTY_METHOD_OR_NULL(plugin)
/* }}} */

/*---------------------------------------------------------------*/
/* {{{ proto long PHK::automap_id() */

static PHP_METHOD(PHK, automap_id)
{
	PHK_GET_INSTANCE_DATA(automap_id) \

	RETVAL_LONG(mp->automap_id);
}

	/*---------------------------------------------------------------*/
/* {{{ proto string PHK::uri(string path) */

static PHP_METHOD(PHK, uri)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(uri)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_uri(mp->mnt, zp, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string PHK::section_uri(string section) */

static PHP_METHOD(PHK, section_uri)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(section_uri)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_section_uri(mp->mnt, zp, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string PHK::command_uri(string command) */

static PHP_METHOD(PHK, command_uri)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(command_uri)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_command_uri(mp->mnt, zp, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto mixed PHK::option(string opt) */

static PHP_METHOD(PHK, option)
{
	char *opt;
	int optlen;
	zval **zpp;

	PHK_GET_INSTANCE_DATA(option)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &opt, &optlen) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	DBG_MSG1("Entering PHK::option(%s)", opt);

	if (zend_hash_find(Z_ARRVAL_P(mp->options), opt, optlen + 1,
		(void **)(&zpp)) == SUCCESS) {
		RETVAL_BY_REF(*zpp);
	}
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string|null PHK::parent_mnt() */

static PHP_METHOD(PHK, parent_mnt)
{
	PHK_GET_INSTANCE_DATA(parent_mnt)

	if (mp->parent) RETVAL_BY_REF(mp->parent->mnt);
}

/* }}} */
/*---------------------------------------------------------------*/

static int web_access_matches(zval * apath, zval * path TSRMLS_DC)
{
	int alen, plen;

	alen = Z_STRLEN_P(apath);
	plen = Z_STRLEN_P(path);

	if ((alen == 1) && (Z_STRVAL_P(apath)[0] == '/'))
		return 1;

	return ((plen >= alen)
			&& ((alen == plen) || (Z_STRVAL_P(path)[alen] == '/'))
			&& (!memcmp(Z_STRVAL_P(path), Z_STRVAL_P(apath), alen)));
}

/*---------------------------------------------------------------*/
/* mp->web_access can only be array|string (type is filtered and forced
  to string if not array when computing persistent data in PHK_Mgr) */

static int web_access_allowed(PHK_Mnt * mp, zval * path TSRMLS_DC)
{
	HashPosition pos;
	zval **apath_pp;
	HashTable *ht;

	if (mp->web_access) {
		if (Z_TYPE_P(mp->web_access) == IS_ARRAY) {
			ht = Z_ARRVAL_P(mp->web_access);
			zend_hash_internal_pointer_reset_ex(ht, &pos);
			while (zend_hash_get_current_data_ex
				   (ht, (void **) (&apath_pp), &pos) == SUCCESS) {
				if (Z_TYPE_PP(apath_pp) != IS_STRING)
					continue;
				if (web_access_matches(*apath_pp, path TSRMLS_CC))
					return 1;
				zend_hash_move_forward_ex(ht, &pos);
			}
		} else {
			return web_access_matches(mp->web_access, path TSRMLS_CC);
		}
	}
	return 0;
}

/*---------------------------------------------------------------*/

static char *goto_main(PHK_Mnt * mp TSRMLS_DC)
{
	char *p;
	zval *zp;

	if (mp->web_main_redirect) {
		ut_http_301_redirect(Z_STRVAL_P(mp->web_run_script), 0 TSRMLS_CC);
		if (EG(exception)) return "";
	} else {
		MAKE_STD_ZVAL(zp);
		PHK_Mgr_uri(mp->mnt, mp->web_run_script, zp TSRMLS_CC);
		spprintf(&p, 1024, "require('%s');", Z_STRVAL_P(zp));
		ut_ezval_ptr_dtor(&zp);
	}
	return p;
}

/*---------------------------------------------------------------*/
/* Returns an allocated value */

#define CLEANUP_WEB_TUNNEL() \
	{ \
	ut_ezval_ptr_dtor(&tpath); \
	ut_ezval_ptr_dtor(&uri); \
	}

static char *web_tunnel(PHK_Mnt * mp, zval * path,
						int webinfo TSRMLS_DC)
{
	int last_slash, found;
	zval *tpath, *uri;
	php_stream_statbuf ssb, ssb2;
	char **pp, *p;

	tpath=NULL;
	uri=NULL;

	if (!path) {
		MAKE_STD_ZVAL(tpath);
		PHK_get_subpath(tpath TSRMLS_CC);
		if (EG(exception)) {
			CLEANUP_WEB_TUNNEL();
			return "";
		}
	} else {
		tpath = path;
		SEPARATE_ARG_IF_REF(tpath);
		convert_to_string(tpath);
	}

	last_slash = (Z_STRLEN_P(tpath)
		&& (Z_STRVAL_P(tpath)[Z_STRLEN_P(tpath) - 1] == '/'));

	if (last_slash && (Z_STRLEN_P(tpath) != 1))
		ut_rtrim_zval(tpath TSRMLS_CC);

	if (!Z_STRLEN_P(tpath)) {
		CLEANUP_WEB_TUNNEL();
		if (mp->web_run_script)
			return goto_main(mp TSRMLS_CC);
		else {
			ut_http_301_redirect("/", 0 TSRMLS_CC);
			if (EG(exception)) return "";
		}
	}

	if ((!webinfo) && (!web_access_allowed(mp, tpath TSRMLS_CC))
		&& (!ut_strings_are_equal(tpath, mp->web_run_script TSRMLS_CC))) {
		CLEANUP_WEB_TUNNEL();
		if (mp->web_run_script)
			return goto_main(mp TSRMLS_CC);
		else
			ut_http_403_fail(TSRMLS_C);
	}

	ALLOC_INIT_ZVAL(uri);
	PHK_Mgr_uri(mp->mnt, tpath, uri TSRMLS_CC);

	if (php_stream_stat_path(Z_STRVAL_P(uri), &ssb) != 0) {
		CLEANUP_WEB_TUNNEL();
		ut_http_404_fail(TSRMLS_C);
	}

	if (S_ISDIR(ssb.sb.st_mode))	/* Directory autoindex */
	{
		found = 0;
		if (last_slash) {
			for (pp = autoindex_fnames; *pp; p++) {
				spprintf(&p, UT_PATH_MAX, "%s%s", Z_STRVAL_P(uri), *pp);
				if ((php_stream_stat_path(p, &ssb2) == 0)
					&& S_ISREG(ssb2.sb.st_mode)) {
					found = 1;
					zval_dtor(tpath);
					ZVAL_STRING(tpath, p, 0);
					zval_dtor(uri);
					PHK_Mgr_uri(mp->mnt, tpath, uri TSRMLS_CC);
					break;
				}
				EALLOCATE(p,0);
			}
			if (!found) {
				CLEANUP_WEB_TUNNEL();
				ut_http_404_fail(TSRMLS_C);
			}
		} else {
			spprintf(&p, UT_PATH_MAX, "%s/", Z_STRVAL_P(uri));
			CLEANUP_WEB_TUNNEL();
			ut_http_301_redirect(p, 1 TSRMLS_CC);
			if (EG(exception)) return "";
		}
	}

/* Here, the path is in tpath and the corresponding uri is in uri */

	if ((!webinfo) && (PHK_is_php_source_path(mp, tpath TSRMLS_CC))) {
		spprintf(&p, UT_PATH_MAX, "require('%s');", Z_STRVAL_P(uri));
	} else {
		spprintf(&p, UT_PATH_MAX,
				 "PHK_Mgr::mime_header('%s','%s'); readfile('%s');",
				 Z_STRVAL_P(mp->mnt), Z_STRVAL_P(tpath),
				 Z_STRVAL_P(uri));
	}

	DBG_MSG1("web_tunnel command: %s",p);

	CLEANUP_WEB_TUNNEL();
	return p;
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK::web_tunnel([ string path [, bool webinfo ]]) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK, web_tunnel)
{
	zval *path;
	int webinfo;

	PHK_GET_INSTANCE_DATA(web_tunnel)

	path = NULL;
	webinfo = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|zb", &path, &webinfo)
		== FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	RETVAL_STRING(web_tunnel(mp, path, webinfo TSRMLS_CC), 0);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_mime_header(PHK_Mnt * mp, zval * path TSRMLS_DC)
{
	zval *type;
	char *p;

	ALLOC_INIT_ZVAL(type);
	PHK_mime_type(type, mp, path TSRMLS_CC);
	if (ZVAL_IS_STRING(type)) {
		spprintf(&p, UT_PATH_MAX, "Content-type: %s", Z_STRVAL_P(type));
		ut_header(200, p TSRMLS_CC);
		EALLOCATE(p,0);
	}

	ut_ezval_ptr_dtor(&type);
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK::mime_header(string path) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK, mime_header)
{
	zval *path;

	PHK_GET_INSTANCE_DATA(mime_header)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_mime_header(mp, path TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* If no mime type associated with suffix, returns a NULL zval */

static void PHK_mime_type(zval *ret, PHK_Mnt * mp, zval * path TSRMLS_DC)
{
	zval *suffix, **zpp;

	ut_ezval_dtor(ret);
	INIT_PZVAL(ret);

	ALLOC_INIT_ZVAL(suffix);
	ut_file_suffix(path, suffix TSRMLS_CC);

	if (mp->mime_types && zend_hash_find(Z_ARRVAL_P(mp->mime_types)
		, Z_STRVAL_P(suffix), Z_STRLEN_P(suffix) + 1
		, (void **) (zpp)) == SUCCESS) {
		ZVAL_COPY_VALUE(ret,*zpp);
		zval_copy_ctor(ret);
	} else {
		init_mime_table(TSRMLS_C);
		if (zend_hash_find
			(Z_ARRVAL_P(PHK_G(mime_table)), Z_STRVAL_P(suffix), Z_STRLEN_P(suffix) + 1,
			(void **) (&zpp)) == SUCCESS) { /* Copy val to dynamic storage */
			ZVAL_COPY_VALUE(ret,*zpp);
			zval_copy_ctor(ret);
		} else {
			if (strstr(Z_STRVAL_P(suffix), "php")) {
				ZVAL_STRINGL(ret,"application/x-httpd-php",23,1);
			}
		}
	}

	ut_ezval_ptr_dtor(&suffix);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK::mime_type(string path) */

static PHP_METHOD(PHK, mime_type)
{
	zval *path;

	PHK_GET_INSTANCE_DATA(mime_type)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	PHK_mime_type(return_value, mp, path TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static int PHK_is_php_source_path(PHK_Mnt * mp, zval * path TSRMLS_DC)
{
	zval *type;
	int res;

	ALLOC_INIT_ZVAL(type);
	PHK_mime_type(type, mp, path TSRMLS_CC);

	res=0;
	if (ZVAL_IS_STRING(type) && (Z_STRLEN_P(type)==23)
		&& (!memcmp(Z_STRVAL_P(type),"application/x-httpd-php",23))) res=1;
	
	ut_ezval_ptr_dtor(&type);
	return res;
}

/*---------------------------------------------------------------*/
/* {{{ proto bool PHK::is_php_source_path(string path) */

static PHP_METHOD(PHK, is_php_source_path)
{
	zval *path;

	PHK_GET_INSTANCE_DATA(is_php_source_path)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	RETVAL_BOOL(PHK_is_php_source_path(mp, path TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto void PHK::proxy() */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK, proxy)
{
	zval *zp;

	PHK_GET_INSTANCE_DATA(proxy)

	zp = PHK_Mgr_proxy_by_mp(mp TSRMLS_CC);

	RETVAL_BY_REF(zp);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_crc_check(PHK_Mnt * mp TSRMLS_DC)
{
	PHK_need_php_runtime(TSRMLS_C);
	ut_call_user_function_void(PHK_Mgr_proxy_by_mp(mp TSRMLS_CC),
		ZEND_STRL("crc_check"), 0, NULL TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto bool PHK::accelerator_is_present() */

static PHP_METHOD(PHK, accelerator_is_present)
{
	RETVAL_TRUE;
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto mixed PHK::build_info([ string name ]) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK, build_info)
{
	zval *zp, **zpp;

	PHK_GET_INSTANCE_DATA(build_info)

		zp = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z!", &zp) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	if (!zp) {
		RETVAL_BY_REF(mp->build_info);
		return;
	}

	if ((Z_TYPE_P(zp) == IS_STRING)
		&& zend_hash_find(Z_ARRVAL_P(mp->build_info)
						  , Z_STRVAL_P(zp), Z_STRLEN_P(zp) + 1,
						  (void **) &zpp) == SUCCESS) {
		RETVAL_BY_REF(*zpp);
	} else {
		EXCEPTION_ABORT_1("%s: unknown build info", Z_STRVAL_P(zp));
	}
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string PHK::subpath_url(string path) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK, subpath_url)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	PHK_need_php_runtime(TSRMLS_C);
	ut_call_user_function_string(NULL, ZEND_STRL("PHK_Backend::subpath_url")
		, return_value, 1, &zp TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_get_subpath(zval * ret TSRMLS_DC)
{
	zval *zp;
	char *p=NULL;
	int len, decode, s1;

	zp = REQUEST_ELEMENT(_PHK_path);
	if (EG(exception)) return;
	if (zp) decode = 1;
	else {
		decode = 0;
		zp = SERVER_ELEMENT(PATH_INFO);
		if (EG(exception)) return;
		if (!zp) zp = SERVER_ELEMENT(ORIG_PATH_INFO);
		if (!zp) {
			ZVAL_STRINGL(ret, "", 0, 1);
			return;
		}
	}

	if (Z_TYPE_P(zp) != IS_STRING) convert_to_string(zp);

	s1 = Z_STRVAL_P(zp)[0] == '/' ? 0 : 1;
	EALLOCATE(p, Z_STRLEN_P(zp) + s1 + 1);

	if (s1) (*p) = '/';
	memmove(p + s1, Z_STRVAL_P(zp), Z_STRLEN_P(zp) + 1);

	len = php_url_decode(p, Z_STRLEN_P(zp) + s1);
	ZVAL_STRINGL(ret, p, len, 0);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK::subpath_url() */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK, get_subpath)
{
	PHK_get_subpath(return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static zval *PHK_backend(PHK_Mnt * mp, zval * phk TSRMLS_DC)
{
	if (!mp->backend) {
		mp->backend=ut_new_instance(ZEND_STRL("PHK_Backend"), 1, 1,	&phk TSRMLS_CC);
	}

	return mp->backend;
}

/*---------------------------------------------------------------*/
/* {{{ proto mixed PHK::__call(mixed args...) */

static PHP_METHOD(PHK, __call)
{
	zval *method, *call_args, *args[3];

	PHK_GET_INSTANCE_DATA(__call)

	if (zend_parse_parameters
			(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &method,
			 &call_args) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	DBG_MSG1("Method: %s", Z_STRVAL_P(method));

	PHK_need_php_runtime(TSRMLS_C);

	args[0] = PHK_backend(mp, getThis()TSRMLS_CC);
	args[1] = method;
	args[2] = call_args;
	ut_call_user_function(NULL, ZEND_STRL("PHK_Util::call_method")
		, return_value, 3, args TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto void PHK::accel_techinfo() */

static PHP_METHOD(PHK, accel_techinfo)
{
	if (sapi_module.phpinfo_as_text) {
		php_printf("Using PHK Accelerator: Yes\n");
		php_printf("Accelerator Version: %s\n", PHK_ACCEL_VERSION);
	} else {
		php_printf("<table border=0>");
		php_printf("<tr><td>Using PHK Accelerator:&nbsp;</td><td>Yes</td></tr>");
		php_printf("<tr><td>Accelerator Version:&nbsp;</td><td>%s</td></tr>",
			 PHK_ACCEL_VERSION);
		php_printf("</table>");
	}
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string PHK::prolog(string file, mixed &cmd, mixed &ret) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK, prolog)
{
	int cli, is_main, status;
	PHK_Mnt *mp;
	zval *file, *cmd, *ret, **argv1_pp, *instance;
	char *p;
	struct stat dummy_sb;

	DBG_MSG("Entering Prolog");

	/* Workaround to PHP bug #39903 (__COMPILER_HALT_OFFSET__ already defined) */
	/* Not perfect but can suppress further notice messages */

	(void)zend_hash_del(EG(zend_constants),"__COMPILER_HALT_OFFSET__"
		,sizeof("__COMPILER_HALT_OFFSET__"));

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zzz", &file, &cmd, &ret) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	if (!ZVAL_IS_STRING(file)) convert_to_string(file);

	if (!PHK_G(root_package)[0]) {	/* Store path to load PHP code later, if needed */
		if (Z_STRLEN_P(file) > UT_PATH_MAX)
			EXCEPTION_ABORT_1("Path too long - max size=%d", UT_PATH_MAX);

		memmove(PHK_G(root_package), Z_STRVAL_P(file), Z_STRLEN_P(file) + 1);
	}

	cli = (!ut_is_web());

	if (cli) {
		(void) zend_alter_ini_entry("display_errors",
									sizeof("display_errors"), "1", 1,
									PHP_INI_USER, PHP_INI_STAGE_RUNTIME);
#if PHP_API_VERSION < 20100412
		if (!PG(safe_mode))
#endif
			(void) zend_alter_ini_entry("memory_limit",
										sizeof("memory_limit"), "1024M", 5,
										PHP_INI_USER,
										PHP_INI_STAGE_RUNTIME);
	}

	PHK_Mgr_php_version_check(TSRMLS_C);

	mp = PHK_Mgr_mount(file, 0 TSRMLS_CC);
	if (EG(exception)) return;					/* TODO: message */

	DBG_MSG1("Mounted on %s", Z_STRVAL_P(mp->mnt));

	is_main = (EG(current_execute_data)->prev_execute_data == NULL);

	DBG_MSG1("Is main: %d", is_main);

	if (!is_main) {
		if (mp->lib_run_script_uri) {
			ut_require(Z_STRVAL_P(mp->lib_run_script_uri), NULL TSRMLS_CC);
		}
		if (mp->auto_umount) {
			PHK_Mgr_umount_mnt(mp TSRMLS_CC);
		} else {
			ZVAL_STRINGL(ret, Z_STRVAL_P(mp->mnt), Z_STRLEN_P(mp->mnt), 1);
		}
		return;
	}

	/* Main script - Dispatch */

	if (cli) {
		if (Z_LVAL_P(SERVER_ELEMENT(argc)) > 1) {
			zend_hash_index_find(Z_ARRVAL_P(SERVER_ELEMENT(argv)),
								 (ulong) 1, (void **) &argv1_pp);
			if (Z_STRVAL_PP(argv1_pp)[0] == '@') {
				PHK_need_php_runtime(TSRMLS_C);
				instance = PHK_Mgr_instance_by_mp(mp TSRMLS_CC);
				ZVAL_LONG(ret, ut_call_user_function_long(instance
					, ZEND_STRL("builtin_prolog"), 1, &file TSRMLS_CC));
				return;
			}
		}

		if (mp->cli_run_command) {
			ZVAL_STRINGL(cmd, Z_STRVAL_P(mp->cli_run_command)
						 , Z_STRLEN_P(mp->cli_run_command), 1);
		}
		return;
	}

	/* HTTP mode */

	spprintf(&p, UT_PATH_MAX, "%s.webinfo", Z_STRVAL_P(file));
	status = stat(p, &dummy_sb);
	EALLOCATE(p,0);
	if (!status) {
		PHK_need_php_runtime(TSRMLS_C);
		instance = PHK_Mgr_instance_by_mp(mp TSRMLS_CC);
		ut_call_user_function_void(NULL, ZEND_STRL("PHK_Util::run_webinfo")
			, 1, &instance TSRMLS_CC);
		return;
	}

	ZVAL_STRING(cmd, web_tunnel(mp, NULL, 0 TSRMLS_CC), 0);
}

/* }}} */
/*---------------------------------------------------------------*/

ZEND_BEGIN_ARG_INFO_EX(PHK_prolog_arginfo, 0, 0, 3)
ZEND_ARG_INFO(0, file)
ZEND_ARG_INFO(1, cmd)
ZEND_ARG_INFO(1, ret)
ZEND_END_ARG_INFO()

/*---------------------------------------------------------------*/

static zend_function_entry PHK_functions[] = {
	PHP_ME(PHK, need_php_runtime, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, map_defined, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, set_cache, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, file_is_package, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, data_is_package, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, mnt, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, flags, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, path, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, mtime, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, options, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, base_uri, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, uri, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, section_uri, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, command_uri, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, automap_uri, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, automap_id, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, option, UT_1arg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, parent_mnt, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, web_tunnel, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, mime_header, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, mime_type, UT_1arg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, is_php_source_path, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, proxy, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, plugin, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, accelerator_is_present, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, build_info, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, subpath_url, UT_1arg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, get_subpath, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, __call, UT_2args_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(PHK, prolog, PHK_prolog_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PHK, accel_techinfo, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/
/* Storing the table in a static zval causes MSHUTDOWN to crash (at least
  in PHP 5.1.0. Workaround: array is initialized before being used and
  stored in PHK_G storage */

static inline void init_mime_table(TSRMLS_D)
{
	mime_entry *mip;

	if (PHK_G(mime_table)) return;

	ALLOC_INIT_ZVAL(PHK_G(mime_table));
	array_init(PHK_G(mime_table));

	for (mip = mime_init; mip->suffix; mip++) {
		add_assoc_string(PHK_G(mime_table), mip->suffix, mip->mime_type, 1);
	}

}

/*---------------------------------------------------------------*/

static void shutdown_mime_table(TSRMLS_D)
{
	ut_ezval_ptr_dtor(&PHK_G(mime_table));
}

/*---------------------------------------------------------------*/

static void set_constants(zend_class_entry * ce)
{
	UT_DECLARE_STRING_CONSTANT(PHP_PHK_VERSION,"VERSION");

	UT_DECLARE_LONG_CONSTANT(PHK_F_CRC_CHECK,"F_CRC_CHECK");
	UT_DECLARE_LONG_CONSTANT(PHK_F_NO_MOUNT_SCRIPT,"F_NO_MOUNT_SCRIPT");
	UT_DECLARE_LONG_CONSTANT(PHK_F_CREATOR,"F_CREATOR");
}

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_PHK(TSRMLS_D)
{
	zend_class_entry ce, *entry;

	/*--- Init class, declare properties and set class constants*/

	INIT_CLASS_ENTRY(ce, "PHK", PHK_functions);
	entry = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_property_null(entry,"m",1,ZEND_ACC_PRIVATE TSRMLS_CC);

	set_constants(entry);

	return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Module shutdown                                               */

static int MSHUTDOWN_PHK(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RINIT_PHK(TSRMLS_D)
{
	PHK_G(root_package)[0] = '\0';

	PHK_G(php_runtime_is_loaded) = 0;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RSHUTDOWN_PHK(TSRMLS_D)
{
	shutdown_mime_table(TSRMLS_C);
	return SUCCESS;
}

/*---------------------------------------------------------------*/
