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

/*---------------------------------------------------------------*/
/* Here, we check every pointers because the function can be called during
   the creation of the structure (load failure) */
 
static void Automap_Mnt_dtor(Automap_Mnt *mp)
{
	TSRMLS_FETCH();

	if (mp->instance) {
		(void)zend_hash_del(Z_OBJPROP_P(mp->instance),AUTOMAP_MP_PROPERTY_NAME
			,sizeof(AUTOMAP_MP_PROPERTY_NAME)); /* Invalidate object */
	}

	ut_ezval_ptr_dtor(&(mp->instance));
	ut_ezval_ptr_dtor(&(mp->zpath));
}

/*---------------------------------------------------------------*/

static void Automap_Mnt_remove(Automap_Mnt *mp TSRMLS_DC)
{
	PHK_G(map_array)[mp->id]=NULL;
	Automap_Mnt_dtor(mp);
	EALLOCATE(mp,0);
}

/*---------------------------------------------------------------*/

static Automap_Mnt *Automap_Mnt_get(long id, int exception TSRMLS_DC)
{
	if ((id <= 0) || (id >= PHK_G(map_count)) || (!PHK_G(map_array)[id])) {
		if (exception) {
			THROW_EXCEPTION_1("%ld: Invalid map ID", id);
		}
		return NULL;
	}

	return PHK_G(map_array)[id];
}

/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::is_active(integer id) */

static PHP_METHOD(Automap, is_active)
{
	long id;
	int retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "l", &id) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	retval = (Automap_Mnt_get(id, 0 TSRMLS_CC) != NULL);

	RETVAL_BOOL(retval);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_Mnt_array_add(Automap_Mnt *mp TSRMLS_DC)
{
	mp->id=PHK_G(map_count);
	EALLOCATE(PHK_G(map_array),(++PHK_G(map_count)) * sizeof(mp));
	PHK_G(map_array)[mp->id]=mp;
	DBG_MSG1("Added Automap_Mnt entry (index=%ld)",mp->id);
}

/*---------------------------------------------------------------*/
/* On entry:
* - zpathp is an absolute path
* - zufidp is the unique file identifier for zpathp
* - hash is the hashcode of zufidp
* - zbasep is the base path to use
*/

static Automap_Mnt *Automap_Mnt_load_extended(zval *zpathp, zval *zufidp
	, ulong hash, zval *zbasep TSRMLS_DC)
{
	Automap_Mnt *mp;
	Automap_Pmap *pmp;

	DBG_MSG("Starting Automap_Mnt_load_extended");

	mp=NULL;
	pmp=NULL;

	if (!(pmp=Automap_Pmap_get_or_create_extended(zpathp, NULL, 0, zufidp
		, hash, zbasep TSRMLS_CC))) return NULL;

	/* Allocate and fill Automap_Mnt slot */

	mp=ut_eallocate(NULL, sizeof(*mp));
	CLEAR_DATA(*mp);	/* Init everything to 0/NULL */

	mp->map=pmp;

	ALLOC_INIT_ZVAL(mp->zpath);
	ZVAL_STRINGL(mp->zpath,Z_STRVAL_P(zpathp),Z_STRLEN_P(zpathp),1);

	Automap_Mnt_array_add(mp);
	return mp;
}

/*---------------------------------------------------------------*/

static Automap_Mnt *Automap_Mnt_load(zval *zpathp, long flags TSRMLS_DC)
{
	Automap_Mnt *mp;
	Automap_Pmap *pmp;
	char *p;
	int len;
	zval *zapathp;

	DBG_MSG("Starting Automap_Mnt_load");

	mp=NULL;
	pmp=NULL;

	if (!ZVAL_IS_STRING(zpathp)) convert_to_string(zpathp);
	/* Make path absolute */
	p=ut_mk_absolute_path(Z_STRVAL_P(zpathp),Z_STRLEN_P(zpathp),&len,0 TSRMLS_CC);
	MAKE_STD_ZVAL(zapathp);
	ZVAL_STRINGL(zapathp, p, len, 0);
	
	if (!(pmp=Automap_Pmap_get_or_create(zapathp TSRMLS_CC))) {
		ut_ezval_ptr_dtor(&zapathp);
		return NULL;
	}

	/* Allocate and fill Automap_Mnt slot */

	mp=ut_eallocate(NULL, sizeof(*mp));
	CLEAR_DATA(*mp);	/* Init everything to 0/NULL */

	mp->map=pmp;
	mp->zpath=zapathp;
	mp->flags=flags;

	Automap_Mnt_array_add(mp);
	return mp;
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::load(string path[, int flags]) */

/* The $_bp arg of the PHP version of Automap::load is only used when loading a
* package through the PHK PHP runtime . Here, as both extensions are merged,
* we know that PHK are loaded through their extension, using another mechanism.
* So, we don't need the $_bp parameter.
*/

static PHP_METHOD(Automap, load)
{
	zval *path;
	long flags;
	Automap_Mnt *mp;

	flags = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|l", &path
		, &flags) == FAILURE)
			EXCEPTION_ABORT("Cannot parse parameters");

	mp = Automap_Mnt_load(path, flags TSRMLS_CC);

	if (EG(exception)) return;
	RETVAL_LONG(mp->id);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Must accept an invalid load point without error */

