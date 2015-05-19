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

#include "ext/standard/php_versioning.h"

#define AUTOMAP_LOCK_pmap_array() { MutexLock(pmap_array); }
#define AUTOMAP_UNLOCK_pmap_array() { MutexUnlock(pmap_array); }

/*---------------------------------------------------------------*/

static Automap_Pmap *Automap_Pmap_get(zval *zufidp, ulong hash TSRMLS_DC)
{
	Automap_Pmap *pmp;
	int found;

	if (!ZVAL_IS_STRING(zufidp)) {
		EXCEPTION_ABORT_RET_1(NULL,"Automap_Pmap_get: UFID should be a string (type=%s)",
							  zend_zval_type_name(zufidp));
	}

	if (!hash) hash = ZSTRING_HASH(zufidp);

	found = (zend_hash_quick_find(&pmap_array, Z_STRVAL_P(zufidp),
		Z_STRLEN_P(zufidp) + 1, hash, (void **) &pmp) == SUCCESS);
	return (found ? pmp : NULL);
}

/*---------------------------------------------------------------*/
/* Callback only - Used by Automap_Pmap_get_or_create() */
/* zpp : Return from Automap_Map::_pecl_get_map(): array(stype,symbol,ptype,path) */

static int Automap_Pmap_create_entry(zval **zpp, zval **zsymbols	TSRMLS_DC)
	{
	Automap_Pmap_Entry tmp_entry;
	zval zkey,**zitem;
	HashTable *htp;
	char *p;

	if (!ZVAL_IS_ARRAY(*zpp)) {
		EXCEPTION_ABORT_RET_1(ZEND_HASH_APPLY_STOP,"Automap_Pmap_create_entry: Invalid entry (should be an array) %d",Z_TYPE_PP(zpp));
	}
	INIT_ZVAL(tmp_entry.zsname);
	INIT_ZVAL(tmp_entry.zfapath);

	/* Symbol type */

	htp=Z_ARRVAL_PP(zpp);
	zend_hash_index_find(htp,0,(void **)(&zitem));
	tmp_entry.stype=Z_STRVAL_PP(zitem)[0];

	/* Symbol name */

	zend_hash_move_forward(htp);
	zend_hash_get_current_data(htp,(void **)(&zitem));
	p=ut_pduplicate(Z_STRVAL_PP(zitem),Z_STRLEN_PP(zitem)+1);
	ZVAL_STRINGL(&(tmp_entry.zsname),p,Z_STRLEN_PP(zitem),0);
	
	/* Target type */

	zend_hash_move_forward(htp);
	zend_hash_get_current_data(htp,(void **)(&zitem));
	tmp_entry.ftype=Z_STRVAL_PP(zitem)[0];

	/* Target name */

	zend_hash_move_forward(htp);
	zend_hash_get_current_data(htp,(void **)(&zitem));
	p=ut_pduplicate(Z_STRVAL_PP(zitem),Z_STRLEN_PP(zitem)+1);
	ZVAL_STRINGL(&(tmp_entry.zfapath),p,Z_STRLEN_PP(zitem),0);

	/* Store entry in map */

	Automap_key(tmp_entry.stype, Z_STRVAL(tmp_entry.zsname)
		, Z_STRLEN(tmp_entry.zsname), &zkey TSRMLS_CC);
	zend_hash_update(Z_ARRVAL_PP(zsymbols),Z_STRVAL(zkey)
				,Z_STRLEN(zkey)+1,&tmp_entry,sizeof(tmp_entry),NULL);
	zval_dtor(&zkey);

	return ZEND_HASH_APPLY_KEEP;
}
	
/*---------------------------------------------------------------*/
/* Used for regular files only */
/* On entry, zapathp is an absolute path */

static Automap_Pmap *Automap_Pmap_get_or_create(zval *zapathp
	, long flags TSRMLS_DC)
{
	zval *zufidp;
	Automap_Pmap *pmp;

	/* Compute UFID (Unique File ID) */

	Automap_ufid(zapathp, &zufidp TSRMLS_CC);
	if (EG(exception)) return NULL;

	/* Run extended func */
	
	pmp=Automap_Pmap_get_or_create_extended(zapathp, zufidp
		, ZSTRING_HASH(zufidp), NULL, flags TSRMLS_DC);

	/* Cleanup */

	ut_ezval_ptr_dtor(&zufidp);

	return pmp;
}

/*---------------------------------------------------------------*/
/* Note: Separating 'get' and 'create' functions would break atomicity */

#define INIT_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	INIT_ZVAL(zdata); \
	INIT_ZVAL(zlong); \
	INIT_ZVAL(znull); \
	CLEAR_DATA(tmp_map); \
	}

#define CLEANUP_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	ut_ezval_dtor(&zdata); \
	ut_ezval_dtor(&zlong); \
	ut_ezval_dtor(&znull); \
	}

#define RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(_ret) \
	{ \
	CLEANUP_AUTOMAP_PMAP_GET_OR_CREATE(); \
	AUTOMAP_UNLOCK_pmap_array(); \
	return _ret; \
	}

#define ABORT_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	Automap_Pmap_dtor(&tmp_map); \
	RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(NULL); \
	}

/* On entry :
* - zpathp is an absolute path
* - zufidp is non null
* - hash is non null
* - If buf is non null, it contains the map data (don't read from path)
* - If zbase_pathp_arg is non null, it is the final absolute base
*       path (with trailing separator)
* - If zbase_pathp_arg is null, the value must be computed from the file path and
*       content
*/

