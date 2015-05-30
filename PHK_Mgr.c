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
/*--- Init persistent data */

static void PHK_Mgr_init_pdata(TSRMLS_D)
{
	MutexSetup(persistent_mtab);

	zend_hash_init(&persistent_mtab, 16, NULL,
				   (dtor_func_t) PHK_Mgr_Persistent_Data_dtor, 1);

	tmp_mnt_num = 0;
}

/*---------------------------------------------------------------*/

static void PHK_Mgr_shutdown_pdata(TSRMLS_D)
{
	zend_hash_destroy(&persistent_mtab);

	MutexShutdown(persistent_mtab);
}

/*---------------------------------------------------------------*/
/* The function can be called during the creation of the structure (mount failure),
   when pointers are not all set. No need to check for null pointers as
   ut_ezval_ptr_dtor() handles this case.
*/

static void PHK_Mgr_mnt_dtor(PHK_Mnt * mp)
{
	TSRMLS_FETCH();

	EALLOCATE(mp->children,0);

	ut_ezval_ptr_dtor(&(mp->mnt));

	if (mp->instance) /* Invalidate object */
		{
		(void)zend_hash_del(Z_OBJPROP_P(mp->instance),PHK_MP_PROPERTY_NAME
			,sizeof(PHK_MP_PROPERTY_NAME));
		}

	ut_ezval_ptr_dtor(&(mp->instance));
	ut_ezval_ptr_dtor(&(mp->proxy));
	ut_ezval_ptr_dtor(&(mp->path));
	ut_ezval_ptr_dtor(&(mp->plugin));
	ut_ezval_ptr_dtor(&(mp->flags));
	ut_ezval_ptr_dtor(&(mp->caching));
	ut_ezval_ptr_dtor(&(mp->mtime));
	ut_ezval_ptr_dtor(&(mp->backend));

	ut_ezval_ptr_dtor(&(mp->minVersion));
	ut_ezval_ptr_dtor(&(mp->options));
	ut_ezval_ptr_dtor(&(mp->buildInfo));
	ut_ezval_ptr_dtor(&(mp->mime_types));
	ut_ezval_ptr_dtor(&(mp->web_run_script));
	ut_ezval_ptr_dtor(&(mp->plugin_class));
	ut_ezval_ptr_dtor(&(mp->web_access));
	ut_ezval_ptr_dtor(&(mp->min_php_version));
	ut_ezval_ptr_dtor(&(mp->max_php_version));

	ut_ezval_ptr_dtor(&(mp->baseURI));
	ut_ezval_ptr_dtor(&(mp->automapURI));
	ut_ezval_ptr_dtor(&(mp->mount_script_uri));
	ut_ezval_ptr_dtor(&(mp->umount_script_uri));
	ut_ezval_ptr_dtor(&(mp->lib_run_script_uri));
	ut_ezval_ptr_dtor(&(mp->cli_run_command));
}

/*---------------------------------------------------------------*/

static void PHK_Mgr_remove_mnt(PHK_Mnt *mp TSRMLS_DC)
{
	if (PHK_G(mtab)) {
		PHK_G(mount_order)[mp->order]=NULL;
		(void) zend_hash_del(PHK_G(mtab), Z_STRVAL_P(mp->mnt), Z_STRLEN_P(mp->mnt) + 1);
	}
}

/*---------------------------------------------------------------*/
/* If the mnt cannot be created, clear it in array and return NULL */

static PHK_Mnt *PHK_Mgr_new_mnt(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt tmp, *mp;

	if (!hash) hash = ZSTRING_HASH(mnt);

	if (!PHK_G(mtab)) {
		EALLOCATE(PHK_G(mtab),sizeof(HashTable));
		zend_hash_init(PHK_G(mtab), 16, NULL,
					   (dtor_func_t) PHK_Mgr_mnt_dtor, 0);
	}

	CLEAR_DATA(tmp);	/* Init everything to 0/NULL */

	zend_hash_quick_update(PHK_G(mtab), Z_STRVAL_P(mnt),
						   Z_STRLEN_P(mnt) + 1, hash, &tmp, sizeof(tmp),
						   (void **) &mp);

	mp->mnt = mnt;
	Z_ADDREF_P(mnt);

	mp->hash = hash;

	mp->order=PHK_G(mcount);
	EALLOCATE(PHK_G(mount_order),(PHK_G(mcount) + 1) * sizeof(mp));
	PHK_G(mount_order)[PHK_G(mcount)++]=mp;

	return mp;
}

/*---------------------------------------------------------------*/

static PHK_Mnt *PHK_Mgr_get_mnt(zval * mnt, ulong hash,
										  int exception TSRMLS_DC)
{
	PHK_Mnt *mp;
	int found;

	if (Z_TYPE_P(mnt) != IS_STRING) {
		EXCEPTION_ABORT_RET_1(NULL,
							  "PHK_Mgr_get_mnt: Mount point should be a string (type=%s)",
							  zend_zval_type_name(mnt));
	}

	if (!hash) hash = ZSTRING_HASH(mnt);

	found = (PHK_G(mtab) && (zend_hash_quick_find(PHK_G(mtab)
												  , Z_STRVAL_P(mnt),
												  Z_STRLEN_P(mnt) + 1,
												  hash,
												  (void **) &mp) == SUCCESS));

	if (!found) {
		if (exception) {
			THROW_EXCEPTION_1("%s: Invalid mount point", Z_STRVAL_P(mnt));
		}
		return NULL;
	}

	return mp;
}

/*---------------------------------------------------------------*/
/* {{{ proto bool PHK_Mgr::isMounted(string mnt) */

