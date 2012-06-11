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

/* $Id$ */

/*---------------------------------------------------------------*/
/* Here, we check every pointers because the function can be called during
   the creation of the structure (mount failure) */
 
static void Automap_Mnt_dtor(Automap_Mnt *mp)
{
	TSRMLS_FETCH();

	if (mp->instance) {
		(void)zend_hash_del(Z_OBJPROP_P(mp->instance),MP_PROPERTY_NAME
			,sizeof(MP_PROPERTY_NAME)); /* Invalidate object */
	}

	ut_ezval_ptr_dtor(&(mp->instance));
	ut_ezval_ptr_dtor(&(mp->zpath));
	ut_ezval_ptr_dtor(&(mp->zbase));
}

/*---------------------------------------------------------------*/

static void Automap_Mnt_remove(Automap_Mnt *mp TSRMLS_DC)
{
	AUTOMAP_G(mount_order)[mp->order]=NULL;
	(void)zend_hash_del(&AUTOMAP_G(mnttab), Z_STRVAL_P(mp->map->zmnt)
		,Z_STRLEN_P(mp->map->zmnt)+1);
}

/*---------------------------------------------------------------*/

static Automap_Mnt *Automap_Mnt_create(Automap_Pmap *pmp, zval *zpathp, zval *zbasep
	, ulong flags TSRMLS_DC)
{
	Automap_Mnt tmp, *mp;
	char *p;
	int len;

	CLEAR_DATA(tmp);	/* Init everything to 0/NULL */

	zend_hash_quick_add(&AUTOMAP_G(mnttab), Z_STRVAL_P(pmp->zmnt),
						   Z_STRLEN_P(pmp->zmnt) + 1, pmp->mnt_hash, &tmp, sizeof(tmp),
						   (void **) &mp);
	mp->map=pmp;
	mp->mnt_count=1;
	mp->instance=NULL;

	/* Make path absolute */
	p=ut_mk_absolute_path(Z_STRVAL_P(zpathp),Z_STRLEN_P(zpathp),&len,0 TSRMLS_CC);
	MAKE_STD_ZVAL(mp->zpath);
	ZVAL_STRINGL(mp->zpath, p, len, 0);

	if (zbasep) { /* Make base dir absolute */
		p=ut_mk_absolute_path(Z_STRVAL_P(zbasep),Z_STRLEN_P(zbasep),&len,1 TSRMLS_CC);
	} else { /* base_dir=dirname(path) */
		p=ut_absolute_dirname(Z_STRVAL_P(zpathp),Z_STRLEN_P(zpathp),&len,1 TSRMLS_CC);
	}
	MAKE_STD_ZVAL(mp->zbase);
	ZVAL_STRINGL(mp->zbase, p, len, 0);

	mp->flags=flags;

	mp->order=AUTOMAP_G(mcount);
	EALLOCATE(AUTOMAP_G(mount_order),(AUTOMAP_G(mcount) + 1) * sizeof(mp));
	AUTOMAP_G(mount_order)[AUTOMAP_G(mcount)++]=mp;

	return mp;
}

/*---------------------------------------------------------------*/

static Automap_Mnt *Automap_Mnt_get(zval *mnt, ulong hash, int exception TSRMLS_DC)
{
	Automap_Mnt *mp;
	int found;

	if (!ZVAL_IS_STRING(mnt)) {
		EXCEPTION_ABORT_RET_1(NULL,"Automap_Mnt_get: Mount point should be a string (type=%s)",
							  zend_zval_type_name(mnt));
	}

	if (!hash) hash = ZSTRING_HASH(mnt);

	found = (zend_hash_quick_find(&AUTOMAP_G(mnttab), Z_STRVAL_P(mnt),
		Z_STRLEN_P(mnt) + 1, hash, (void **) &mp) == SUCCESS);

	if (!found) {
		if (exception) {
			THROW_EXCEPTION_1("%s: Invalid mount point", Z_STRVAL_P(mnt));
		}
		return NULL;
	}

	return mp;
}

/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::is_mounted(string mnt) */

static PHP_METHOD(Automap, is_mounted)
{
	zval *mnt;
	int retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	retval = (Automap_Mnt_get(mnt, 0, 0 TSRMLS_CC) != NULL);

	RETVAL_BOOL(retval);
}

/* }}} */
/*---------------------------------------------------------------*/

