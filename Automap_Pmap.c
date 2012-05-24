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

#include "ext/standard/php_versioning.h"

#define AUTOMAP_LOCK_PTAB() { MutexLock(ptab); }
#define AUTOMAP_UNLOCK_PTAB() { MutexUnlock(ptab); }

/*---------------------------------------------------------------*/

static Automap_Pmap *Automap_Pmap_get(zval *zmntp, ulong hash TSRMLS_DC)
{
	Automap_Pmap *pmp;
	int found;

	if (!ZVAL_IS_STRING(zmntp)) {
		EXCEPTION_ABORT_RET_1(NULL,"Automap_Pmap_get: Map ID should be a string (type=%s)",
							  zend_zval_type_name(zmntp));
	}

	if (!hash) hash = ZSTRING_HASH(zmntp);

	found = (zend_hash_quick_find(&ptab, Z_STRVAL_P(zmntp),
		Z_STRLEN_P(zmntp) + 1, hash, (void **) &pmp) == SUCCESS);
	return (found ? pmp : NULL);
}

/*---------------------------------------------------------------*/
/* Callback only - Used by Automap_Pmap_get_or_create() */

static int Automap_Pmap_create_entry(zval **zpp
#if PHP_API_VERSION >= 20090626
		/* PHP 5.3+ requires this */
	TSRMLS_DC
#endif
	, int num_args, va_list va, zend_hash_key *hash_key
#if PHP_API_VERSION < 20090626
	TSRMLS_DC
#endif
	)
	{
	HashTable *htp;
	Automap_Pmap_Entry tmp_entry;
	char *sp, *p, *p2;
	int len, map_major_version;
	zval zkey;

	htp=va_arg(va, HashTable *);
	map_major_version=va_arg(va, int);
	
	if (!ZVAL_IS_STRING(*zpp)) {
		EXCEPTION_ABORT_RET_1(ZEND_HASH_APPLY_STOP,"Automap_Pmap_create_entry: Invalid entry (should be a string) %d",Z_TYPE_PP(zpp));
	}
	INIT_ZVAL(tmp_entry.zsname);
	INIT_ZVAL(tmp_entry.zfpath);
	INIT_ZVAL(zkey);
	sp=Z_STRVAL_PP(zpp);
	
	switch(map_major_version) {
		case '1':
			if ((hash_key->nKeyLength<3)||(Z_STRLEN_PP(zpp)<2)) {
				EXCEPTION_ABORT_RET(ZEND_HASH_APPLY_STOP,"Automap_Pmap_create_entry: Invalid entry (V1 map)");
			}
			tmp_entry.stype=hash_key->arKey[0];
			p=pduplicate((void *)(&(hash_key->arKey[1])),(len=hash_key->nKeyLength-2)+1);
			ZVAL_STRINGL(&(tmp_entry.zsname),p,len,0);
			ZVAL_STRINGL(&zkey,hash_key->arKey,len+1,0);
			len=Z_STRLEN_PP(zpp)-1;
			tmp_entry.ftype=sp[0];
			p=pduplicate(sp+1,len);
			ZVAL_STRINGL(&(tmp_entry.zfpath),p,len,0);
			zend_hash_update(htp,Z_STRVAL(zkey),Z_STRLEN(zkey)+1
				,&tmp_entry,sizeof(tmp_entry),NULL);
			break;

		default:
			if (Z_STRLEN_PP(zpp)<5) {
				EXCEPTION_ABORT_RET(ZEND_HASH_APPLY_STOP,"Automap_Pmap_create_entry: Invalid entry (too short)");
			}

			tmp_entry.stype=sp[0];
			tmp_entry.ftype=sp[1];
			for (p=sp+2, p2=NULL; *p; p++) {
				if ((*p)=='|') {
					(*p)='\0';
					p2=p+1;
					len=p-sp-2;
				}
			}
			if (!p2) {
				EXCEPTION_ABORT_RET(ZEND_HASH_APPLY_STOP,"Automap_Pmap_create_entry: Invalid entry");
			}
			p=pduplicate(sp+2,len+1);
			ZVAL_STRINGL(&(tmp_entry.zsname),p,len,0);
			len=Z_STRLEN_PP(zpp)-len-3;
			p=pduplicate(p2,len+1);
			ZVAL_STRINGL(&(tmp_entry.zfpath),p,len,0);

			Automap_key(tmp_entry.stype, Z_STRVAL(tmp_entry.zsname), Z_STRLEN(tmp_entry.zsname)
				, &zkey TSRMLS_CC);

			zend_hash_update(htp,Z_STRVAL(zkey),Z_STRLEN(zkey)+1
				,&tmp_entry,sizeof(tmp_entry),NULL);

			zval_dtor(&zkey);
	}

	return ZEND_HASH_APPLY_KEEP;
}
	
