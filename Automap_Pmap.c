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
/* zpp : Map entry as written in the file */

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
	Automap_Pmap_Entry tmp_entry;
	Automap_Pmap *pmp;
	char *sp, *p, *p2,*path,*apath;
	int len,path_len,apath_len;
	zval zkey, *zbase_path;

	pmp=va_arg(va, Automap_Pmap *);
	zbase_path=va_arg(va, zval *);	/* Absolute base path */

	if (!ZVAL_IS_STRING(*zpp)) {
		EXCEPTION_ABORT_RET_1(ZEND_HASH_APPLY_STOP,"Automap_Pmap_create_entry: Invalid entry (should be a string) %d",Z_TYPE_PP(zpp));
	}
	INIT_ZVAL(tmp_entry.zsname);
	INIT_ZVAL(tmp_entry.zfpath);
	INIT_ZVAL(tmp_entry.zfapath);
	sp=Z_STRVAL_PP(zpp);
	len=0; /* Avoid compile warning */

	switch(pmp->map_major_version) {
		/* Version 1 is not supported anymore */
		case '2':
		case '3':
			if (Z_STRLEN_PP(zpp)<5) {
				EXCEPTION_ABORT_RET_1(ZEND_HASH_APPLY_STOP
					,"Invalid value string: <%s>",Z_STRVAL_PP(zpp));
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
				EXCEPTION_ABORT_RET_1(ZEND_HASH_APPLY_STOP
					,"Invalid value string: <%s>",Z_STRVAL_PP(zpp));
			}
			/* Symbol name */
			p=ut_pduplicate(sp+2,len+1);
			ZVAL_STRINGL(&(tmp_entry.zsname),p,len,0);
			/* Target path */
			path_len=Z_STRLEN_PP(zpp)-len-3;
			path=ut_pduplicate(p2,path_len+1);
			ZVAL_STRINGL(&(tmp_entry.zfpath),path,path_len,0);
			/* Target absolute path */
			if (IS_ABSOLUTE_PATH(path,path_len)) { /* Target path is already absolute */
				apath=ut_pduplicate(path,path_len+1);
				apath_len=path_len;
			} else {
				apath_len=Z_STRLEN_P(zbase_path)+path_len;
				spprintf(&p,apath_len+1,"%s%s",Z_STRVAL_P(zbase_path),path);
				apath=ut_pduplicate(p,apath_len+1);
				EFREE(p);
			}
			ZVAL_STRINGL(&(tmp_entry.zfapath),apath,apath_len,0);

			Automap_key(tmp_entry.stype, Z_STRVAL(tmp_entry.zsname)
				, Z_STRLEN(tmp_entry.zsname), &zkey TSRMLS_CC);

			zend_hash_update(Z_ARRVAL_P(pmp->zsymbols),Z_STRVAL(zkey)
				,Z_STRLEN(zkey)+1,&tmp_entry,sizeof(tmp_entry),NULL);

			zval_dtor(&zkey);
			break;

		default:
			EXCEPTION_ABORT_RET_1(ZEND_HASH_APPLY_STOP
				,"Cannot understand this map version (%c)"
				,pmp->map_major_version);
	}

	return ZEND_HASH_APPLY_KEEP;
}
	
/*---------------------------------------------------------------*/
/* Used for regular files only */
/* On entry, zapathp is an absolute path */

static Automap_Pmap *Automap_Pmap_get_or_create(zval *zapathp
	, long flags TSRMLS_DC)
{
	zval *zufidp;
	ulong hash;
	Automap_Pmap *pmp;

	/* Compute UFID (Unique File ID) */

	Automap_ufid(zapathp, &zufidp TSRMLS_CC);
	hash=ZSTRING_HASH(zufidp);

	/* Run extended func */
	
	pmp=Automap_Pmap_get_or_create_extended(zapathp, NULL, 0, zufidp
		, ZSTRING_HASH(zufidp), NULL, flags TSRMLS_DC);

	/* Cleanup */

	ut_ezval_ptr_dtor(&zufidp);

	return pmp;
}

/*---------------------------------------------------------------*/
/* Note: Separating 'get' and 'create' functions would break atomicity */

#define INIT_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	INIT_ZVAL(zversion); \
	INIT_ZVAL(zbuf); \
	INIT_ZVAL(zdata); \
	INIT_ZVAL(zbase_path); \
	CLEAR_DATA(tmp_map); \
	}