ZEND_DLEXPORT void Automap_Mnt_validate(zval * mnt, ulong hash TSRMLS_DC)
{
	(void) Automap_Mnt_get(mnt, hash, 1 TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void Automap::validate(string mnt) */

static PHP_METHOD(Automap, validate)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	(void) Automap_Mnt_validate(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

#define CLEANUP_AUTOMAP_MNT_MOUNT()	{ \
	ut_ezval_ptr_dtor(&mnt); \
	}

#define ABORT_AUTOMAP_MNT_MOUNT() { \
	RETURN_AUTOMAP_MNT_MOUNT(NULL); \
	}

#define RETURN_AUTOMAP_MNT_MOUNT(_ret) { \
	CLEANUP_AUTOMAP_MNT_MOUNT(); \
	return (_ret); \
	}

ZEND_DLEXPORT Automap_Mnt *Automap_Mnt_mount(zval * path, zval * base_dir,
									   zval * mnt, ulong flags TSRMLS_DC)
{
	Automap_Mnt *mp;
	Automap_Pmap *pmp;
	ulong hash;

	DBG_MSG("Starting Automap::mount");

	mp=NULL;
	pmp=NULL;

	if (!ZVAL_IS_STRING(path)) convert_to_string(path);

	/* First, get mnt */

	if (mnt) {
		SEPARATE_ARG_IF_REF(mnt);
		if (!ZVAL_IS_STRING(mnt)) convert_to_string(mnt);
	} else {
		Automap_path_id(path,&mnt TSRMLS_CC);
		if (EG(exception)) ABORT_AUTOMAP_MNT_MOUNT();
	}

	hash = ZSTRING_HASH(mnt);
	if ((mp = Automap_Mnt_get(mnt, hash, 0 TSRMLS_CC)) != NULL) {
		/* Already mounted */
		DBG_MSG1("%s: Map is already mounted",Z_STRVAL_P(path));
		mp->mnt_count++;
		RETURN_AUTOMAP_MNT_MOUNT(mp);
	}

	if (!(pmp=Automap_Pmap_get_or_create(path,mnt,hash TSRMLS_CC))) return NULL;

	mp=Automap_Mnt_create(pmp,path,base_dir,flags TSRMLS_CC);

	RETURN_AUTOMAP_MNT_MOUNT(mp);
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::mount(string path [, string base_dir [, string mnt [, int flags]]]) */

static PHP_METHOD(Automap, mount)
{
	zval *path, *base_dir, *mnt;
	int flags;
	Automap_Mnt *mp;

	base_dir = mnt = NULL;
	flags = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|z!z!l", &path
		, &base_dir, &mnt, &flags) == FAILURE)
			EXCEPTION_ABORT("Cannot parse parameters");

	mp = Automap_Mnt_mount(path, base_dir, mnt, flags TSRMLS_CC);

	if (EG(exception)) return;
	RETVAL_BY_VAL(mp->map->zmnt);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Must accept an invalid mount point without error */