static PHP_METHOD(PHK_Mgr, isMounted)
{
	zval *mnt;
	int retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	retval = (PHK_Mgr_get_mnt(mnt, 0, 0 TSRMLS_CC) != NULL);
	RETVAL_BOOL(retval);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_validate(zval * mnt, ulong hash TSRMLS_DC)
{
	(void) PHK_Mgr_get_mnt(mnt, hash, 1 TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::validate(string mnt) */

static PHP_METHOD(PHK_Mgr, validate)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	(void) PHK_Mgr_validate(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Must accept an invalid mount point without error */

static void PHK_Mgr_umount(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt *mp;

	mp = PHK_Mgr_get_mnt(mnt, hash, 0 TSRMLS_CC);
	if (!mp) return;

	PHK_Mgr_umount_mnt(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void PHK_Mgr_umount_mnt(PHK_Mnt * mp TSRMLS_DC)
{
	int i;

	if (mp->nb_children) {
		for (i = 0; i < mp->nb_children; i++) {
			if (mp->children[i])
				PHK_Mgr_umount_mnt(mp->children[i] TSRMLS_CC);
		}
	}

/* Here, every children have unregistered themselves */
/* Now, unregister from parent (whose nb_children cannot be null) */

	if (mp->parent) {
		for (i = 0; i < mp->parent->nb_children; i++) {
			if (mp->parent->children[i] == mp) {
				mp->parent->children[i] = NULL;
				break;
			}
		}
	}

	PHK_umount(mp TSRMLS_CC);

	PHK_Mgr_remove_mnt(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::umount(string mnt) */

static PHP_METHOD(PHK_Mgr, umount)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_umount(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static zval *PHK_Mgr_instance_by_mp(PHK_Mnt * mp TSRMLS_DC)
{
	if (!mp->instance) {
		mp->instance=ut_new_instance(ZEND_STRL("PHK"), 0, 0, NULL TSRMLS_CC);
		PHK_set_mp_property(mp->instance, mp->order TSRMLS_CC);
	}

	return mp->instance;
}

/*---------------------------------------------------------------*/

static zval *PHK_Mgr_instance(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt *mp;

	mp = PHK_Mgr_get_mnt(mnt, hash, 1 TSRMLS_CC);
	if (EG(exception)) return NULL;

	return PHK_Mgr_instance_by_mp(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto PHK PHK_Mgr::instance(string mnt) */

static PHP_METHOD(PHK_Mgr, instance)
{
	zval *mnt, *instance;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

/*DBG_MSG1("Entering PHK_Mgr::instance(%s)",Z_STRVAL_P(mnt));*/

	instance = PHK_Mgr_instance(mnt, 0 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(instance);
}

/* }}} */
/*---------------------------------------------------------------*/

static zval *PHK_Mgr_proxy_by_mp(PHK_Mnt * mp TSRMLS_DC)
{
	zval *args[2];

	if (!(mp->proxy)) {
		args[0] = mp->path;
		args[1] = mp->flags;
		mp->proxy=ut_new_instance(ZEND_STRL("PHK\\Proxy"), 1, 2, args TSRMLS_CC);
	}

	return mp->proxy;
}

/*---------------------------------------------------------------*/

static zval *PHK_Mgr_proxy(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt *mp;

	mp = PHK_Mgr_get_mnt(mnt, hash, 1 TSRMLS_CC);
	if (EG(exception)) return NULL;

	return PHK_Mgr_proxy_by_mp(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto PHK_Proxy PHK_Mgr::proxy(string mnt) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK_Mgr, proxy)
{
	zval *mnt, *proxy;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	proxy = PHK_Mgr_proxy(mnt, 0 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(proxy);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_mntList(zval * ret TSRMLS_DC)
{
	char *mnt_p;
	uint mnt_len;
	ulong dummy;
	HashPosition pos;

	array_init(ret);

	if (PHK_G(mtab)) {
		zend_hash_internal_pointer_reset_ex(PHK_G(mtab), &pos);
		while (zend_hash_get_current_key_ex(PHK_G(mtab), &mnt_p, &mnt_len
			, &dummy, 0, &pos) != HASH_KEY_NON_EXISTANT) {
			add_next_index_stringl(ret, mnt_p, mnt_len - 1, 1);
			zend_hash_move_forward_ex(PHK_G(mtab), &pos);
		}
	}
}

/*---------------------------------------------------------------*/
/* {{{ proto array PHK_Mgr::mntList() */

static PHP_METHOD(PHK_Mgr, mntList)
{
	PHK_Mgr_mntList(return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static inline int PHK_Mgr_isPhkUri(zval * path TSRMLS_DC)
{
	char *p;
	
	p=Z_STRVAL_P(path);
	return ((p[0] == 'p')
			&& (p[1] == 'h')
			&& (p[2] == 'k')
			&& (p[3] == ':')
			&& (p[4] == '/')
			&& (p[5] == '/'));
}

/*---------------------------------------------------------------*/
/* {{{ proto bool PHK_Mgr::isPhkUri(string uri) */

static PHP_METHOD(PHK_Mgr, isPhkUri)
{
	zval *path;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	RETVAL_BOOL(PHK_Mgr_isPhkUri(path TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/

#define PHK_ZSTR_LTRIM(_source,_ret_string,_ret_len) \
{ \
_ret_string=Z_STRVAL_P(_source); \
_ret_len=Z_STRLEN_P(_source); \
 \
while ((*_ret_string)=='/') \
	{ \
	_ret_string++; \
	_ret_len--; \
	} \
}

#define PHK_ZSTR_BASE_DECL()	char *_currentp;

#define PHK_ZSTR_APPEND_STRING(str,len) \
	{ \
	memmove(_currentp,str,len+1); \
	_currentp+=len; \
	}

#define PHK_ZSTR_INIT_URI() PHK_ZSTR_APPEND_STRING("phk://",6)

#define PHK_ZSTR_APPEND_ZVAL(zp) \
	PHK_ZSTR_APPEND_STRING(Z_STRVAL_P(zp),Z_STRLEN_P(zp))

#define PHK_ZSTR_ALLOC_ZSTRING(len) \
	{ \
	ZVAL_STRINGL(ret,(_currentp=ut_eallocate(NULL,len+1)),len,0); \
	}

/*---------------------------------------------------------------*/

static void PHK_Mgr_uri(zval * mnt, zval * path, zval * ret TSRMLS_DC)
{
	PHK_ZSTR_BASE_DECL()
	char *tpath;
	int tlen;

	PHK_ZSTR_LTRIM(path, tpath, tlen);

	PHK_ZSTR_ALLOC_ZSTRING(Z_STRLEN_P(mnt) + tlen + 7);

	PHK_ZSTR_INIT_URI();
	PHK_ZSTR_APPEND_ZVAL(mnt);
	PHK_ZSTR_APPEND_STRING("/", 1);
	PHK_ZSTR_APPEND_STRING(tpath, tlen);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::uri(string mnt, string path) */

static PHP_METHOD(PHK_Mgr, uri)
{
	zval *mnt, *path;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &mnt, &path)
		== FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_uri(mnt, path, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_commandURI(zval * mnt, zval * command,
								zval * ret TSRMLS_DC)
{
	PHK_ZSTR_BASE_DECL()

	PHK_ZSTR_ALLOC_ZSTRING(Z_STRLEN_P(mnt) + Z_STRLEN_P(command) + 8);

	PHK_ZSTR_INIT_URI();
	PHK_ZSTR_APPEND_ZVAL(mnt);
	PHK_ZSTR_APPEND_STRING("/?", 2);
	PHK_ZSTR_APPEND_ZVAL(command);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::commandURI(string mnt, string command) */

static PHP_METHOD(PHK_Mgr, commandURI)
{
	zval *mnt, *command;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &mnt, &command) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_commandURI(mnt, command, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_sectionURI(zval * mnt, zval * section,
								zval * ret TSRMLS_DC)
{
	PHK_ZSTR_BASE_DECL()

	PHK_ZSTR_ALLOC_ZSTRING(Z_STRLEN_P(mnt) + Z_STRLEN_P(section) + 21);

	PHK_ZSTR_INIT_URI();
	PHK_ZSTR_APPEND_ZVAL(mnt);

	PHK_ZSTR_APPEND_STRING("/?section&name=", 15);
	PHK_ZSTR_APPEND_ZVAL(section);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::sectionURI(string mnt, string section) */

static PHP_METHOD(PHK_Mgr, sectionURI)
{
	zval *mnt, *section;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &mnt, &section) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_sectionURI(mnt, section, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_normalizeURI(zval * uri, zval * ret TSRMLS_DC)
{
	char *p;

	(*ret) = (*uri);
	zval_copy_ctor(ret);

	p = Z_STRVAL_P(ret);
	while (*p) {
		if ((*p) == '\\')
			(*p) = '/';
		p++;
	}
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::normalizeURI(string uri) */

static PHP_METHOD(PHK_Mgr, normalizeURI)
{
	zval *uri;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &uri) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_normalizeURI(uri, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void compute_automapURI(zval * mnt, zval * ret TSRMLS_DC)
{
	PHK_ZSTR_BASE_DECL()

	PHK_ZSTR_ALLOC_ZSTRING(Z_STRLEN_P(mnt) + 28);

	PHK_ZSTR_INIT_URI();
	PHK_ZSTR_APPEND_ZVAL(mnt);

	PHK_ZSTR_APPEND_STRING("/?section&name=AUTOMAP", 22);
}

/*---------------------------------------------------------------*/
/* {{{ proto string|null PHK_Mgr::automapURI(string mnt) */

static PHP_METHOD(PHK_Mgr, automapURI)
{
	zval *mnt;
	PHK_Mnt *mp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	mp = PHK_Mgr_get_mnt(mnt, 0, 1 TSRMLS_CC);
	if (EG(exception) || (!mp->automapURI)) return;

	RETVAL_BY_REF(mp->automapURI);
}

/* }}} */
/*---------------------------------------------------------------*/

static void compute_baseURI(zval * mnt, zval * ret TSRMLS_DC)
{
	PHK_ZSTR_BASE_DECL()

	PHK_ZSTR_ALLOC_ZSTRING(Z_STRLEN_P(mnt) + 7);

	PHK_ZSTR_INIT_URI();
	PHK_ZSTR_APPEND_ZVAL(mnt);
	PHK_ZSTR_APPEND_STRING("/", 1);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::baseURI(string mnt) */

static PHP_METHOD(PHK_Mgr, baseURI)
{
	zval *mnt;
	PHK_Mnt *mp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	mp = PHK_Mgr_get_mnt(mnt, 0, 1 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(mp->baseURI);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_uriToMnt(zval * uri, zval * ret TSRMLS_DC)
{
	char *bp, *p;
	int len;

	if (!PHK_Mgr_isPhkUri(uri TSRMLS_CC)) {
		EXCEPTION_ABORT_1("%s: Not a PHK URI", Z_STRVAL_P(uri));
	}

	bp = p = Z_STRVAL_P(uri) + 6;
	while ((*p) && ((*p) != '/') && ((*p) != '\\') && ((*p) != ' '))
		p++;
	len = p - bp;

	zval_dtor(ret);
	ZVAL_STRINGL(ret, bp, len, 1);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::uriToMnt(string uri) */

static PHP_METHOD(PHK_Mgr, uriToMnt)
{
	zval *uri;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &uri) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_uriToMnt(uri, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_topLevelPath(zval * zpath, zval * ret TSRMLS_DC)
{
	zval zmnt;
	PHK_Mnt *mp;

	INIT_ZVAL(zmnt);

	while (PHK_Mgr_isPhkUri(zpath TSRMLS_CC)) {
		PHK_Mgr_uriToMnt(zpath,&zmnt TSRMLS_CC);
		mp=PHK_Mgr_get_mnt(&zmnt, 0, 1 TSRMLS_CC);
		zval_dtor(&zmnt);
		zpath=mp->path;
	}

	zval_dtor(ret);
	/* Don't know why but using zval_copy_ctor() to set zpath into ret
	   creates a memory leak ! So, using another way... */
	ZVAL_STRINGL(ret,Z_STRVAL_P(zpath),Z_STRLEN_P(zpath),1);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::topLevelPath(string uri) */

static PHP_METHOD(PHK_Mgr, topLevelPath)
{
	zval *zpath;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zpath) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_topLevelPath(zpath, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Empty as long as PHK does not require more than PHP 5.1 as, if version
* is <5.1, we don't even get here */

static void PHK_Mgr_checkPhpVersion(TSRMLS_D)
{
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::checkPhpVersion() */

static PHP_METHOD(PHK_Mgr, checkPhpVersion)
{
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_setCache(zval * zp TSRMLS_DC)
{
	if ((Z_TYPE_P(zp) != IS_NULL) && (Z_TYPE_P(zp) != IS_BOOL)) {
		EXCEPTION_ABORT("setCache value can be only bool or null");
	}

	PHK_G(caching) = (*zp);	/* No need to copy_ctor() as it can only be null/bool */
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::setCache(mixed toggle) */

static PHP_METHOD(PHK_Mgr, setCache)
{
	zval *caching;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &caching) ==
		FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_setCache(caching TSRMLS_CC);
}


/* }}} */
/*---------------------------------------------------------------*/

static int PHK_Mgr_cacheEnabled(zval * mnt, ulong hash, zval * command,
								 zval * params, zval * path TSRMLS_DC)
{
	PHK_Mnt *mp;

	if (Z_TYPE(PHK_G(caching)) != IS_NULL) return Z_BVAL(PHK_G(caching));

	if (ZVAL_IS_NULL(mnt)) return 0; /* Default for global commands: no cache */

	mp = PHK_Mgr_get_mnt(mnt, hash, 1 TSRMLS_CC);
	if (EG(exception)) return 0;

	return PHK_cacheEnabled(mp, command, params, path TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* In case of error, free mem allocated by computeMnt() */

static void PHK_Mgr_pathToMnt(zval * path, zval * mnt TSRMLS_DC)
{
	zval *tmp_mnt;

	tmp_mnt=NULL;
	PHK_Mgr_computeMnt(path, NULL, &tmp_mnt, NULL TSRMLS_CC);
	if (EG(exception)) {
		ut_ezval_ptr_dtor(&tmp_mnt);
		return;
	}

	PHK_Mgr_get_mnt(tmp_mnt, 0, 1 TSRMLS_CC);
	if (EG(exception)) {
		ut_ezval_ptr_dtor(&tmp_mnt);
		return;
	}

	ZVAL_COPY_VALUE(mnt,tmp_mnt);
	zval_copy_ctor(mnt);
	ut_ezval_ptr_dtor(&tmp_mnt);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::pathToMnt(string path) */

static PHP_METHOD(PHK_Mgr, pathToMnt)
{
	zval *path;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	PHK_Mgr_pathToMnt(path, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* On entry, parentMnt, mnt, and mtime can be null */

#define INIT_PHK_MGR_COMPUTE_MNT() \
	{ \
	INIT_ZVAL(subpath); \
	INIT_ZVAL(tmp_parentMnt); \
	INIT_ZVAL(tmp_mnt); \
	}

#define CLEANUP_PHK_MGR_COMPUTE_MNT() \
	{ \
	zval_dtor(&subpath); \
	zval_dtor(&tmp_parentMnt); \
	zval_dtor(&tmp_mnt); \
	}

#define ABORT_PHK_MGR_COMPUTE_MNT() \
	{ \
	CLEANUP_PHK_MGR_COMPUTE_MNT(); \
	return; \
	}

static void PHK_Mgr_computeMnt(zval * path, PHK_Mnt ** parent_mpp,
								zval ** mnt, zval ** mtime TSRMLS_DC)
{
	zval subpath, tmp_mnt, tmp_parentMnt;
	int len;
	char *p;
	PHK_Mnt *parent_mp;
	time_t mti;

	INIT_PHK_MGR_COMPUTE_MNT();

	if (PHK_Mgr_isPhkUri(path TSRMLS_CC)) {	/* Sub-package */
		PHK_Stream_parseURI(path, NULL, NULL, &tmp_parentMnt,
							   &subpath TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_COMPUTE_MNT();

		parent_mp = PHK_Mgr_get_mnt(&tmp_parentMnt, 0, 1 TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_COMPUTE_MNT();

		if (parent_mpp) (*parent_mpp) = parent_mp;

		if (mnt) {
			p = Z_STRVAL(subpath);
			while (*p) {
				if ((*p) == '/')
					(*p) = '*';
				p++;
			}
			len = Z_STRLEN(tmp_parentMnt) + Z_STRLEN(subpath) + 1;
			spprintf(&p, len, "%s#%s", Z_STRVAL(tmp_parentMnt),
					 Z_STRVAL(subpath));
			if (*mnt) zval_dtor(*mnt);
			else MAKE_STD_ZVAL(*mnt);
			ZVAL_STRINGL((*mnt), p, len, 0);
		}

		if (mtime) {
			(*mtime) = parent_mp->mtime;	/*Add ref to parent's mtime */
			Z_ADDREF_P(parent_mp->mtime);
		}
	} else {
		ut_pathUniqueID('p',path,mnt,&mti TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_COMPUTE_MNT();

		if (parent_mpp) (*parent_mpp) = NULL;

		if (mtime) {
			if (*mtime) zval_dtor(*mtime);
			else MAKE_STD_ZVAL((*mtime));
			ZVAL_LONG((*mtime), mti);
		}
	}
	CLEANUP_PHK_MGR_COMPUTE_MNT();
}

/*---------------------------------------------------------------*/
/* Mount a package and return its map ID */
/* Shortcut used when loading a package during Automap symbol resolution */

static long PHK_Mgr_mount_from_Automap(zval * path, long flags TSRMLS_DC)
{
	PHK_Mnt *mp;
	
	mp=PHK_Mgr_mount(path, flags TSRMLS_CC);
	if (EG(exception)) return 0;
	return mp->automapID;
}

/*---------------------------------------------------------------*/

#define INIT_PHK_MGR_MOUNT() \
	{ \
	mp=NULL; \
	mnt=mtime=NULL; \
	}

#define CLEANUP_PHK_MGR_MOUNT() \
	{ \
	ut_ezval_ptr_dtor(&mnt); \
	ut_ezval_ptr_dtor(&mtime); \
	}

#define RETURN_FROM_PHK_MGR_MOUNT(_ret) \
	{ \
	CLEANUP_PHK_MGR_MOUNT(); \
	DBG_MSG("Exiting PHK_Mgr_mount"); \
	return _ret; \
	}

#define ABORT_PHK_MGR_MOUNT() \
	{ \
	if (mp) { \
		if (mp->parent && mp->parent->nb_children) { \
			int i; \
			for (i=0;i<mp->parent->nb_children;i++) { \
				if (mp->parent->children[i]==mp) { \
					mp->parent->children[i]=NULL; \
					break; \
				} \
			} \
		} \
		PHK_Mgr_remove_mnt(mp TSRMLS_CC); \
	} \
	RETURN_FROM_PHK_MGR_MOUNT(NULL); \
	}

static PHK_Mnt *PHK_Mgr_mount(zval * path, long flags TSRMLS_DC)
{
	zval *args[5], *mnt, *mtime;
	PHK_Mnt *mp, *parent_mp;
	char *p;
	ulong hash;
	int len;

	DBG_MSG2("Entering PHK_Mgr_mount(%s,%ld)", Z_STRVAL_P(path), flags);

	INIT_PHK_MGR_MOUNT();

	if (!ZVAL_IS_STRING(path)) convert_to_string(path);

	if (flags & PHK_FLAG_IS_CREATOR) {
		spprintf(&p, 32, "_tmp_mnt_%d", tmp_mnt_num++);
		MAKE_STD_ZVAL(mnt);
		ZVAL_STRING(mnt, p, 0);

		mp = PHK_Mgr_new_mnt(mnt, 0 TSRMLS_CC);
		if (!mp) ABORT_PHK_MGR_MOUNT();

		SEPARATE_ARG_IF_REF(path);
		mp->path = path;

		MAKE_STD_ZVAL(mp->flags);
		ZVAL_LONG(mp->flags, flags);

		ALLOC_INIT_ZVAL(mp->caching);

		MAKE_STD_ZVAL((mp->mtime));
		ZVAL_LONG((mp->mtime), time(NULL));

		MAKE_STD_ZVAL(mp->minVersion);
		ZVAL_STRINGL((mp->minVersion), "", 0, 1);

		MAKE_STD_ZVAL(mp->options);
		array_init(mp->options);

		MAKE_STD_ZVAL(mp->buildInfo);
		array_init(mp->buildInfo);

		MAKE_STD_ZVAL(mp->baseURI);
		compute_baseURI(mnt, mp->baseURI TSRMLS_CC);

		args[0] = mp->mnt;
		args[1] = mp->path;
		args[2] = mp->flags;
		mp->instance=ut_new_instance(ZEND_STRL("PHK\\Build\\Creator"), 1, 3, args TSRMLS_CC);
	} else {
		PHK_Mgr_computeMnt(path, &parent_mp, &mnt, &mtime TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_MOUNT();

		hash = ZSTRING_HASH(mnt);
		mp = PHK_Mgr_get_mnt(mnt, hash, 0 TSRMLS_CC);
		if (mp) RETURN_FROM_PHK_MGR_MOUNT(mp);	/* Already mounted */

		mp = PHK_Mgr_new_mnt(mnt, hash TSRMLS_CC);
		if (!mp) ABORT_PHK_MGR_MOUNT();

		mp->mtime = mtime; /* Transfer mtime to mp */
		mtime=NULL; /* Don't delete zval in CLEANUP */

		p=ut_mkAbsolutePath(Z_STRVAL_P(path),Z_STRLEN_P(path),&len,0 TSRMLS_CC);
		MAKE_STD_ZVAL(mp->path);
		ZVAL_STRINGL(mp->path, p, len, 0);

		if (parent_mp) {
			mp->parent = parent_mp;
			EALLOCATE(parent_mp->children
				,(parent_mp->nb_children + 1) * sizeof(PHK_Mnt *));
			parent_mp->children[parent_mp->nb_children++] = mp;
		}

		MAKE_STD_ZVAL(mp->flags);
		ZVAL_LONG(mp->flags, flags);

		ALLOC_INIT_ZVAL(mp->caching);

		PHK_Mgr_populate_pdata(mnt, hash, mp TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_MOUNT();

		PHK_init(mp TSRMLS_CC);

		DBG_MSG2("Mounted %s on %s",Z_STRVAL_P(path),Z_STRVAL_P(mnt));

		/* Instance object is created when needed by PHK_Mgr_instance() */
	}

	if (EG(exception)) ABORT_PHK_MGR_MOUNT();
	RETURN_FROM_PHK_MGR_MOUNT(mp);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::mount(string path [, int flags ]) */

static PHP_METHOD(PHK_Mgr, mount)
{
	zval *path;
	long flags;
	PHK_Mnt *mp;

	flags = 0;
	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "z|l", &path, &flags) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	mp = PHK_Mgr_mount(path, flags TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(mp->mnt);
}

/* }}} */
/*---------------------------------------------------------------*/

static PHK_Pdata *PHK_Mgr_get_pdata(
	zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Pdata *pmp;

	if (!ZVAL_IS_STRING(mnt)) {
		EXCEPTION_ABORT_RET_1(NULL,"PHK_get_pdata: Mount point should be a string (type=%s)",
							  zend_zval_type_name(mnt));
	}

	if (!hash) hash = ZSTRING_HASH(mnt);

	if (zend_hash_quick_find(&persistent_mtab, Z_STRVAL_P(mnt),
			Z_STRLEN_P(mnt) + 1, hash, (void **) &pmp) != SUCCESS) return NULL;

	return pmp;
}

/*---------------------------------------------------------------*/

#define INIT_PHK_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	MutexLock(persistent_mtab); \
	MAKE_STD_ZVAL(minVersion); \
	MAKE_STD_ZVAL(options); \
	MAKE_STD_ZVAL(buildInfo); \
	}

#define CLEANUP_PHK_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	ut_ezval_ptr_dtor(&minVersion); \
	ut_ezval_ptr_dtor(&options); \
	ut_ezval_ptr_dtor(&buildInfo); \
	}
	
#define RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(_ret) \
	{ \
	CLEANUP_PHK_GET_OR_CREATE_PERSISTENT_DATA(); \
	MutexUnlock(persistent_mtab); \
	return _ret; \
	}

#define ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	CLEANUP_PHK_GET_OR_CREATE_PERSISTENT_DATA(); \
	RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(NULL); \
	}

static PHK_Pdata *PHK_Mgr_get_or_create_pdata(zval * mnt,
	ulong hash TSRMLS_DC)
{
	PHK_Pdata tmp_entry, *entry;
	zval *minVersion, *options, *buildInfo, *args[2], **zpp, *ztmp,*ztmp2;
	HashTable *opt_ht;
	char *p;

	DBG_MSG1("Entering PHK_Mgr_get_or_create_pdata(%s)", Z_STRVAL_P(mnt));

	INIT_PHK_GET_OR_CREATE_PERSISTENT_DATA();

	if (!hash) hash = ZSTRING_HASH(mnt);

	entry=PHK_Mgr_get_pdata(mnt, hash TSRMLS_CC);
	if (entry) {
		RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(entry);
	}

	/* Create entry - slow path */

	PHK_needPhpRuntime(TSRMLS_C);

	/* We won't cache this information as they are read only once per
	* thread/process, when the package is opened for the 1st time. */

	args[0] = mnt;
	MAKE_STD_ZVAL(ztmp);
	ZVAL_BOOL(ztmp,0);
	args[1] = ztmp;
	ut_call_user_function_string(NULL
		, ZEND_STRL("PHK\\Tools\\Util::getMinVersion"), minVersion,2, args TSRMLS_CC);
	ut_ezval_ptr_dtor(&ztmp);
	if (EG(exception)) ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();

	/* Check minVersion */

	if (php_version_compare(Z_STRVAL_P(minVersion), PHK_ACCEL_VERSION) > 0) {
		THROW_EXCEPTION_1("Cannot understand this format. Requires version %s",
							  Z_STRVAL_P(minVersion));
		ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	}
	
	ALLOC_INIT_ZVAL(ztmp);
	args[1] = ztmp;
	ut_call_user_function_array(NULL
			, ZEND_STRL("PHK\\Tools\\Util::getOptions"), options, 2,	args TSRMLS_CC);
	ut_ezval_ptr_dtor(&ztmp);
	if (EG(exception)) ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();

	/* Check that the required extensions are present or can be loaded */
	/* The PHP code must check this in PHK::init() for each mount() because
	   it is not sure to remain in the same thread. As dynamic extension
	   loading is not supported in multithread mode, we can check it here. */

	if ((FIND_HKEY(Z_ARRVAL_P(options),required_extensions,&zpp)==SUCCESS)
		&& (ZVAL_IS_ARRAY(*zpp))) {
		ut_loadExtensions(*zpp TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	}

	/* Check that current PHP version is between min and max values, if set */

	if ((FIND_HKEY(Z_ARRVAL_P(options),min_php_version,&zpp)==SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		if (php_version_compare(PHP_VERSION,Z_STRVAL_PP(zpp)) < 0) {
			THROW_EXCEPTION_1("PHP minimum supported version: %s",Z_STRVAL_PP(zpp));
		}
		if (EG(exception)) ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	}
	
	if ((FIND_HKEY(Z_ARRVAL_P(options),max_php_version,&zpp)==SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		if (php_version_compare(PHP_VERSION,Z_STRVAL_PP(zpp)) > 0) {
			THROW_EXCEPTION_1("PHP maximum supported version: %s",Z_STRVAL_PP(zpp));
		}
		if (EG(exception)) ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	}

	/* Now, we can create the entry (it cannot fail any more) */

	CLEAR_DATA(tmp_entry);
	zend_hash_quick_update(&persistent_mtab, Z_STRVAL_P(mnt)
						   , Z_STRLEN_P(mnt) + 1, hash, &tmp_entry,
						   sizeof(tmp_entry), (void **) &entry);

	entry->ctime = time(NULL);

	entry->minVersion=ut_persist_zval(minVersion);

	entry->options=ut_persist_zval(options);
	opt_ht = Z_ARRVAL_P(entry->options);

	ut_call_user_function_array(NULL
		, ZEND_STRL("PHK\\Tools\\Util::getBuildInfo"), buildInfo, 2, args TSRMLS_CC);
	entry->buildInfo=ut_persist_zval(buildInfo);

	/* Set shortcuts */

#define PHK_GPD_BOOL_SHORTCUT(name) \
	entry->name=((FIND_HKEY(opt_ht,name,&zpp)==SUCCESS) \
		&& (ZVAL_IS_BOOL(*zpp))) ? Z_BVAL_PP(zpp) : 0

	PHK_GPD_BOOL_SHORTCUT(no_cache);
	PHK_GPD_BOOL_SHORTCUT(no_opcode_cache);
	PHK_GPD_BOOL_SHORTCUT(web_main_redirect);
	PHK_GPD_BOOL_SHORTCUT(auto_umount);

	/* If it is present, PHK_Creator ensures that option['mime_types']
		is an array. But it must be checked for security */

#define PHK_GPD_ARRAY_SHORTCUT(name) \
	if ((FIND_HKEY(opt_ht,name,&zpp)==SUCCESS)&&(ZVAL_IS_ARRAY(*zpp))) { \
		entry->name=(*zpp); \
		Z_ADDREF_PP(zpp); \
	} else { \
		entry->name=NULL; \
	}

	PHK_GPD_ARRAY_SHORTCUT(mime_types);

#define PHK_GPD_STRING_SHORTCUT(name) \
	if ((FIND_HKEY(opt_ht,name,&zpp)==SUCCESS)&&(ZVAL_IS_STRING(*zpp))) { \
		entry->name=(*zpp); \
		Z_ADDREF_PP(zpp); \
	} else { \
		entry->name=NULL; \
	}

	PHK_GPD_STRING_SHORTCUT(web_run_script);
	PHK_GPD_STRING_SHORTCUT(plugin_class);
	PHK_GPD_STRING_SHORTCUT(web_access);
	PHK_GPD_STRING_SHORTCUT(min_php_version);
	PHK_GPD_STRING_SHORTCUT(max_php_version);

	/* Pre-computed values */

	ALLOC_INIT_ZVAL(ztmp);
	compute_baseURI(mnt, ztmp TSRMLS_CC);
	entry->baseURI=ut_persist_zval(ztmp);
	ut_ezval_ptr_dtor(&ztmp);

	if ((FIND_HKEY(Z_ARRVAL_P(entry->buildInfo), map_defined, &zpp) ==
		 SUCCESS) && ZVAL_IS_BOOL(*zpp) && Z_BVAL_PP(zpp)) {
		ALLOC_INIT_ZVAL(ztmp);
		compute_automapURI(mnt, ztmp TSRMLS_CC);
		entry->automapURI=ut_persist_zval(ztmp);
		ut_ezval_ptr_dtor(&ztmp);
	}

	if ((FIND_HKEY(opt_ht, mount_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		ALLOC_INIT_ZVAL(ztmp);
		PHK_Mgr_uri(mnt, *zpp, ztmp TSRMLS_CC);
		entry->mount_script_uri=ut_persist_zval(ztmp);
		ut_ezval_ptr_dtor(&ztmp);
	}

	if ((FIND_HKEY(opt_ht, umount_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		ALLOC_INIT_ZVAL(ztmp);
		PHK_Mgr_uri(mnt, *zpp, ztmp TSRMLS_CC);
		entry->umount_script_uri=ut_persist_zval(ztmp);
		ut_ezval_ptr_dtor(&ztmp);
	}

	if ((FIND_HKEY(opt_ht, lib_run_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		ALLOC_INIT_ZVAL(ztmp);
		PHK_Mgr_uri(mnt, *zpp, ztmp TSRMLS_CC);
		entry->lib_run_script_uri=ut_persist_zval(ztmp);
		ut_ezval_ptr_dtor(&ztmp);
	}

	if ((FIND_HKEY(opt_ht, cli_run_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		ALLOC_INIT_ZVAL(ztmp);
		ALLOC_INIT_ZVAL(ztmp2);
		PHK_Mgr_uri(mnt, *zpp, ztmp TSRMLS_CC);
		spprintf(&p, UT_PATH_MAX, "require('%s');", Z_STRVAL_P(ztmp));
		ZVAL_STRING(ztmp2, p, 0);
		entry->cli_run_command=ut_persist_zval(ztmp2);
		ut_ezval_ptr_dtor(&ztmp);
		ut_ezval_ptr_dtor(&ztmp2);
	}

	/* Cleanup and return */

	RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(entry);
}

/*---------------------------------------------------------------*/

#define PHK_MGR_PPD_COPY_PZVAL(_field) \
	{ \
		if (entry->_field) { \
			ALLOC_ZVAL(mp->_field); \
			INIT_PZVAL_COPY(mp->_field,entry->_field); \
			zval_copy_ctor(mp->_field); \
		} else mp->_field=NULL; \
	}

static void PHK_Mgr_populate_pdata(zval * mnt, ulong hash,
											 PHK_Mnt * mp TSRMLS_DC)
{
	PHK_Pdata *entry;

	DBG_MSG1("Entering PHK_Mgr_populate_pdata(%s)",
			 Z_STRVAL_P(mnt));

	entry = PHK_Mgr_get_or_create_pdata(mnt, hash TSRMLS_CC);
	if (EG(exception)) return;
	mp->pdata=entry;

	/* Populate mp structure from persistent data */

	PHK_MGR_PPD_COPY_PZVAL(minVersion);
	PHK_MGR_PPD_COPY_PZVAL(options);
	PHK_MGR_PPD_COPY_PZVAL(buildInfo);

	mp->no_cache = entry->no_cache;
	mp->no_opcode_cache = entry->no_opcode_cache;
	mp->web_main_redirect = entry->web_main_redirect;
	mp->auto_umount = entry->auto_umount;
	PHK_MGR_PPD_COPY_PZVAL(mime_types);
	PHK_MGR_PPD_COPY_PZVAL(web_run_script);
	PHK_MGR_PPD_COPY_PZVAL(plugin_class);
	PHK_MGR_PPD_COPY_PZVAL(web_access);
	PHK_MGR_PPD_COPY_PZVAL(min_php_version);
	PHK_MGR_PPD_COPY_PZVAL(max_php_version);

	PHK_MGR_PPD_COPY_PZVAL(baseURI);
	PHK_MGR_PPD_COPY_PZVAL(automapURI);
	PHK_MGR_PPD_COPY_PZVAL(mount_script_uri);
	PHK_MGR_PPD_COPY_PZVAL(umount_script_uri);
	PHK_MGR_PPD_COPY_PZVAL(lib_run_script_uri);
	PHK_MGR_PPD_COPY_PZVAL(cli_run_command);
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::mimeHeader(string mnt, int hash, string path) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK_Mgr, mimeHeader)
{
	zval *mnt, *path;
	PHK_Mnt *mp;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &mnt, &path) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	mp = PHK_Mgr_get_mnt(mnt, 0, 1 TSRMLS_CC);
	if (EG(exception)) return;

	PHK_mimeHeader(mp, path TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* zval_dtor works for persistent arrays, but not for persistent strings */

static void PHK_Mgr_Persistent_Data_dtor(PHK_Pdata * entry)
{
	ut_pzval_ptr_dtor(&(entry->minVersion));
	/* TODO: Why do these two lines cause free() errors ? */
	ut_pzval_ptr_dtor(&(entry->options));
	ut_pzval_ptr_dtor(&(entry->buildInfo));

	ut_pzval_ptr_dtor(&(entry->mime_types));
	ut_pzval_ptr_dtor(&(entry->web_run_script));
	ut_pzval_ptr_dtor(&(entry->plugin_class));
	ut_pzval_ptr_dtor(&(entry->web_access));
	ut_pzval_ptr_dtor(&(entry->min_php_version));
	ut_pzval_ptr_dtor(&(entry->max_php_version));

	ut_pzval_ptr_dtor(&(entry->baseURI));
	ut_pzval_ptr_dtor(&(entry->automapURI));
	ut_pzval_ptr_dtor(&(entry->mount_script_uri));
	ut_pzval_ptr_dtor(&(entry->umount_script_uri));
	ut_pzval_ptr_dtor(&(entry->lib_run_script_uri));
	ut_pzval_ptr_dtor(&(entry->cli_run_command));
}

/*---------------------------------------------------------------*/

static zend_function_entry PHK_Mgr_functions[] = {
	PHP_ME(PHK_Mgr, isMounted, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, validate, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, instance, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, proxy, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, mntList, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, setCache, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, pathToMnt, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, mount, UT_2args_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, umount, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, uri, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, isPhkUri, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, baseURI, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, commandURI, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, sectionURI, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, automapURI, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, normalizeURI, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, uriToMnt, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, topLevelPath, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, checkPhpVersion, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, mimeHeader, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_PHK_Mgr(TSRMLS_D)
{
	zend_class_entry ce;

	/*--- Init class */

	INIT_CLASS_ENTRY(ce, "PHK\\Mgr", PHK_Mgr_functions);
	zend_register_internal_class(&ce TSRMLS_CC);

	/*--- Init persistent data */

	PHK_Mgr_init_pdata(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Module shutdown                                               */

static int MSHUTDOWN_PHK_Mgr(TSRMLS_D)
{
	PHK_Mgr_shutdown_pdata(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RINIT_PHK_Mgr(TSRMLS_D)
{
	INIT_ZVAL(PHK_G(caching));

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static inline int RSHUTDOWN_PHK_Mgr(TSRMLS_D)
{
	if (PHK_G(mtab)) {
		zend_hash_destroy(PHK_G(mtab));
		EALLOCATE(PHK_G(mtab),0);
	}

	EALLOCATE(PHK_G(mount_order),0);
	PHK_G(mcount)=0;

	return SUCCESS;
}

/*---------------------------------------------------------------*/