#define CLEANUP_AUTOMAP_PMAP_GET_OR_CREATE() \
	{ \
	ut_ezval_dtor(&zversion); \
	ut_ezval_dtor(&zbuf); \
	ut_ezval_dtor(&zdata); \
	if (!zbase_pathp_arg) ut_ezval_dtor(&zbase_path); \
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
* - If zbase_pathp_arg is null, we compute its value from the file path and
*       content
* - pmap_flags:
*       AUTOMAP_PMAP_NO_CRC_CHECK: DOn't check CRC
*/

static Automap_Pmap *Automap_Pmap_get_or_create_extended(zval *zpathp
	, char *buf, int buflen, zval *zufidp, ulong hash
	, zval *zbase_pathp_arg, long flags TSRMLS_DC)
{
	Automap_Pmap tmp_map, *pmp;
	zval zversion, **zpp, zbuf, zdata, zbase_path;
	char tmpc, *p, *p2, crc[9], file_crc[8];
	unsigned long fsize;
	HashTable *htp;
	int len;

	DBG_MSG1("Entering Automap_Pmap_get_or_create_extended(%s)",Z_STRVAL_P(zpathp));

	AUTOMAP_LOCK_pmap_array();

	pmp=Automap_Pmap_get(zufidp, hash TSRMLS_CC);
	if (pmp) { /* Already exists */
		AUTOMAP_UNLOCK_pmap_array(); \
		DBG_MSG2("Automap_Pmap cache hit (path=%s,ufid=%s)",Z_STRVAL_P(zpathp)
			,Z_STRVAL_P(zufidp));
		return pmp; \
	}

	/* Map is not in memory -> load it */
	/*-- Slow path --*/

	DBG_MSG2("Automap_Pmap cache miss (path=%s,ufid=%s)",Z_STRVAL_P(zpathp)
			,Z_STRVAL_P(zufidp));

	INIT_AUTOMAP_PMAP_GET_OR_CREATE();

	if (!buf) {
		ut_file_get_contents(Z_STRVAL_P(zpathp), &zbuf TSRMLS_CC);
		if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
		buf = Z_STRVAL(zbuf);
		buflen = Z_STRLEN(zbuf);
	}

	/* File cannot be smaller than 62 bytes - Secure memory access */

	if (buflen < 62) {
		THROW_EXCEPTION_2("%s : Short file (size=%l)", Z_STRVAL_P(zpathp),buflen);
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
	len=ut_cut_at_space(p);
	if (php_version_compare(p, AUTOMAP_VERSION) > 0) {
		THROW_EXCEPTION_2
			("%s: Cannot understand this map. Requires at least Automap version %s",
			 Z_STRVAL_P(zpathp), p);
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	p[len]=' ';
	*p2 = tmpc;

	/* Check version against MIN_MAP_VERSION */

	p=&(buf[30]);
	p2=p+12;
	tmpc=*p2;
	*p2 = '\0';
	len=ut_cut_at_space(p);
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
	p[len]=' ';
	*p2 = tmpc;

	/* Check file size */

	tmpc = buf[53];
	buf[53] = '\0';
	fsize = 0;
	(void) sscanf(&(buf[45]), "%lu", &fsize);
	if (fsize != buflen) {
		THROW_EXCEPTION_2("%s : Invalid file size. Should be %lu",
						  Z_STRVAL_P(zpathp), fsize);
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	buf[53] = tmpc;

	/* Check CRC */

	if (!(flags & AUTOMAP_FLAG_NO_CRC_CHECK)) {
		memmove(file_crc,&buf[53],8);
		memmove(&buf[53],"00000000",8);
		ut_compute_crc32((unsigned char *)buf,(size_t)buflen,crc);
		if (memcmp(file_crc,crc,8)) {
			THROW_EXCEPTION("CRC error");
			ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
		}
	}

	/* All checks done, create data struct */

	tmp_map.zufid=ut_persist_zval(zufidp);
	tmp_map.ufid_hash=hash;
	
	/* Record minimum version (to be able to display it) */

	ZVAL_STRINGL(&zversion, &(buf[16]), 12, 1);
	ut_zval_cut_at_space(&zversion);
	tmp_map.zmin_version=ut_persist_zval(&zversion);
	ut_ezval_dtor(&zversion);

	/* Get map version */

	ZVAL_STRINGL(&zversion, &(buf[30]), 12, 1);
	ut_zval_cut_at_space(&zversion);
	tmp_map.zversion=ut_persist_zval(&zversion);
	tmp_map.map_major_version=Z_STRVAL(zversion)[0];
	ut_ezval_dtor(&zversion);

	/* Get the rest as an unserialized array */

	ut_unserialize_zval((unsigned char *)(buf + 61), buflen - 61, &zdata TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PMAP_GET_OR_CREATE();

	if (!ZVAL_IS_ARRAY(&zdata)) {
		THROW_EXCEPTION_1("%s : Map file should contain an array",Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}

	/* Get options array */

	if (FIND_HKEY(Z_ARRVAL(zdata), options, &zpp) != SUCCESS) {
		THROW_EXCEPTION_1("%s : No options array", Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	if (!ZVAL_IS_ARRAY(*zpp)) {
		THROW_EXCEPTION_1("%s : Options should be an array",
						  Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	tmp_map.zoptions=ut_persist_zval(*zpp);

	/* Compute base path */
	
	if (zbase_pathp_arg) {
		/* If base_path arg is provided, use it */
		ZVAL_STRINGL(&zbase_path,Z_STRVAL_P(zbase_pathp_arg)
			,Z_STRLEN_P(zbase_pathp_arg),0); /* No copy */
	} else {
		if (FIND_HKEY(Z_ARRVAL_P(tmp_map.zoptions), base_path, &zpp) == SUCCESS) {
			/* base_path option is set */
			if (!ZVAL_IS_STRING(*zpp)) {
				THROW_EXCEPTION_1("%s : base_path option must be a string",
								  Z_STRVAL_P(zpathp));
				ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
			}
			if (IS_ABSOLUTE_PATH(Z_STRVAL_PP(zpp),Z_STRLEN_PP(zpp))) {
				/* base_path option is an absolute path */
				p=ut_eduplicate(Z_STRVAL_PP(zpp),Z_STRLEN_PP(zpp)+1);
				len=Z_STRLEN_PP(zpp);
			} else {
				/* base path option is a relative path */
				spprintf(&p,MAXPATHLEN,"%s/%s/",ut_absolute_dirname(
					Z_STRVAL_P(zpathp),Z_STRLEN_P(zpathp),&len,0 TSRMLS_CC)
					,Z_STRVAL_PP(zpp));
				len+=Z_STRLEN_PP(zpp)+2;
			}
			EFREE(p2);
			p2=p;
		} else {
			/* base_path option is not set */
			p=ut_absolute_dirname(Z_STRVAL_P(zpathp),Z_STRLEN_P(zpathp)
				,&len,1 TSRMLS_CC);
		}
		ZVAL_STRINGL(&zbase_path,p,len,0);
	}

	/* Get symbol table */

	if (FIND_HKEY(Z_ARRVAL(zdata), map, &zpp) != SUCCESS) {
		THROW_EXCEPTION_1("%s : No symbol table", Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}
	if (!ZVAL_IS_ARRAY(*zpp)) {
		THROW_EXCEPTION_1("%s : Symbol table should contain an array",
						  Z_STRVAL_P(zpathp));
		ABORT_AUTOMAP_PMAP_GET_OR_CREATE();
	}

	/* Process symbols */

	htp=(HashTable *)ut_pallocate(NULL,sizeof(HashTable));
	zend_hash_init(htp,zend_hash_num_elements(Z_ARRVAL_PP(zpp)),NULL
		,(dtor_func_t)Automap_Pmap_Entry_dtor,1);
	ALLOC_INIT_PERMANENT_ZVAL(tmp_map.zsymbols);
	ZVAL_ARRAY(tmp_map.zsymbols,htp);
	zend_hash_apply_with_arguments(Z_ARRVAL_PP(zpp)
#if PHP_API_VERSION >= 20090626
		/* PHP 5.3+ requires this */
		TSRMLS_CC
#endif
		,(apply_func_args_t)Automap_Pmap_create_entry, 2, &tmp_map, &zbase_path
#if PHP_API_VERSION < 20090626
		TSRMLS_CC
#endif
		);
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
	ut_pzval_ptr_dtor(&(pmp->zufid));
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