static void Automap_unload(long id TSRMLS_DC)
{
	Automap_Mnt *mp;

	if (!(mp=Automap_Mnt_get(id, 0 TSRMLS_CC))) return;

	Automap_Mnt_remove(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void Automap::unload(integer id) */

static PHP_METHOD(Automap, unload)
{
	long id;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "l", &id) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	Automap_unload(id TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto array Automap::active_ids() */

static PHP_METHOD(Automap, active_ids)
{
	long i;
	Automap_Mnt *mp;

	array_init(return_value);

	if (PHK_G(map_count)) {
		for (i=0;i<PHK_G(map_count);i++) {
			if ((mp=PHK_G(map_array)[i])==NULL) continue;
			add_next_index_long(return_value,i);
		}
	}
}

/* }}} */
/*---------------------------------------------------------------*/
/* Returns SUCCESS/FAILURE */

#define CLEANUP_AUTOMAP_MNT_RESOLVE_KEY() \
	{ \
	EALLOCATE(req_str,0); \
	}

#define RETURN_AUTOMAP_MNT_RESOLVE_KEY(_ret) \
	{ \
	CLEANUP_AUTOMAP_MNT_RESOLVE_KEY(); \
	return _ret; \
	}

static int Automap_Mnt_resolve_key(Automap_Mnt *mp, zval *zkey, ulong hash TSRMLS_DC)
{
	char ftype,*req_str;
	int old_error_reporting,id;
	Automap_Pmap *pmp;
	Automap_Pmap_Entry *pep;
	
	req_str=NULL;

	if (mp->flags & AUTOMAP_FLAG_NO_AUTOLOAD) return FAILURE;

	pmp=mp->map;
	if (!(pep=Automap_Pmap_find_key(pmp,zkey,hash TSRMLS_CC))) {
		return FAILURE;
	}

	ftype=pep->ftype;

	if (ftype == AUTOMAP_F_EXTENSION) {
		ut_load_extension_file(&(pep->zfpath) TSRMLS_CC);
		if (EG(exception)) RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
		Automap_call_success_handlers(mp,pep TSRMLS_CC);
		RETURN_AUTOMAP_MNT_RESOLVE_KEY(SUCCESS);
	}

	/* Compute "require '<absolute path>';" */

	spprintf(&req_str,1024,"require '%s';",Z_STRVAL(pep->zfapath));

	if (ftype == AUTOMAP_F_SCRIPT) {
		DBG_MSG1("eval : %s",req_str);
		zend_eval_string(req_str,NULL,req_str TSRMLS_CC);
		Automap_call_success_handlers(mp,pep TSRMLS_CC);
		RETURN_AUTOMAP_MNT_RESOLVE_KEY(SUCCESS);
	}

	if (ftype == AUTOMAP_F_PACKAGE) { /* Symbol is in a package */

		/* Remove E_NOTICE messages if the test script is a package - workaround */
		/* to PHP bug #39903 ('__COMPILER_HALT_OFFSET__ already defined') */

		old_error_reporting = EG(error_reporting);
		EG(error_reporting) &= ~E_NOTICE;

		id=PHK_G(map_count);
		zend_eval_string(req_str,NULL,req_str TSRMLS_CC);

		EG(error_reporting) = old_error_reporting;

		/* Check that package inclusion caused a map load */

		if (PHK_G(map_count) != (id + 1)) {
			THROW_EXCEPTION_1("%s : Package inclusion should load a map"
				,Z_STRVAL(pep->zfapath));
			RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
		}

		RETURN_AUTOMAP_MNT_RESOLVE_KEY(
			Automap_Mnt_resolve_key(PHK_G(map_array)[id], zkey, hash TSRMLS_CC));
	}

	/* Unknown target type (never happens in a valid map */
	THROW_EXCEPTION_1("<%c>: Unknown target type",ftype);
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
	/* Initialize map_array with an empty slot. Allows to start numbering IDs
	* from 1 (ID 0 is considered invalid) */

	EALLOCATE(PHK_G(map_array),sizeof(Automap_Mnt *));
	PHK_G(map_array)[0]=NULL;
	PHK_G(map_count)=1;

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Mnt(TSRMLS_D)
{
	int i;
	Automap_Mnt *mp;

	if (PHK_G(map_count) > 1) {
		for (i=1;i<PHK_G(map_count);i++) { /* Slot 0 is always NULL */
			mp=PHK_G(map_array)[i];
			if (mp) Automap_Mnt_remove(mp);
		}
	}

	EALLOCATE(PHK_G(map_array),0);
	PHK_G(map_count)=0;

	return SUCCESS;
}