static Automap_Pmap *Automap_Pmap_get_or_create_extended(zval *zpathp
	,zval *zufidp, ulong hash, zval *zbase_pathp_arg, long flags TSRMLS_DC)
{
	Automap_Pmap tmp_map, *pmp;
	zval zdata, *args[3], zlong, *map, znull;
	HashTable *htp;

	DBG_MSG1("Entering Automap_Pmap_get_or_create_extended(%s)",Z_STRVAL_P(zpathp));

	AUTOMAP_LOCK_pmap_array();

	pmp=Automap_Pmap_get(zufidp, hash TSRMLS_CC);
	if (pmp) { /* Already exists */
		AUTOMAP_UNLOCK_pmap_array(); \
		DBG_MSG2("Automap_Pmap cache hit (path=%s,ufid=%s)",Z_STRVAL_P(zpathp)
			,Z_STRVAL_P(zufidp));
		return pmp; \
	}

	/* Map is not in Pmap cache -> load it using \Automap\Map (PHP) */
	/*-- Slow path --*/

	DBG_MSG2("Automap_Pmap cache miss (path=%s,ufid=%s)",Z_STRVAL_P(zpathp)
			,Z_STRVAL_P(zufidp));

	INIT_AUTOMAP_PMAP_GET_OR_CREATE();

	PHK_need_php_runtime(TSRMLS_C);

	/* Instantiate \Automap\Map (load the map file) */

	args[0] = zpathp;
	ZVAL_LONG(&zlong,flags|AUTOMAP_FLAG_CRC_CHECK|AUTOMAP_FLAG_PECL_LOAD);
	args[1] = &zlong;
	args[2] = (zbase_pathp_arg ? zbase_pathp_arg : (&znull));
	map=ut_new_instance(ZEND_STRL("Automap\\Map"), YES, 3, args TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();

	/* Get data from Automap_Map object */

	ZVAL_LONG(&zlong,AUTOMAP_MAP_PROTOCOL);
	args[0] = &zlong;
	ut_call_user_function_array(map,ZEND_STRL("_pecl_get_map"),&zdata,1,args);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	if (!ZVAL_IS_ARRAY(&zdata)) {
		THROW_EXCEPTION_1("%s : Automap\\Map::_pecl_get_map() should return an array",Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	
	ut_ezval_ptr_dtor(&map);	/* Delete Automap_Map object */
	
	/* Move data to persistent storage */

	htp=(HashTable *)ut_pallocate(NULL,sizeof(HashTable));
	zend_hash_init(htp,zend_hash_num_elements(Z_ARRVAL(zdata)),NULL
		,(dtor_func_t)Automap_Pmap_Entry_dtor,1);
	ALLOC_INIT_PERMANENT_ZVAL(tmp_map.zsymbols);
	ZVAL_ARRAY(tmp_map.zsymbols,htp);
	zend_hash_apply_with_argument(Z_ARRVAL(zdata)
		,(apply_func_arg_t)Automap_Pmap_create_entry
		, (void *)(&tmp_map.zsymbols) TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	
	/* Create slot */

	zend_hash_quick_update(&pmap_array, Z_STRVAL_P(zufidp), Z_STRLEN_P(zufidp) + 1
		, hash, &tmp_map, sizeof(tmp_map), (void **) &pmp);

	/* Cleanup and return */

	RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(pmp);
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_dtor(Automap_Pmap *pmp)
{
	ut_pzval_ptr_dtor(&(pmp->zsymbols));
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_Entry_dtor(Automap_Pmap_Entry *pep)
{
	ut_pzval_dtor(&(pep->zsname));
	ut_pzval_dtor(&(pep->zfapath));
}

/*---------------------------------------------------------------*/
/* Returns SUCCESS/FAILURE */
/* Fast path */

static Automap_Pmap_Entry *Automap_Pmap_find_key(Automap_Pmap *pmp
	, zval *zkey, ulong hash TSRMLS_DC)
{
	Automap_Pmap_Entry *pep;

	pep=(Automap_Pmap_Entry *)NULL;
	zend_hash_quick_find(Z_ARRVAL_P(pmp->zsymbols),Z_STRVAL_P(zkey)
		,Z_STRLEN_P(zkey)+1,hash,(void **)(&pep));
	return pep;
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_export_entry(Automap_Pmap_Entry *pep, zval *zp TSRMLS_DC)
{
	char str[2];

	array_init(zp);
	str[1]='\0';

	str[0]=pep->stype;
	add_assoc_stringl(zp,"stype",str,1,1);
	add_assoc_stringl(zp,"symbol",Z_STRVAL(pep->zsname),Z_STRLEN(pep->zsname),1);
	str[0]=pep->ftype;
	add_assoc_stringl(zp,"ptype",str,1,1);
	add_assoc_stringl(zp,"path",Z_STRVAL(pep->zfapath),Z_STRLEN(pep->zfapath),1);
}

/*===============================================================*/

static int MINIT_Automap_Pmap(TSRMLS_D)
{
	MutexSetup(pmap_array);
	zend_hash_init(&pmap_array, 16, NULL,(dtor_func_t) Automap_Pmap_dtor, 1);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Pmap(TSRMLS_D)
{
	zend_hash_destroy(&pmap_array);
	MutexShutdown(pmap_array);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Pmap(TSRMLS_D)
{
	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Pmap(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
