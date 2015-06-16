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

static Automap_Pmap *Automap_Pmap_get(zend_string *ufid TSRMLS_DC)
{
	Automap_Pmap **pmpp;
	int found;

	found = (FIND_ZSTRING(&pmap_array, ufid, (void **) &pmpp) == SUCCESS);
	return (found ? (*pmpp) : NULL);
}

/*---------------------------------------------------------------*/
/* Callback only - Used by Automap_Pmap_get_or_create() */
/* zpp : Return from Automap_Map::_peclGetMap(): array(stype,symbol,ptype,path) */

static int Automap_Pmap_create_entry(zval **zpp, HashTable *zsymbols TSRMLS_DC)
	{
	Automap_Pmap_Entry *entry;
	zval **zitem;
	HashTable *htp;
	zend_string *key;

	if (!ZVAL_IS_ARRAY(*zpp)) {
		EXCEPTION_ABORT_RET_1(ZEND_HASH_APPLY_STOP,"Automap_Pmap_create_entry: Invalid entry (should be an array) %d",Z_TYPE_PP(zpp));
	}

	entry=ut_pallocate(NULL,sizeof(*entry));

	/* Symbol type */

	htp=Z_ARRVAL_PP(zpp);
	zend_hash_index_find(htp,0,(void **)(&zitem));
	entry->stype=Z_STRVAL_PP(zitem)[0];

	/* Symbol name */

	zend_hash_move_forward(htp);
	zend_hash_get_current_data(htp,(void **)(&zitem));
	entry->sname=zend_string_init(Z_STRVAL_PP(zitem),Z_STRLEN_PP(zitem),1);
	
	/* Target type */

	zend_hash_move_forward(htp);
	zend_hash_get_current_data(htp,(void **)(&zitem));
	entry->ftype=Z_STRVAL_PP(zitem)[0];

	/* Target name */

	zend_hash_move_forward(htp);
	zend_hash_get_current_data(htp,(void **)(&zitem));
	entry->fapath=zend_string_init(Z_STRVAL_PP(zitem),Z_STRLEN_PP(zitem),1);

	/* Store entry in map */

	key=Automap_key(entry->stype, entry->sname TSRMLS_CC);
	zend_hash_update(zsymbols,ZSTR_VAL(key),ZSTR_LEN(key) + 1, entry, sizeof(entry),NULL);
	zend_string_release(key);

	return ZEND_HASH_APPLY_KEEP;
}
	
/*---------------------------------------------------------------*/
/* Used for regular files only */
/* On entry, zapathp is an absolute path */

static Automap_Pmap *Automap_Pmap_get_or_create(zend_string *path, long flags TSRMLS_DC)
{
	zend_string *ufid;
	Automap_Pmap *pmp;

	/* Compute UFID (Unique File ID) */

	ufid=Automap_ufid(path TSRMLS_CC);
	if (EG(exception)) return NULL;

	/* Run extended func */
	
	pmp=Automap_Pmap_get_or_create_extended(path, ufid, NULL, flags TSRMLS_CC);

	/* Cleanup */

	zend_string_release(ufid);

	return pmp;
}

/*---------------------------------------------------------------*/
/* Note: Separating 'get' and 'create' functions would break atomicity */

#define INIT_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	INIT_ZVAL(zdata); \
	INIT_ZVAL(zlong); \
	INIT_ZVAL(znull); \
	pmp=NULL; \
	}

#define CLEANUP_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	ut_ezval_dtor(&zdata); \
	}

#define RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(_ret) \
	{ \
	CLEANUP_AUTOMAP_PMAP_GET_OR_CREATE(); \
	AUTOMAP_UNLOCK_pmap_array(); \
	return _ret; \
	}

#define ABORT_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	Automap_Pmap_dtor(&pmp); \
	RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(NULL); \
	}

/* On entry :
* - path is an absolute path
* - ufid is non null
* - If buf is non null, it contains the map data (don't read from path)
* - If base_path_arg is non null, it is the final absolute base
*       path (with trailing separator)
* - If base_path_arg is null, the value must be computed from the file path and
*       content
*/

