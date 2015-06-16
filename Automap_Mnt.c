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
	ut_ezval_ptr_dtor(&(mp->map_object));
	zend_string_release(mp->path);
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
/* {{{ proto boolean \Automap\Mgr::isActiveID(integer id) */

static PHP_METHOD(Automap, isActiveID)
{
	zval *zid;
	int retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zid) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");
	convert_to_long(zid);

	retval = (Automap_Mnt_get(Z_LVAL_P(zid), 0 TSRMLS_CC) != NULL);

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
* - path is an absolute path
* - ufid is the unique file identifier for zpathp
* - base_path is the base path to use when creating pmp if null
* - If non null, pmp points to the Pmap to use (used by PHK to speed up loading)
*/

static Automap_Mnt *Automap_Mnt_load_extended(zend_string *path, zend_string *ufid
	, zend_string *base_path, Automap_Pmap *pmp, long flags TSRMLS_DC)
{
	Automap_Mnt *mp;

	DBG_MSG("Starting Automap_Mnt_load_extended");

	mp=NULL;

	if (!pmp) pmp=Automap_Pmap_get_or_create_extended(path, ufid, base_path
		, flags TSRMLS_CC);
	if (!pmp) return NULL;

	/* Allocate and fill Automap_Mnt slot */

	mp=ut_eallocate(NULL, sizeof(*mp));
	CLEAR_DATA(*mp);	/* Init everything to 0/NULL */

	mp->map=pmp;

	mp->path=zend_string_dup(path,0);
	mp->flags=flags;
	
	Automap_Mnt_array_add(mp TSRMLS_CC);
	return mp;
}

/*---------------------------------------------------------------*/

static Automap_Mnt *Automap_Mnt_load(zend_string *path, long flags TSRMLS_DC)
{
	Automap_Mnt *mp;
	Automap_Pmap *pmp;
	zend_string *zsp;

	DBG_MSG("Starting Automap_Mnt_load");

	mp=NULL;
	pmp=NULL;

	/* Make path absolute */
	zsp=ut_mkAbsolutePath(path, 0 TSRMLS_CC);
	
	if (!(pmp=Automap_Pmap_get_or_create(zsp, flags TSRMLS_CC))) {
		zend_string_release(zsp);
		return NULL;
	}

	/* Allocate and fill Automap_Mnt slot */

	mp=ut_eallocate(NULL, sizeof(*mp));
	CLEAR_DATA(*mp);	/* Init everything to 0/NULL */

	mp->map=pmp;
	mp->path=zsp;
	mp->flags=flags;

	Automap_Mnt_array_add(mp TSRMLS_CC);
	return mp;
}

/*---------------------------------------------------------------*/
/* {{{ proto string \Automap\Mgr::load(string path[, int flags]) */

/* The $_bp arg of the PHP version of Automap\Mgr::load is only used when loading a
* package through the PHK PHP runtime . Here, as both extensions are merged,
* we know that PHK are loaded through their extension, using another mechanism.
* So, we don't need the $_bp parameter.
*/

static PHP_METHOD(Automap, load)
{
	char *path;
	int path_len;
	zend_string *zs;
	long flags;
	Automap_Mnt *mp;

	flags = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s|l", &path, &path_len
		, &flags) == FAILURE)
			EXCEPTION_ABORT("Cannot parse parameters");

	zs=zend_string_init(path,path_len,0);
	mp = Automap_Mnt_load(zs, flags TSRMLS_CC);
	zend_string_release(zs);

	if (EG(exception)) return;
	RETVAL_LONG(mp->id);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Must accept an invalid load point without error */

static void Automap_unload(long id TSRMLS_DC)
{
	Automap_Mnt *mp;

	if (!(mp=Automap_Mnt_get(id, 1 TSRMLS_CC))) return;

	Automap_Mnt_remove(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void \Automap\Mgr::unload(integer id) */

static PHP_METHOD(Automap, unload)
{
	zval *zid;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zid) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");
	convert_to_long(zid);

	Automap_unload(Z_LVAL_P(zid) TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto array \Automap\Mgr::activeIDs() */

static PHP_METHOD(Automap, activeIDs)
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

static int Automap_Mnt_resolve_key(Automap_Mnt *mp, zend_string *key TSRMLS_DC)
{
	char ftype,*req_str;
	int id;
	Automap_Pmap *pmp;
	Automap_Pmap_Entry *pep;
	
	req_str=NULL;

	if (mp->flags & AUTOMAP_FLAG_NO_AUTOLOAD) return FAILURE;

	pmp=mp->map;
	if (!(pep=Automap_Pmap_find_key(pmp,key TSRMLS_CC))) {
		return FAILURE;
	}

	switch(ftype=pep->ftype) {
		case AUTOMAP_F_EXTENSION:
			ut_load_extension_file(pep->fapath TSRMLS_CC);
			if (EG(exception)) RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
			Automap_callSuccessHandlers(mp,pep TSRMLS_CC);
			RETURN_AUTOMAP_MNT_RESOLVE_KEY(SUCCESS);
			break;

		case AUTOMAP_F_SCRIPT:
			/* Compute "require '<absolute path>';" */
			spprintf(&req_str,1024,"require '%s';",ZSTR_VAL(pep->fapath));
			DBG_MSG1("eval : %s",req_str);
			zend_eval_string(req_str,NULL,req_str TSRMLS_CC);
			Automap_callSuccessHandlers(mp,pep TSRMLS_CC);
			RETURN_AUTOMAP_MNT_RESOLVE_KEY(SUCCESS);
			break;

		case AUTOMAP_F_PACKAGE:	/* Symbol is in a package */
			id=PHK_Mgr_mount_from_Automap(pep->fapath,0 TSRMLS_CC);
			if (!id) {
				THROW_EXCEPTION_1("%s : Package inclusion should load a map"
					,ZSTR_VAL(pep->fapath));
				RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
			}
			RETURN_AUTOMAP_MNT_RESOLVE_KEY(
				Automap_Mnt_resolve_key(PHK_G(map_array)[id], key TSRMLS_CC));
			break;

		default:	/* Unknown target type (never happens in a valid map */
			THROW_EXCEPTION_1("<%c>: Unknown target type",ftype);
			RETURN_AUTOMAP_MNT_RESOLVE_KEY(FAILURE);
	}
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
	if (PHK_G(ext_is_enabled)) {
		/* Initialize map_array with an empty slot. Allows to start numbering IDs
		* from 1 (ID 0 is considered invalid) */

		EALLOCATE(PHK_G(map_array),sizeof(Automap_Mnt *));
		PHK_G(map_array)[0]=NULL;
		PHK_G(map_count)=1;
	}

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Mnt(TSRMLS_D)
{
	int i;
	Automap_Mnt *mp;

	if (PHK_G(ext_is_enabled)) {
		if (PHK_G(map_count) > 1) {
			for (i=1;i<PHK_G(map_count);i++) { /* Slot 0 is always NULL */
				mp=PHK_G(map_array)[i];
				if (mp) Automap_Mnt_remove(mp TSRMLS_CC);
			}
		}

		EALLOCATE(PHK_G(map_array),0);
		PHK_G(map_count)=0;
	}

	return SUCCESS;
}