ZEND_DLEXPORT void Automap_umount(zval *mnt, ulong hash TSRMLS_DC)
{
	Automap_Mnt *mp;

	if (!(mp=Automap_Mnt_get(mnt,hash,0 TSRMLS_CC))) return;
	if (--(mp->mnt_count)) return;

	Automap_Mnt_remove(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void Automap::umount(string mnt) */

static PHP_METHOD(Automap, umount)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	Automap_umount(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto array Automap::mnt_list() */

static PHP_METHOD(Automap, mnt_list)
{
	int i;
	Automap_Mnt *mp;

	array_init(return_value);

	if (AUTOMAP_G(mcount)) {
		for (i=0;i<AUTOMAP_G(mcount);i++) {
			if ((mp=AUTOMAP_G(mount_order)[i])==NULL) continue;
			add_next_index_stringl(return_value,Z_STRVAL_P(mp->map->zmnt)
				,Z_STRLEN_P(mp->map->zmnt),1);
		}
	}
}

/* }}} */
/*---------------------------------------------------------------*/
/* Returns a newly allocated 'absolute path' corresponding to a mount point and
  a symbol entry (the absolute path for an extension is the extension name) */

static char *Automap_Mnt_abs_path(Automap_Mnt *mp, Automap_Pmap_Entry *pep
	, int *lenp TSRMLS_DC)
{
	char *ret;

	if (pep->ftype==AUTOMAP_F_PACKAGE) {
	ret=ut_eduplicate(Z_STRVAL(pep->zfpath),Z_STRLEN(pep->zfpath)+1);
	(*lenp)=Z_STRLEN(pep->zfpath);
	}
	else {
	spprintf(&ret,1024,"%s%s",Z_STRVAL_P(mp->zbase),Z_STRVAL(pep->zfpath));
	(*lenp)=Z_STRLEN_P(mp->zbase)+Z_STRLEN(pep->zfpath);
	}
	return ret;
}

/*---------------------------------------------------------------*/
/* Returns SUCCESS/FAILURE */

#define CLEANUP_AUTOMAP_MNT_RESOLVE_KEY() \
	{ \
	ut_ezval_ptr_dtor(&zp); \
	EALLOCATE(fpath,0); \
	EALLOCATE(req_str,0); \
	}

#define RETURN_AUTOMAP_MNT_RESOLVE_KEY(_ret) \
	{ \
	CLEANUP_AUTOMAP_MNT_RESOLVE_KEY(); \
	return _ret; \
	}

static int Automap_Mnt_resolve_key(Automap_Mnt *mp, zval *zkey, ulong hash TSRMLS_DC)
{
	zval *zp;
	char *fpath,ftype,*req_str;
	int old_error_reporting,len;
	Automap_Mnt *sub_mp;
	Automap_Pmap *pmp;
	Automap_Pmap_Entry *pep;

	zp=NULL;
	fpath=req_str=NULL;
	pmp=mp->map;
	if (!(pep=Automap_Pmap_find_key(pmp,zkey,hash TSRMLS_CC))) {
		RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
	}

	ftype=pep->ftype;

	if (ftype == AUTOMAP_F_EXTENSION) {
		ut_load_extension_file(&(pep->zfpath) TSRMLS_CC);
		if (EG(exception)) RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
		Automap_call_success_handlers(mp,pep TSRMLS_CC);
		RETURN_AUTOMAP_MNT_RESOLVE_KEY(SUCCESS);
	}

	/* Compute "require '<absolute path>';" */

	fpath=Automap_Mnt_abs_path(mp,pep,&len TSRMLS_CC);
	spprintf(&req_str,1024,"require '%s';",fpath);

	if (ftype == AUTOMAP_F_SCRIPT) {
		DBG_MSG1("eval : %s",req_str);
		zend_eval_string(req_str,NULL,req_str TSRMLS_CC);
		Automap_call_success_handlers(mp,pep TSRMLS_CC);
		RETURN_AUTOMAP_MNT_RESOLVE_KEY(SUCCESS);
	}

	if (ftype == AUTOMAP_F_PACKAGE) { /* Symbol is in a sub-package */

		/* Remove E_NOTICE messages if the test script is a package - workaround */
		/* to PHP bug #39903 ('__COMPILER_HALT_OFFSET__ already defined') */

		old_error_reporting = EG(error_reporting);
		EG(error_reporting) &= ~E_NOTICE;

		ALLOC_INIT_ZVAL(zp);
		zend_eval_string(req_str,zp,req_str TSRMLS_CC);

		EG(error_reporting) = old_error_reporting;

		if (!ZVAL_IS_STRING(zp)) {
			THROW_EXCEPTION_1("%s : Package inclusion should return a string",fpath);
			RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
		}

		sub_mp=Automap_Mnt_get(zp, 0, 1 TSRMLS_CC);
		ut_ezval_ptr_dtor(&zp);
		/* If no corresponding mounted map, forward the exception (shouldn't happen) */
		if (EG(exception)) RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
		RETURN_AUTOMAP_MNT_RESOLVE_KEY(Automap_Mnt_resolve_key(sub_mp, zkey, hash TSRMLS_CC));
	}

	/* Unknown value type (never happens in a valid map */
	THROW_EXCEPTION_1("<%c>: Unknown file type",ftype);
	RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE); 
}

/*===============================================================*/

static int MINIT_Automap_Mnt(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Mnt(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Mnt(TSRMLS_D)
{
	zend_hash_init(&AUTOMAP_G(mnttab), 16, NULL,(dtor_func_t)Automap_Mnt_dtor, 0);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Mnt(TSRMLS_D)
{
	zend_hash_destroy(&AUTOMAP_G(mnttab));

	EALLOCATE(AUTOMAP_G(mount_order),0);

	AUTOMAP_G(mcount)=0;

	return SUCCESS;
}