static Automap_Pmap *Automap_Pmap_get_or_create_extended(zend_string *path
	,zend_string *ufid, zend_string *base_path_arg, long flags TSRMLS_DC)
{
	Automap_Pmap *pmp;
	zval zdata, *args[3], zlong, *map, znull, zpath, zbase;

	DBG_MSG1("Entering Automap_Pmap_get_or_create_extended(%s)",ZSTR_VAL(path));

	AUTOMAP_LOCK_pmap_array();

	pmp=Automap_Pmap_get(ufid TSRMLS_CC);
	if (pmp) { /* Already exists */
		AUTOMAP_UNLOCK_pmap_array(); \
		DBG_MSG2("Automap_Pmap cache hit (path=%s,ufid=%s)",ZSTR_VAL(path),ZSTR_VAL(ufid));
		return pmp; \
	}

	/* Map is not in Pmap cache -> load it using \Automap\Map (PHP) */
	/*-- Slow path --*/

	DBG_MSG2("Automap_Pmap cache miss (path=%s,ufid=%s)",ZSTR_VAL(path),ZSTR_VAL(ufid));

	INIT_AUTOMAP_PMAP_GET_OR_CREATE();

	PHK_needPhpRuntime(TSRMLS_C);

	/* Instantiate \Automap\Map (load the map file) */

	INIT_ZVAL(zpath);
	ZVAL_STR(&zpath,path);
	args[0] = &zpath;
	ZVAL_LONG(&zlong,flags|AUTOMAP_FLAG_CRC_CHECK|AUTOMAP_FLAG_PECL_LOAD);
	args[1] = &zlong;
	INIT_ZVAL(zbase);
	if (base_path_arg) ZVAL_STR(&zbase,base_path_arg);
	args[2] = &zbase;
	map=ut_new_instance(ZEND_STRL("Automap\\Map"), YES, 3, args TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();

	/* Get data from Automap_Map object */

	ZVAL_LONG(&zlong,AUTOMAP_MAP_PROTOCOL);
	args[0] = &zlong;
	ut_call_user_function_array(map,ZEND_STRL("_peclGetMap"),&zdata,1,args TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	if (!ZVAL_IS_ARRAY(&zdata)) {
		THROW_EXCEPTION_1("%s : Automap\\Map::_peclGetMap() should return an array",ZSTR_VAL(path));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	
	ut_ezval_ptr_dtor(&map);	/* Delete Automap_Map object */
	
	/* Move data to persistent storage */

	pmp=(Automap_Pmap *)ut_pallocate(NULL,sizeof(*pmp));
	zend_hash_init(&(pmp->zsymbols),zend_hash_num_elements(Z_ARRVAL(zdata)),NULL
		,(dtor_func_t)Automap_Pmap_Entry_dtor,1);
	zend_hash_apply_with_argument(Z_ARRVAL(zdata)
		,(apply_func_arg_t)Automap_Pmap_create_entry
		, (void *)(&(pmp->zsymbols)) TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	
	/* Create slot */

	zend_hash_quick_update(&pmap_array, _ZSTR_VALUES(ufid), &pmp, sizeof(pmp), NULL);

	/* Cleanup and return */

	RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(pmp);
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_dtor(Automap_Pmap **pmpp)
{
	Automap_Pmap *pmp;
	
	pmp = (*pmpp);
	zend_hash_destroy(&(pmp->zsymbols));
	ut_pallocate(pmp,0);
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_Entry_dtor(Automap_Pmap_Entry **pepp)
{
	Automap_Pmap_Entry *pep;
	
	pep = (*pepp);
	zend_string_release(pep->sname);
	zend_string_release(pep->fapath);
	ut_pallocate(pep,0);
}

/*---------------------------------------------------------------*/
/* Returns NULL on failure */
/* Fast path */

static Automap_Pmap_Entry *Automap_Pmap_find_key(Automap_Pmap *pmp
	, zend_string *key TSRMLS_DC)
{
	Automap_Pmap_Entry **pepp;
	int status;

	pepp=(Automap_Pmap_Entry **)NULL;
	status=FIND_ZSTRING(&(pmp->zsymbols),key,(void **)(&pepp));
	return ((status==SUCCESS) ? (*pepp) : NULL);
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_export_entry(Automap_Pmap_Entry *pep, zval *zp TSRMLS_DC)
{
	char str[2];

	array_init(zp);
	str[1]='\0';

	str[0]=pep->stype;
	add_assoc_stringl(zp,"stype",str,1,1);
	add_assoc_stringl(zp,"symbol",ZSTR_VAL(pep->sname),ZSTR_LEN(pep->sname),1);
	str[0]=pep->ftype;
	add_assoc_stringl(zp,"ptype",str,1,1);
	add_assoc_stringl(zp,"path",ZSTR_VAL(pep->fapath),ZSTR_LEN(pep->fapath),1);
}

/*===============================================================*/

static int MINIT_Automap_Pmap(TSRMLS_D)
{
	if (PHK_G(ext_is_enabled)) {
		MutexSetup(pmap_array);
		zend_hash_init(&pmap_array, 16, NULL,(dtor_func_t) Automap_Pmap_dtor, 1);
	}

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Pmap(TSRMLS_D)
{
	if (PHK_G(ext_is_enabled)) {
		zend_hash_destroy(&pmap_array);
		MutexShutdown(pmap_array);
	}

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