/*---------------------------------------------------------------*/
/* Note: Separating 'get' and 'create' functions would break atomicity */

#define INIT_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	ALLOC_INIT_ZVAL(zversion); \
	ALLOC_INIT_ZVAL(zbuf); \
	ALLOC_INIT_ZVAL(zdata); \
	ALLOC_INIT_ZVAL(zkey); \
	CLEAR_DATA(tmp_map); \
	}

#define CLEANUP_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	ut_ezval_ptr_dtor(&zversion); \
	ut_ezval_ptr_dtor(&zbuf); \
	ut_ezval_ptr_dtor(&zdata); \
	ut_ezval_ptr_dtor(&zkey); \
	}

#define RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(_ret) \
	{ \
	CLEANUP_AUTOMAP_PMAP_GET_OR_CREATE(); \
	AUTOMAP_UNLOCK_PTAB(); \
	return _ret; \
	}

#define ABORT_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	Automap_Pmap_dtor(&tmp_map); \
	RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(NULL); \
	}

static Automap_Pmap *Automap_Pmap_get_or_create(zval *zpathp, zval *zmntp
	, ulong hash TSRMLS_DC)
{
	Automap_Pmap tmp_map, *pmp;
	zval *zversion, **zpp, *zbuf, *zdata, *zkey;
	char *buf, tmpc, *p, *p2;
	unsigned long fsize, len;
	HashTable *htp;

	DBG_MSG1("Entering Automap_Pmap_get_or_create(%s)",Z_STRVAL_P(zpathp));

	AUTOMAP_LOCK_PTAB();

	if (!hash) hash=ZSTRING_HASH(zmntp);

	pmp=Automap_Pmap_get(zmntp, hash TSRMLS_CC);
	if (pmp) { /* Already exists */
		AUTOMAP_UNLOCK_PTAB(); \
		return pmp; \
	}

	/*-- Slow path --*/

	INIT_AUTOMAP_PMAP_GET_OR_CREATE();

	ut_file_get_contents(Z_STRVAL_P(zpathp), zbuf TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	buf = Z_STRVAL_P(zbuf);
	len = Z_STRLEN_P(zbuf);

	/* File cannot be smaller than 54 bytes - Secure future memory access */

	if (len < 54) {
		THROW_EXCEPTION_1("%s : file is too small", Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}

	/* Check magic string */

	if (memcmp(buf, AUTOMAP_MAGIC, sizeof(AUTOMAP_MAGIC) - 1)) {
		THROW_EXCEPTION_1("%s : Bad magic", Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}

	/* Check minimum required runtime version */

	p=&(buf[16]);
	p2=p+12;
	tmpc = *p2;
	*p2 = '\0';
	ut_cut_at_space(p);
	if (php_version_compare(p, AUTOMAP_API) > 0) {
		THROW_EXCEPTION_2
			("%s: Cannot understand this map. Requires at least Automap version %s",
			 Z_STRVAL_P(zpathp), p);
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	*p2 = tmpc;

	/* Check version against MIN_MAP_VERSION */

	p=&(buf[30]);
	p2=p+12;
	tmpc=*p2;
	*p2 = '\0';
	ut_cut_at_space(p);
	if (!(*p)) {
		THROW_EXCEPTION_1("%s: Invalid empty map version",Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	if (php_version_compare(&(buf[30]),AUTOMAP_MIN_MAP_VERSION) < 0) {
		THROW_EXCEPTION_3
			("%s: Cannot understand this map. Format too old (%s < %s)"
			, Z_STRVAL_P(zpathp),&(buf[30]),AUTOMAP_MIN_MAP_VERSION);
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	*p2 = tmpc;

	/* Check file size */

	tmpc = buf[53];
	buf[53] = '\0';
	fsize = 0;
	(void) sscanf(&(buf[45]), "%lu", &fsize);
	if (fsize != len) {
		THROW_EXCEPTION_2("%s : Invalid file size. Should be %lu",
						  Z_STRVAL_P(zpathp), fsize);
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	buf[53] = tmpc;

	/* All checks done, create data struct */

	tmp_map.zmnt=ut_persist_zval(zmntp);
	tmp_map.mnt_hash=hash;
	
	/* Record minimum version (to be able to display it) */

	ZVAL_STRINGL(zversion, &(buf[16]), 12, 1);
	ut_zval_cut_at_space(zversion);
	tmp_map.zmin_version=ut_persist_zval(zversion);
	ut_ezval_dtor(zversion);

	/* Get map version */

	ZVAL_STRINGL(zversion, &(buf[30]), 12, 1);
	ut_zval_cut_at_space(zversion);
	tmp_map.zversion=ut_persist_zval(zversion);
	ut_ezval_dtor(zversion);

	/* Get the rest as an unserialized array */

	ut_unserialize_zval((unsigned char *)(buf + 53), len - 53, zdata TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();

	if (!ZVAL_IS_ARRAY(zdata)) {
		THROW_EXCEPTION_1("%s : Map file should contain an array",Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}

	/* Get the options array */

	if (FIND_HKEY(Z_ARRVAL_P(zdata), options, &zpp) != SUCCESS) {
		THROW_EXCEPTION_1("%s : No options array", Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	if (!ZVAL_IS_ARRAY(*zpp)) {
		THROW_EXCEPTION_1("%s : Cannot load map - Options should be an array",
						  Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	tmp_map.zoptions=ut_persist_zval(*zpp);

	/* Get the symbol table */

	if (FIND_HKEY(Z_ARRVAL_P(zdata), map, &zpp) != SUCCESS) {
		THROW_EXCEPTION_1("%s : Cannot load map - No symbol table", Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	if (!ZVAL_IS_ARRAY(*zpp)) {
		THROW_EXCEPTION_1("%s : Cannot load map - Symbol table should contain an array",
						  Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}

	htp=(HashTable *)pallocate(NULL,sizeof(HashTable));
	zend_hash_init(htp,zend_hash_num_elements(Z_ARRVAL_PP(zpp)),NULL
		,(dtor_func_t)Automap_Pmap_Entry_dtor,1);
	ALLOC_INIT_PERMANENT_ZVAL(tmp_map.zsymbols);
	ZVAL_ARRAY(tmp_map.zsymbols,htp);
	zend_hash_apply_with_arguments(Z_ARRVAL_PP(zpp)
#if PHP_API_VERSION >= 20090626
		/* PHP 5.3+ requires this */
		TSRMLS_CC
#endif
		,(apply_func_args_t)Automap_Pmap_create_entry, 2, htp
		, *Z_STRVAL_P(tmp_map.zversion)
#if PHP_API_VERSION < 20090626
		TSRMLS_CC
#endif
		);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	
	/* Create slot */

	zend_hash_quick_update(&ptab, Z_STRVAL_P(zmntp), Z_STRLEN_P(zmntp) + 1
		, hash, &tmp_map, sizeof(tmp_map), (void **) &pmp);

	/* Cleanup and return */

	RETURN_FROM_AUTOMAP_PMAP_GET_OR_CREATE(pmp);
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_dtor(Automap_Pmap *pmp)
{
	ut_pzval_ptr_dtor(&(pmp->zmnt));
	ut_pzval_ptr_dtor(&(pmp->zmin_version));
	ut_pzval_ptr_dtor(&(pmp->zversion));
	ut_pzval_ptr_dtor(&(pmp->zoptions));
	ut_pzval_ptr_dtor(&(pmp->zsymbols));
}

/*---------------------------------------------------------------*/

static void Automap_Pmap_Entry_dtor(Automap_Pmap_Entry *pep)
{
	ut_pzval_dtor(&(pep->zsname));
	ut_pzval_dtor(&(pep->zfpath));
}

/*---------------------------------------------------------------*/
/* Returns SUCCESS/FAILURE */

static Automap_Pmap_Entry *Automap_Pmap_find_key(Automap_Pmap *pmp
	, zval *zkey, ulong hash TSRMLS_DC)
{
	Automap_Pmap_Entry *pep;

	pep=(Automap_Pmap_Entry *)NULL;
	zend_hash_quick_find(Z_ARRVAL_P(pmp->zsymbols),Z_STRVAL_P(zkey)
		,Z_STRLEN_P(zkey)+1,hash,(void **)(&pep));
	return pep;
}

/*===============================================================*/

static int MINIT_Automap_Pmap(TSRMLS_D)
{
	MutexSetup(ptab);
	zend_hash_init(&ptab, 16, NULL,(dtor_func_t) Automap_Pmap_dtor, 1);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Pmap(TSRMLS_D)
{
	zend_hash_destroy(&ptab);
	MutexShutdown(ptab);

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
