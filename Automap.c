/*
  +----------------------------------------------------------------------+
  | Automap extension <http://automap.tekwire.net>                     |
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

#define AUTOMAP_VERSION ""

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_versioning.h"
#include "zend_constants.h"
#include "zend_execute.h"
#include "zend_errors.h"
#include "TSRM/tsrm_virtual_cwd.h"

#include "php_automap.h"
#include "utils.h"
#include "Automap.h"

ZEND_EXTERN_MODULE_GLOBALS(automap)

/*---------------------------------------------------------------*/

typedef struct {				/* Private */
	unsigned long nb_hits;
	unsigned long refcount;
	time_t ctime;

	zval min_version;			/* String */
	zval version;				/* String */
	zval options;				/* Array */
	zval symbols;				/* Array */
} Automap_Mnt_Persistent_Data;

static HashTable persistent_mtab;

MutexDeclare(persistent_mtab);

/*---------------------------------------------------------------*/

#define AUTOMAP_GET_INSTANCE_DATA(_func) \
	zval **_tmp; \
	Automap_Mnt_Info *mp; \
	\
	DBG_MSG("Entering Automap::" #_func); \
	CHECK_MEM(); \
	FIND_HKEY(Z_OBJPROP_P(getThis()),m,&_tmp); \
	mp=*((Automap_Mnt_Info **)(Z_STRVAL_PP(_tmp)));

/*---------------------------------------------------------------*/

static void Automap_init_persistent_data(TSRMLS_D);
static void Automap_shutdown_persistent_data(TSRMLS_D);
static void Automap_mnt_info_dtor(Automap_Mnt_Info * mp);
static void Automap_remove_mnt_info(zval * mnt TSRMLS_DC);
static Automap_Mnt_Info *Automap_new_mnt_info(zval * mnt,ulong hash TSRMLS_DC);
static Automap_Mnt_Info *Automap_get_mnt_info(zval * mnt, ulong hash, int exception TSRMLS_DC);
static void Automap_validate(zval * mnt, ulong hash TSRMLS_DC);
static void Automap_umount(zval * mnt, ulong hash TSRMLS_DC);
static zval *Automap_instance_by_mp(Automap_Mnt_Info * mp TSRMLS_DC);
static zval *Automap_instance(zval * mnt, ulong hash TSRMLS_DC);
static void Automap_mnt_list(zval * ret TSRMLS_DC);
static void Automap_path_to_mnt(zval * path, zval * mnt TSRMLS_DC);
static void Automap_compute_mnt(zval * path, zval ** mnt TSRMLS_DC);
static Automap_Mnt_Info *Automap_mount(zval * path, zval * base_dir, zval * mnt, int flags TSRMLS_DC);
static Automap_Mnt_Persistent_Data *Automap_get_persistent_data(zval * mnt, ulong hash TSRMLS_DC);
static Automap_Mnt_Persistent_Data *Automap_get_or_create_persistent_data(Automap_Mnt_Info * mp TSRMLS_DC);
static void Automap_populate_persistent_data(Automap_Mnt_Info *mp TSRMLS_DC);
static void Automap_Persistent_Data_dtor(Automap_Mnt_Persistent_Data *entry);
static char *Automap_get_type_string(char type TSRMLS_DC);
static char Automap_string_to_type(char *string TSRMLS_DC);
static void Automap_key(char type, char *symbol, unsigned long len, zval *ret TSRMLS_DC);
static void Automap_get_type_from_key(char *key, zval *ret TSRMLS_DC);
static void Automap_get_symbol_from_key(char *key, zval *ret TSRMLS_DC);
static int Automap_symbol_is_defined(char type, char *symbol, unsigned int slen TSRMLS_DC);
static int Automap_get_symbol(char type, char *symbol, int slen, int autoload, int exception TSRMLS_DC);
static unsigned long Automap_symbol_count(Automap_Mnt_Info *mp TSRMLS_DC);
static void Automap_call_failure_handlers(zval *key TSRMLS_DC);
static void Automap_call_success_handlers(zval *key, zval *value, Automap_Mnt_Info *mp TSRMLS_DC);
static int Automap_resolve_key(zval *key, unsigned long hash, Automap_Mnt_Info *mp TSRMLS_DC);
static void Automap_html_show(Automap_Mnt_Info *mp, zval * subfile_to_url_function TSRMLS_DC);

static void Automap_set_mp_property(zval * obj,Automap_Mnt_Info * mp TSRMLS_DC);
static void Automap_register_autoload_hook(TSRMLS_D);

/*---------------------------------------------------------------*/
/*--- Init persistent data */

static void Automap_init_persistent_data(TSRMLS_D)
{
	MutexSetup(persistent_mtab);

	zend_hash_init(&persistent_mtab, 16, NULL,
				   (dtor_func_t) Automap_Persistent_Data_dtor, 1);
}

/*---------------------------------------------------------------*/

static void Automap_shutdown_persistent_data(TSRMLS_D)
{
	zend_hash_destroy(&persistent_mtab);

	MutexShutdown(persistent_mtab);
}

/*---------------------------------------------------------------*/
/* Here, we check every pointers because the function can be called during
   the creation of the structure (mount failure) */
 
static void Automap_mnt_info_dtor(Automap_Mnt_Info * mp)
{
	if (mp->refcountp) (*(mp->refcountp))--;

	if (mp->mnt) zval_ptr_dtor(&(mp->mnt));
	if (mp->instance) zval_ptr_dtor(&(mp->instance));
	if (mp->path) zval_ptr_dtor(&(mp->path));
	if (mp->base_dir) zval_ptr_dtor(&(mp->base_dir));
	if (mp->flags) zval_ptr_dtor(&(mp->flags));

	/* The following calls just decrement the refcount, as it is always >1 */

	if (mp->min_version) zval_ptr_dtor(&(mp->min_version));
	if (mp->version) zval_ptr_dtor(&(mp->version));
	if (mp->options) zval_ptr_dtor(&(mp->options));
	if (mp->symbols) zval_ptr_dtor(&(mp->symbols));
}

/*---------------------------------------------------------------*/

static void Automap_remove_mnt_info(zval * mnt TSRMLS_DC)
{
	Automap_Mnt_Info *mp;

	if (AUTOMAP_G(mcount)) {
		if (zend_hash_find(AUTOMAP_G(mtab), Z_STRVAL_P(mnt),
			Z_STRLEN_P(mnt) + 1, (void **) &mp) == SUCCESS) {
			AUTOMAP_G(mount_order)[mp->order]=NULL;
			(void)zend_hash_del(AUTOMAP_G(mtab), Z_STRVAL_P(mnt)
				,Z_STRLEN_P(mnt)+1);
		}
	}
}

/*---------------------------------------------------------------*/
/* If the mnt_info cannot be created, clear it in mtab and return NULL */

static Automap_Mnt_Info *Automap_new_mnt_info(zval * mnt, ulong hash TSRMLS_DC)
{
	Automap_Mnt_Info tmp, *mp;

	if (!hash) hash = ZSTRING_HASH(mnt);

	if (!AUTOMAP_G(mcount)) { /* First mount for this request */
		EALLOCATE(AUTOMAP_G(mtab),sizeof(HashTable));
		zend_hash_init(AUTOMAP_G(mtab), 16, NULL,
					   (dtor_func_t) Automap_mnt_info_dtor, 0);

		Automap_register_autoload_hook(TSRMLS_C);
	}

	memset(&tmp, 0, sizeof(tmp));	/* Init everything to 0/NULL */
	zend_hash_quick_update(AUTOMAP_G(mtab), Z_STRVAL_P(mnt),
						   Z_STRLEN_P(mnt) + 1, hash, &tmp, sizeof(tmp),
						   (void **) &mp);

	mp->hash = hash;
	mp->mnt = mnt;				/* Don't add ref */
	mp->order=AUTOMAP_G(mcount);
	mp->mnt_count=1;

	EALLOCATE(AUTOMAP_G(mount_order),(AUTOMAP_G(mcount) + 1) * sizeof(mp));
	AUTOMAP_G(mount_order)[AUTOMAP_G(mcount)++]=mp;

	return mp;
}

/*---------------------------------------------------------------*/

static Automap_Mnt_Info *Automap_get_mnt_info(zval * mnt, ulong hash,
											  int exception TSRMLS_DC)
{
	Automap_Mnt_Info *mp;
	int found;

	if (!ZVAL_IS_STRING(mnt)) {
		EXCEPTION_ABORT_RET_1(NULL,"Automap_get_mnt_info: Mount point should be a string (type=%s)",
							  zend_zval_type_name(mnt));
	}

	if (!hash) hash = ZSTRING_HASH(mnt);

	found = (AUTOMAP_G(mcount)
		&& (zend_hash_quick_find(AUTOMAP_G(mtab), Z_STRVAL_P(mnt),
			Z_STRLEN_P(mnt) + 1, hash, (void **) &mp) == SUCCESS));

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
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	retval = (Automap_get_mnt_info(mnt, 0, 0 TSRMLS_CC) != NULL);
	RETVAL_BOOL(retval);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_validate(zval * mnt, ulong hash TSRMLS_DC)
{
	if (!hash)
		hash = ZSTRING_HASH(mnt);
	(void) Automap_get_mnt_info(mnt, hash, 1 TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void Automap::validate(string mnt) */

static PHP_METHOD(Automap, validate)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	(void) Automap_validate(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
// Must accept an invalid mount point without error

static void Automap_umount(zval * mnt, ulong hash TSRMLS_DC)
{
	Automap_Mnt_Info *mp;

	if (!(mp=Automap_get_mnt_info(mnt,0,0 TSRMLS_CC))) return;
	if (--(mp->mnt_count)) return;

	Automap_remove_mnt_info(mnt TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void Automap::umount(string mnt) */

static PHP_METHOD(Automap, umount)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	Automap_umount(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static zval *Automap_instance_by_mp(Automap_Mnt_Info * mp TSRMLS_DC)
{
	if (!mp->instance) {
		ut_new_instance(&(mp->instance), &CZVAL(Automap), 0, 0,
						NULL TSRMLS_CC);
		Automap_set_mp_property(mp->instance, mp TSRMLS_CC);
	}

	return mp->instance;
}

/*---------------------------------------------------------------*/

static zval *Automap_instance(zval * mnt, ulong hash TSRMLS_DC)
{
	Automap_Mnt_Info *mp;

	mp = Automap_get_mnt_info(mnt, hash, 1 TSRMLS_CC);
	if (EG(exception)) return NULL;

	return Automap_instance_by_mp(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto Automap Automap::instance(string mnt) */

static PHP_METHOD(Automap, instance)
{
	zval *mnt, *instance;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	/*DBG_MSG1("Entering Automap::instance(%s)",Z_STRVAL_P(mnt)); */

	instance = Automap_instance(mnt, 0 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(instance);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_mnt_list(zval * ret TSRMLS_DC)
{
	int i;
	Automap_Mnt_Info * mp;

	array_init(ret);

	if (AUTOMAP_G(mcount)) {
		for (i=0;i<AUTOMAP_G(mcount);i++) {
			if ((mp=AUTOMAP_G(mount_order)[i])==NULL) continue;
			add_next_index_stringl(ret,Z_STRVAL_P(mp->mnt)
				,Z_STRLEN_P(mp->mnt),1);
		}
	}
}

/*---------------------------------------------------------------*/
/* {{{ proto array Automap::mnt_list() */

static PHP_METHOD(Automap, mnt_list)
{
	Automap_mnt_list(return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* In case of error, free mem allocated by compute_mnt() */

static void Automap_path_to_mnt(zval * path, zval * mnt TSRMLS_DC)
{
	zval *tmp_mnt;

	Automap_compute_mnt(path, &tmp_mnt TSRMLS_CC);
	if (EG(exception)) return;

	Automap_get_mnt_info(tmp_mnt, 0, 1 TSRMLS_CC);
	if (EG(exception)) {
		zval_ptr_dtor(&tmp_mnt);
		return;
	}

	(*mnt) = (*tmp_mnt);
	zval_copy_ctor(mnt);

	zval_ptr_dtor(&tmp_mnt);
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::path_to_mnt(string path) */

static PHP_METHOD(Automap, path_to_mnt)
{
	zval *path;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	Automap_path_to_mnt(path, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_compute_mnt(zval * path, zval ** mnt TSRMLS_DC)
{
	char *p;
	php_stream_statbuf ssb;
	time_t mti;

	DBG_MSG1("Starting Automap_compute_mnt(%s)",Z_STRVAL_P(path));

	if (php_stream_stat_path(Z_STRVAL_P(path), &ssb) != 0) {
		THROW_EXCEPTION_1("%s: File not found", Z_STRVAL_P(path));
		return;
	}

	DBG_MSG("after stat");

#ifdef NETWARE
	mti = ssb.sb.st_mtime.tv_sec;
#else
	mti = ssb.sb.st_mtime;
#endif

	if (mnt) {
		spprintf(&p, 256, "%lX_%lX_%lX", (unsigned long) (ssb.sb.st_dev)
				 , (unsigned long) (ssb.sb.st_ino), (unsigned long) mti);
		MAKE_STD_ZVAL(*mnt);
		ZVAL_STRING((*mnt), p, 0);
	}

	DBG_MSG("Ending Automap_compute_mnt");
}

/*---------------------------------------------------------------*/

#define INIT_AUTOMAP_MOUNT() \
	{ \
	mp=NULL; \
	}

#define ABORT_AUTOMAP_MOUNT() \
	{ \
	if (mp) Automap_remove_mnt_info(mnt TSRMLS_CC); \
	return NULL; \
	}

static Automap_Mnt_Info *Automap_mount(zval * path, zval * base_dir,
									   zval * mnt, int flags TSRMLS_DC)
{
	Automap_Mnt_Info *mp;
	size_t len;
	ulong hash;
	char *p;

	DBG_MSG("Starting Automap::mount");

	INIT_AUTOMAP_MOUNT();

	if (!ZVAL_IS_STRING(path)) convert_to_string(path);

	if (mnt) {
		SEPARATE_ARG_IF_REF(mnt);
		if (!ZVAL_IS_STRING(mnt)) convert_to_string(mnt);
	} else {
		Automap_compute_mnt(path, &mnt TSRMLS_CC);
		if (EG(exception)) ABORT_AUTOMAP_MOUNT();
	}

	hash = ZSTRING_HASH(mnt);
	if ((mp = Automap_get_mnt_info(mnt, hash, 0 TSRMLS_CC)) != NULL) {
		/* Already mounted */
		DBG_MSG1("%s: Map is already mounted",Z_STRVAL_P(path));
		mp->mnt_count++;
		zval_ptr_dtor(&mnt);
		return mp;
	}

	DBG_MSG("Calling Automap_new_mnt_info");

	mp = Automap_new_mnt_info(mnt, hash TSRMLS_CC);
	if (!mp) {
		zval_ptr_dtor(&mnt);
		ABORT_AUTOMAP_MOUNT();
	}

	ALLOC_INIT_ZVAL(mp->path);
	p=ut_mk_absolute_path(Z_STRVAL_P(path),Z_STRLEN_P(path),&len,0 TSRMLS_CC);
	ZVAL_STRINGL(mp->path, p, len, 0);

	if (base_dir) {
		SEPARATE_ARG_IF_REF(base_dir);
		if (!ZVAL_IS_STRING(base_dir)) convert_to_string(base_dir);
		mp->base_dir = base_dir;
	} else {
		DBG_MSG("Computing base dir");
		p=ut_absolute_dirname(Z_STRVAL_P(path),Z_STRLEN_P(path),&len,1 TSRMLS_CC);
		MAKE_STD_ZVAL(mp->base_dir);
		ZVAL_STRINGL(mp->base_dir, p, len, 0);
	}

	MAKE_STD_ZVAL(mp->flags);
	ZVAL_LONG(mp->flags, flags);

	Automap_populate_persistent_data(mp TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_MOUNT();

	DBG_MSG("Ending Automap::mount");

	return mp;
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::mount(string path [, string base_dir [, string mnt [, int flags]]]) */

static PHP_METHOD(Automap, mount)
{
	zval *path, *base_dir, *mnt;
	int flags;
	Automap_Mnt_Info *mp;

	base_dir = mnt = NULL;
	flags = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|z!z!l", &path
		, &base_dir, &mnt, &flags) == FAILURE)
			EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	mp = Automap_mount(path, base_dir, mnt, flags TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(mp->mnt);
}

/* }}} */
/*---------------------------------------------------------------*/

static Automap_Mnt_Persistent_Data *Automap_get_persistent_data(
	zval * mnt, ulong hash TSRMLS_DC)
{
	Automap_Mnt_Persistent_Data *pmp;

	if (!ZVAL_IS_STRING(mnt)) {
		EXCEPTION_ABORT_RET_1(NULL,"Automap_get_persistent_data: Mount point should be a string (type=%s)",
							  zend_zval_type_name(mnt));
	}

	if (!hash) hash = ZSTRING_HASH(mnt);

	if (zend_hash_quick_find(&persistent_mtab, Z_STRVAL_P(mnt),
			Z_STRLEN_P(mnt) + 1, hash, (void **) &pmp) != SUCCESS) return NULL;

	return pmp;
}

/*---------------------------------------------------------------*/

#define INIT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	MutexLock(persistent_mtab); \
	INIT_ZVAL(zbuf); \
	INIT_ZVAL(zdata); \
	}

#define CLEANUP_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	zval_dtor(&zbuf); \
	zval_dtor(&zdata); \
	}

#define RETURN_FROM_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA(_ret) \
	{ \
	MutexUnlock(persistent_mtab); \
	return _ret; \
	}

#define ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	CLEANUP_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA(); \
	RETURN_FROM_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA(NULL); \
	}

static Automap_Mnt_Persistent_Data *Automap_get_or_create_persistent_data(
	Automap_Mnt_Info * mp TSRMLS_DC)
{
	Automap_Mnt_Persistent_Data tmp_entry, *entry;
	zval version, **zpp, zbuf, zdata;
	char *buf, tmpc;
	unsigned long fsize, blen;

	DBG_MSG1("Entering Automap_new_persistent_data(%s)",Z_STRVAL_P(mp->mnt));

	INIT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();

	entry=Automap_get_persistent_data(mp->mnt, mp->hash TSRMLS_CC);
	if (entry) {
		entry->nb_hits++;
		entry->refcount++;
		MutexUnlock(persistent_mtab);
		return entry;
	}

	/* Create entry - slow path */

	ut_file_get_contents(Z_STRVAL_P(mp->path), &zbuf TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	buf = Z_STRVAL(zbuf);
	blen = Z_STRLEN(zbuf);

	/* File cannot be smaller than 54 bytes - Secure future memory access */

	if (blen < 54) {
		THROW_EXCEPTION_1("%s : file is too small", Z_STRVAL_P(mp->path));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}

	/* Check magic string */

	if (memcmp(buf, AUTOMAP_MAGIC, sizeof(AUTOMAP_MAGIC) - 1)) {
		THROW_EXCEPTION_1("%s : Bad magic", Z_STRVAL_P(mp->path));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}

	/* Check minimum required runtime version */

	tmpc = buf[28];
	buf[28] = '\0';
	if (php_version_compare(&(buf[16]), AUTOMAP_API) > 0) {
		THROW_EXCEPTION_2
			("%s : Cannot understand this map. Requires at least Automap version %s",
			 Z_STRVAL_P(mp->path), &(buf[16]));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}
	buf[28] = tmpc;

	/* Check file size */

	tmpc = buf[53];
	buf[53] = '\0';
	fsize = 0;
	(void) sscanf(&(buf[45]), "%lu", &fsize);
	if (fsize != blen) {
		THROW_EXCEPTION_2("%s : Invalid file size. Should be %lu",
						  Z_STRVAL_P(mp->path), fsize);
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}
	buf[53] = tmpc;

	/* Create slot */

	zend_hash_quick_update(&persistent_mtab, Z_STRVAL_P(mp->mnt)
						   , Z_STRLEN_P(mp->mnt) + 1, mp->hash, &tmp_entry,
						   sizeof(tmp_entry), (void **) &entry);

	entry->ctime = time(NULL);

	entry->nb_hits=entry->refcount=1;

	/* Record minimum version (to be able to display it) */

	INIT_ZVAL(version);
	ZVAL_STRINGL(&version, &(buf[16]), 12, 1);
	Z_STRLEN(version)=ut_rtrim(Z_STRVAL(version) TSRMLS_CC);
	ut_persist_zval(&version, &(entry->min_version));
	zval_dtor(&version);

	/* Get map version */

	INIT_ZVAL(version);
	ZVAL_STRINGL(&version, &(buf[30]), 12, 1);
	Z_STRLEN(version)=ut_rtrim(Z_STRVAL(version) TSRMLS_CC);
	ut_persist_zval(&version, &(entry->version));
	zval_dtor(&version);

	/* Get the rest as an unserialized array */

	ut_unserialize_zval(buf + 53, blen - 53, &zdata TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();

	if (!ZVAL_IS_ARRAY(&zdata)) {
		THROW_EXCEPTION_1("%s : Map file should contain an array",
						  Z_STRVAL_P(mp->path));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}

	/* Get the symbol table */

	if (FIND_HKEY(Z_ARRVAL(zdata), map, &zpp) != SUCCESS) {
		THROW_EXCEPTION_1("%s : No symbol table", Z_STRVAL_P(mp->path));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}
	if (!ZVAL_IS_ARRAY(*zpp)) {
		THROW_EXCEPTION_1("%s : Symbol table should contain an array",
						  Z_STRVAL_P(mp->path));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}
	ut_persist_zval(*zpp, &(entry->symbols));

	/* Get the options array */

	if (FIND_HKEY(Z_ARRVAL(zdata), options, &zpp) != SUCCESS) {
		THROW_EXCEPTION_1("%s : No options array", Z_STRVAL_P(mp->path));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}
	if (!ZVAL_IS_ARRAY(*zpp)) {
		THROW_EXCEPTION_1("%s : Options should be an array",
						  Z_STRVAL_P(mp->path));
		ABORT_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();
	}
	ut_persist_zval(*zpp, &(entry->options));

	/* Cleanup and return */

	CLEANUP_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA();

	RETURN_FROM_AUTOMAP_GET_OR_CREATE_PERSISTENT_DATA(entry);
}

/*---------------------------------------------------------------*/

static void Automap_populate_persistent_data(Automap_Mnt_Info *mp TSRMLS_DC)
{
	Automap_Mnt_Persistent_Data *entry;

	DBG_MSG1("Entering Automap_populate_persistent_data(%s)",
			 Z_STRVAL_P(mp->mnt));

	entry = Automap_get_or_create_persistent_data(mp TSRMLS_CC);
	if (EG(exception)) return;

	/* Populate mp structure */

	mp->refcountp = &(entry->refcount);

	mp->min_version = &(entry->min_version);
	ZVAL_ADDREF(&(entry->min_version));

	mp->version = &(entry->version);
	ZVAL_ADDREF(&(entry->version));

	mp->options = &(entry->options);
	ZVAL_ADDREF(&(entry->options));

	mp->symbols = &(entry->symbols);
	ZVAL_ADDREF(&(entry->symbols));
}

/*---------------------------------------------------------------*/
/* zval_dtor works for persistent arrays, but not for persistent strings */
/* TODO: Generic persistent zval API */

static void Automap_Persistent_Data_dtor(Automap_Mnt_Persistent_Data *entry)
{
	ut_persistent_zval_dtor(&(entry->min_version));
	ut_persistent_zval_dtor(&(entry->version));
	ut_persistent_zval_dtor(&(entry->options));
	ut_persistent_zval_dtor(&(entry->symbols));
}

/*---------------------------------------------------------------*/

static char *Automap_get_type_string(char type TSRMLS_DC)
{
	automap_type_string *sp;

	for (sp=automap_type_strings;sp->type;sp++) {
		if (sp->type == type) return sp->string;
	}

	THROW_EXCEPTION_1("%c : Invalid type", type);
	return NULL;
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::get_type_string(string type) */

static PHP_METHOD(Automap, get_type_string)
{
	char *type,*p;
	int tlen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &type,&tlen) ==
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	if (!(p=Automap_get_type_string(*type TSRMLS_CC))) return;
	RETURN_STRING(p,1);
}

/* }}} */
/*---------------------------------------------------------------*/

static char Automap_string_to_type(char *string TSRMLS_DC)
{
	automap_type_string *sp;

	for (sp=automap_type_strings;sp->type;sp++) {
		if (!strcmp(sp->string,string)) return sp->type;
	}

	THROW_EXCEPTION_1("%s : Invalid type", string);
	return '\0';
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::string_to_type(string str) */

static PHP_METHOD(Automap, string_to_type)
{
	char *string,c[2];
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &string,&slen) ==
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	if (!(c[0]=Automap_string_to_type(string TSRMLS_CC))) return;
	c[1]='\0';
	RETURN_STRINGL(c,1,1);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::register_failure_handler(callable callback) */

static PHP_METHOD(Automap, register_failure_handler)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp)
		==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	EALLOCATE(AUTOMAP_G(failure_handlers),(AUTOMAP_G(fh_count)+1)*sizeof(zp));
	AUTOMAP_G(failure_handlers)[AUTOMAP_G(fh_count)++]=zp;
	ZVAL_ADDREF(zp);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::register_success_handler(callable callback) */

static PHP_METHOD(Automap, register_success_handler)
{
	zval *zp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zp)
		==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	EALLOCATE(AUTOMAP_G(success_handlers),(AUTOMAP_G(sh_count)+1)*sizeof(zp));
	AUTOMAP_G(success_handlers)[AUTOMAP_G(sh_count)++]=zp;
	ZVAL_ADDREF(zp);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_key(char type, char *symbol, unsigned long len
	, zval *ret TSRMLS_DC)
{
	char *p;

	p=eallocate(NULL,len+2);
	p[0]=type;

	memmove(p+1,symbol,len+1);

	if ((type == AUTOMAP_T_EXTENSION)
		|| (type == AUTOMAP_T_FUNCTION)
		|| (type == AUTOMAP_T_CLASS)) ut_tolower(p+1,len TSRMLS_CC);

	ZVAL_STRINGL(ret,p,len+1,0);
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::key(string type, string symbol) */

static PHP_METHOD(Automap, key)
{
	char *type,*symbol;
	int tlen,slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ss", &type,&tlen
		,&symbol,&slen)==FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters",0);

	Automap_key(*type,symbol,slen,return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_get_type_from_key(char *key, zval *ret TSRMLS_DC)
{
	if (!(*key)) EXCEPTION_ABORT("Invalid key");

	ZVAL_STRINGL(ret,key,1,1);
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::get_type_from_key(string key) */

static PHP_METHOD(Automap, get_type_from_key)
{
	char *key;
	int klen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &key, &klen
		)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	Automap_get_type_from_key(key, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Removes the 1st char and anything following a '|' char. */

static void Automap_get_symbol_from_key(char *key, zval *ret TSRMLS_DC)
{
	char *p;
	int slen;

	if (!(*key)) EXCEPTION_ABORT("Invalid key");

	for (slen=0,p=key+1;(*p) && ((*p) != '|');p++,slen++);

	ZVAL_STRINGL(ret,key+1,slen,1);
}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::get_symbol_from_key(string key) */

static PHP_METHOD(Automap, get_symbol_from_key)
{
	char *key;
	int klen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &key, &klen
		)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	Automap_get_symbol_from_key(key, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static int Automap_symbol_is_defined(char type, char *symbol
	, unsigned int slen TSRMLS_DC)
{
	char *p;
	int status;
	zval dummy_zp;

	if (!slen) return 0;

	if (type==AUTOMAP_T_CONSTANT) p=NULL;
	else p=zend_str_tolower_dup(symbol,slen);

	status=0;
	switch(type) {
		case AUTOMAP_T_CONSTANT:
			status=zend_get_constant(symbol,slen,&dummy_zp TSRMLS_CC);
			zval_dtor(&dummy_zp);
			break;

		case AUTOMAP_T_FUNCTION:
			status=zend_hash_exists(EG(function_table),p,slen+1);
			break;

		case AUTOMAP_T_CLASS:
			status=zend_hash_exists(EG(class_table),p,slen+1);
			break;

		case AUTOMAP_T_EXTENSION:
			status=ut_extension_loaded(p,slen TSRMLS_CC);
			break;


	}

	if (p) efree(p);

	return status;
}

/*---------------------------------------------------------------*/
/* {{{ proto void Automap::autoload_hook(string symbol [, string type ]) */

static PHP_METHOD(Automap, autoload_hook)
{
	char *symbol,*type;
	int slen,tlen;

	type=NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s|s",&symbol, &slen
		,&type,&tlen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	DBG_MSG1("Starting Automap::autoload_hook(%s)",symbol);

	if (!Automap_get_symbol((type ? (*type) : AUTOMAP_T_CLASS), symbol
		, slen, 1, 0 TSRMLS_CC)) {
/*		TODO : call_failure_handlers */
	}	

	DBG_MSG("Ending Automap::autoload_hook");
}

/* }}} */
/*---------------------------------------------------------------*/

static int Automap_get_symbol(char type, char *symbol, int slen, int autoload
	, int exception TSRMLS_DC)
{
	zval *key;
	unsigned long hash;
	char *ts;
	int i;
	Automap_Mnt_Info *mp;

	DBG_MSG2("Starting Automap_get_symbol(%c,%s)",type,symbol);

	if ((!autoload) && Automap_symbol_is_defined(type,symbol,slen TSRMLS_CC)) {
		return 1;
	}

	if ((i=AUTOMAP_G(mcount))==0) return 0;

	MAKE_STD_ZVAL(key);
	Automap_key(type,symbol,slen,key TSRMLS_CC);

	hash=ZSTRING_HASH(key);

	while ((--i) >= 0) {
		mp=AUTOMAP_G(mount_order)[i];
		if (!mp) continue;
		if (Automap_resolve_key(key, hash, mp TSRMLS_CC)) {
			zval_ptr_dtor(&key);
			return 1;
		}
	}

	Automap_call_failure_handlers(key TSRMLS_CC);

	if (exception) {
		ts=Automap_get_type_string(type TSRMLS_CC);
		if (!EG(exception))
			THROW_EXCEPTION_2("Automap: Unknown %s: %s",ts,symbol);
	}

	zval_ptr_dtor(&key);
	return 0;
}

/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::get_function(string name) */

static PHP_METHOD(Automap, get_function)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_FUNCTION, symbol, slen
		, 0, 0 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::get_constant(string name) */

static PHP_METHOD(Automap, get_constant)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_CONSTANT, symbol, slen
		, 0, 0 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::get_class(string name) */

static PHP_METHOD(Automap, get_class)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_CLASS, symbol, slen
		, 0, 0 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::get_extension(string name) */

static PHP_METHOD(Automap, get_extension)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_EXTENSION, symbol, slen
		, 0, 0 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::require_function(string name) */

static PHP_METHOD(Automap, require_function)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_FUNCTION, symbol, slen
		, 0, 1 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::require_constant(string name) */

static PHP_METHOD(Automap, require_constant)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_CONSTANT, symbol, slen
		, 0, 1 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::require_class(string name) */

static PHP_METHOD(Automap, require_class)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_CLASS, symbol, slen
		, 0, 1 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto boolean Automap::require_extension(string name) */

static PHP_METHOD(Automap, require_extension)
{
	char *symbol;
	int slen;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol
		, &slen)==FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters",0);

	RETURN_BOOL(Automap_get_symbol(AUTOMAP_T_EXTENSION, symbol, slen
		, 0, 1 TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* Instance methods. The instance object is returned by Automap::instance() */

/*---------------------------------------------------------------*/

#define AUTOMAP_GET_PROPERTY_METHOD(_field) \
	static PHP_METHOD(Automap, _field) \
	{ \
		AUTOMAP_GET_INSTANCE_DATA(_field) \
		if (mp->_field) RETVAL_BY_REF(mp->_field); \
	}

/*---------------------------------------------------------------*/

/* {{{ proto string Automap::mnt() */
AUTOMAP_GET_PROPERTY_METHOD(mnt)
/* }}} */

/* {{{ proto string Automap::path() */
AUTOMAP_GET_PROPERTY_METHOD(path)
/* }}} */

/* {{{ proto string Automap::base_dir() */
AUTOMAP_GET_PROPERTY_METHOD(base_dir)
/* }}} */

/* {{{ proto integer Automap::flags() */
AUTOMAP_GET_PROPERTY_METHOD(flags)
/* }}} */

/* {{{ proto array Automap::symbols() */
AUTOMAP_GET_PROPERTY_METHOD(symbols)
/* }}} */

/* {{{ proto array Automap::options() */
AUTOMAP_GET_PROPERTY_METHOD(options)
/* }}} */

/* {{{ proto string Automap::version() */
AUTOMAP_GET_PROPERTY_METHOD(version)
/* }}} */

/* {{{ proto string Automap::min_version() */
AUTOMAP_GET_PROPERTY_METHOD(min_version)
/* }}} */

/*---------------------------------------------------------------*/

static unsigned long Automap_symbol_count(Automap_Mnt_Info *mp TSRMLS_DC)
{
	return zend_hash_num_elements(Z_ARRVAL_P(mp->symbols));
}

/*---------------------------------------------------------------*/
/* {{{ proto integer Automap::symbol_count() */

static PHP_METHOD(Automap, symbol_count)
{
	AUTOMAP_GET_INSTANCE_DATA(symbol_count)

	RETURN_LONG(Automap_symbol_count(mp TSRMLS_CC));
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto mixed Automap::option(string name) */

static PHP_METHOD(Automap, option)
{
	char *opt;
	int optlen;
	zval **zpp;

	AUTOMAP_GET_INSTANCE_DATA(option)

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &opt, &optlen) ==
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	DBG_MSG1("Entering AUTOMAP::option(%s)", opt);

	if (zend_hash_find(Z_ARRVAL_P(mp->options), opt, optlen + 1,
		(void **)(&zpp)) == SUCCESS) {
		RETVAL_BY_REF(*zpp);
	}
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_call_failure_handlers(zval *key TSRMLS_DC)
{
	int i;

	if (AUTOMAP_G(fh_count)) {
		for (i=0;i<AUTOMAP_G(fh_count);i++) {
			ut_call_user_function_void(NULL,AUTOMAP_G(failure_handlers)[i],1
				,&key TSRMLS_CC);
		}
	}
}

/*---------------------------------------------------------------*/

static void Automap_call_success_handlers(zval *key, zval *value
	, Automap_Mnt_Info *mp TSRMLS_DC)
{
	zval *args[3];
	int i;

	if (AUTOMAP_G(sh_count)) {
		args[0]=key;
		args[1]=mp->mnt;
		args[2]=value;
		for (i=0;i<AUTOMAP_G(sh_count);i++) {
			ut_call_user_function_void(NULL,AUTOMAP_G(success_handlers)[i],3
				,args TSRMLS_CC);
		}
	}
}

/*---------------------------------------------------------------*/

static int Automap_resolve_key(zval *key, unsigned long hash
	, Automap_Mnt_Info *mp TSRMLS_DC)
{
	zval **value,zv;
	char *symbol,type,*req_str;
	int slen;
	int old_error_reporting;
	Automap_Mnt_Info *sub_mp;

	if (zend_hash_quick_find(Z_ARRVAL_P(mp->symbols),Z_STRVAL_P(key)
		,Z_STRLEN_P(key)+1,hash,(void **)(&value)) != SUCCESS) return 0;

	symbol=Z_STRVAL_PP(value)+1;
	slen=Z_STRLEN_PP(value)-1;
	type=*Z_STRVAL_PP(value);

	if (type == AUTOMAP_F_EXTENSION) {
		ut_load_extension(symbol,slen TSRMLS_CC);
		if (EG(exception)) return 0;
		Automap_call_success_handlers(key,*value,mp TSRMLS_CC);
		return 1;
	}

	/* Compute "require '$mp->base_dir.$symbol';" */

	spprintf(&req_str,1024,"require '%s%s';",Z_STRVAL_P(mp->base_dir),symbol);

	if (type == AUTOMAP_F_SCRIPT) {
		DBG_MSG1("eval : %s",req_str);
		zend_eval_string(req_str,NULL,req_str TSRMLS_CC);
		DBG_MSG("After eval");
		EALLOCATE(req_str,0);
		Automap_call_success_handlers(key,*value,mp TSRMLS_CC);
		return 1;
	}

	if (type == AUTOMAP_F_PACKAGE) { /* Symbol is in a sub-package */

		/* Remove E_NOTICE messages if the test script is a package - workaround */
		/* to PHP bug #39903 ('__COMPILER_HALT_OFFSET__ already defined') */

		old_error_reporting = EG(error_reporting);
		EG(error_reporting) &= ~E_NOTICE;

		INIT_ZVAL(zv);
		zend_eval_string(req_str,&zv,req_str TSRMLS_CC);
		EALLOCATE(req_str,0);

		EG(error_reporting) = old_error_reporting;

		if (!ZVAL_IS_STRING(&zv)) {
			THROW_EXCEPTION_1("%s : Package inclusion should return a string"
				,req_str);
			zval_dtor(&zv);
			return 0;
		}

		sub_mp=Automap_get_mnt_info(&zv, 0, 1 TSRMLS_CC);
		zval_dtor(&zv);
		if (EG(exception)) return 0; /* If no corresponding mounted map, */
									 /* forward the exception (shouldn't happen) */
		return Automap_resolve_key(key, hash, sub_mp TSRMLS_CC);
	}

	return 0; /* Unknown value type. Shouldn't happen in valid maps */
}

/*---------------------------------------------------------------*/

static void Automap_html_show(Automap_Mnt_Info *mp
	, zval *subfile_to_url_function TSRMLS_DC)
{
	char *keyp,*p,*ktype,*kname,ftype,*fname;
	zval zv,*zp,zret;
	int keylen;
	zval **zpp;
	HashPosition pos;
	HashTable *arrht;
	ulong dummy_idx;

	php_printf("<h2>Global information</h2>\n<table border=0>\n");

	p=ut_htmlspecialchars(Z_STRVAL_P(mp->version), Z_STRLEN_P(mp->version)
		,NULL TSRMLS_CC);
	php_printf("<tr><td>Map version:&nbsp;</td><td>%s</td></tr>\n",p);
	EALLOCATE(p,0);

	p=ut_htmlspecialchars(Z_STRVAL_P(mp->min_version)
		, Z_STRLEN_P(mp->min_version), NULL TSRMLS_CC);
	php_printf("<tr><td>Min reader version:&nbsp;</td><td>%s</td></tr>\n",p);
	EALLOCATE(p,0);

	php_printf("<tr><td>Symbol count:&nbsp;</td><td>%lu</td></tr>\n"
		,Automap_symbol_count(mp TSRMLS_CC));

	php_printf("</table>\n");

	php_start_ob_buffer (NULL, 0, 1 TSRMLS_CC);
	zend_print_zval_r(mp->options, 0 TSRMLS_CC);
	php_ob_get_buffer (&zv TSRMLS_CC);
	php_end_ob_buffer (0, 0 TSRMLS_CC);
	p=ut_htmlspecialchars(Z_STRVAL(zv), Z_STRLEN(zv), NULL TSRMLS_CC);
	php_printf("<h2>Options</h2>\n<pre>%s</pre>\n",p);
	zval_dtor(&zv);
	EALLOCATE(p,0);

	php_printf("<h2>Symbols</h2>\n<table border=1 bordercolor=\"#BBBBBB\" \
cellpadding=3 cellspacing=0 style=\"border-collapse: collapse\">\n\
<tr><th>Type</th><th>Name</th><th>FT</th><th>Defined in</th></tr>\n");

	arrht=Z_ARRVAL_P(mp->symbols);
	zend_hash_internal_pointer_reset_ex(arrht, &pos);
	while (zend_hash_get_current_key_ex(arrht, &keyp, &keylen, &dummy_idx, 0,
				&pos) != HASH_KEY_NON_EXISTANT) {
		zend_hash_get_current_data_ex(arrht,(void **)(&zpp),&pos);
		if (ZVAL_IS_STRING(*zpp)) { /* Security */
			p=Automap_get_type_string(*keyp TSRMLS_CC);
			if (EG(exception)) return;
			ktype=ut_ucfirst(p, strlen(p) TSRMLS_CC);
			kname=ut_htmlspecialchars(keyp+1,keylen-2,NULL TSRMLS_CC);
			ftype=*Z_STRVAL_PP(zpp);
			fname=ut_htmlspecialchars(Z_STRVAL_PP(zpp)+1,Z_STRLEN_PP(zpp)-1
				,NULL TSRMLS_CC);

			php_printf("<tr><td>%s</td><td>%s</td><td align=center>%c</td><td>"
				,ktype,kname,ftype);

			if (subfile_to_url_function
				&& (!ZVAL_IS_NULL(subfile_to_url_function))) {
				INIT_ZVAL(zv);
				ZVAL_STRINGL(&zv,Z_STRVAL_PP(zpp)+1,Z_STRLEN_PP(zpp)-1,0);
				zp = &zv;
				INIT_ZVAL(zret);
				ut_call_user_function_string(NULL,subfile_to_url_function,&zret
					,1,&zp TSRMLS_CC);
				if (ZVAL_IS_STRING(&zret)) {
					php_printf("<a href=\"%s\">%s</a>",Z_STRVAL(zret),fname);
				} else php_printf(fname);
				zval_dtor(&zret);
			} else php_printf(fname);

			php_printf("</td></tr>\n");

			EALLOCATE(fname,0);
			EALLOCATE(kname,0);
			EALLOCATE(ktype,0);

		}
		zend_hash_move_forward_ex(arrht, &pos);
	}

	php_printf("</table>\n");
}

/*---------------------------------------------------------------*/
/* {{{ proto void Automap::show([ callable subfile_to_url_function ]) */

static PHP_METHOD(Automap, show)
{
	zval *subfile_to_url_function;
	char *keyp,*p,*ktype,*kname,ftype,*fname;
	int len,keylen,ktype_len,kname_len,fname_len;
	zval **zpp;
	HashPosition pos;
	HashTable *arrht;
	ulong dummy_idx;

	AUTOMAP_GET_INSTANCE_DATA(show)

	subfile_to_url_function=NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z"
		, &subfile_to_url_function) == FAILURE)
			EXCEPTION_ABORT_1("Cannot parse parameters", 0);
	
	if (ut_is_web()) {
		Automap_html_show(mp, subfile_to_url_function TSRMLS_CC);
		return;
	}

	php_printf("\n* Global information :\n\n");
	php_printf("	Map version : %s\n",Z_STRVAL_P(mp->version));
	php_printf("	Min reader version : %s\n",Z_STRVAL_P(mp->min_version));
	php_printf("	Symbol count : %lu\n",Automap_symbol_count(mp TSRMLS_CC));

	php_printf("\n* Options :\n\n");
	zend_print_zval_r(mp->options, 0 TSRMLS_CC);

	php_printf("\n* Symbols :\n\n");

	/* Compute column sizes */

	ktype_len=kname_len=4;
	fname_len=10;
	arrht=Z_ARRVAL_P(mp->symbols);
	zend_hash_internal_pointer_reset_ex(arrht, &pos);
	while (zend_hash_get_current_key_ex(arrht, &keyp, &keylen, &dummy_idx, 0,
				&pos) != HASH_KEY_NON_EXISTANT) {
		zend_hash_get_current_data_ex(arrht,(void **)(&zpp),&pos);
		if (ZVAL_IS_STRING(*zpp)) { /* Security */
			ktype=Automap_get_type_string(*keyp TSRMLS_CC);
			if (EG(exception)) return;
			len=strlen(ktype);
			ktype_len=MAX(ktype_len,len+2);
			kname_len=MAX(kname_len,keylen); /* + 2 - 1 - 1 */
			fname_len=MAX(fname_len,Z_STRLEN_PP(zpp)+1);
		}
		zend_hash_move_forward_ex(arrht, &pos);
	}

	ut_repeat_printf('-',ktype_len+kname_len+fname_len+8 TSRMLS_CC);
	php_printf("\n|");
	ut_printf_pad_both("Type",4,ktype_len TSRMLS_CC);
	php_printf("|");
	ut_printf_pad_both("Name",4,kname_len TSRMLS_CC);
	php_printf("| T |");
	ut_printf_pad_both("Defined in",10,fname_len TSRMLS_CC);
	php_printf("|\n|");
	ut_repeat_printf('-',ktype_len TSRMLS_CC);
	php_printf("|");
	ut_repeat_printf('-',kname_len TSRMLS_CC);
	php_printf("|---|");
	ut_repeat_printf('-',fname_len TSRMLS_CC);
	php_printf("|\n");

	zend_hash_internal_pointer_reset_ex(arrht, &pos);
	while (zend_hash_get_current_key_ex(arrht, &keyp, &keylen, &dummy_idx, 0,
				&pos) != HASH_KEY_NON_EXISTANT) {
		zend_hash_get_current_data_ex(arrht,(void **)(&zpp),&pos);
		if (ZVAL_IS_STRING(*zpp)) { /* Security */
			p=Automap_get_type_string(*keyp TSRMLS_CC);
			if (EG(exception)) return;
			len=strlen(p);
			ktype=ut_ucfirst(p, len TSRMLS_CC);
			kname=keyp+1;
			ftype=*Z_STRVAL_PP(zpp);
			fname=Z_STRVAL_PP(zpp)+1;

			PHPWRITE("| ",2);
			ut_printf_pad_right(ktype,len,ktype_len-1 TSRMLS_CC);
			PHPWRITE("| ",2);
			ut_printf_pad_right(kname,keylen-2,kname_len-1 TSRMLS_CC);
			php_printf("| %c | ",ftype);
			ut_printf_pad_right(fname,Z_STRLEN_PP(zpp)-1,fname_len-1 TSRMLS_CC);
			PHPWRITE("|\n",2);
			
			EALLOCATE(ktype,0);
		}
		zend_hash_move_forward_ex(arrht, &pos);
	}
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::export([ string path ]) */

static PHP_METHOD(Automap, export)
{
	char *path,*p,*keyp;
	int plen,keylen;
	zval **zpp;
	php_stream *stream;
	HashPosition pos;
	HashTable *arrht;
	ulong dummy_idx;

	AUTOMAP_GET_INSTANCE_DATA(export)

	path=NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|s", &path, &plen) ==
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	p = (path && (*path)) ? path : "php://stdout";
	stream=php_stream_open_wrapper(p,"wb",ENFORCE_SAFE_MODE|REPORT_ERRORS,NULL);
	if (!stream) EXCEPTION_ABORT_1("%s: Cannot open for writing",p);

	arrht=Z_ARRVAL_P(mp->symbols);
	zend_hash_internal_pointer_reset_ex(arrht, &pos);
	while (zend_hash_get_current_key_ex(arrht, &keyp, &keylen, &dummy_idx, 0,
				&pos) != HASH_KEY_NON_EXISTANT) {
		zend_hash_get_current_data_ex(arrht,(void **)(&zpp),&pos);
		if (ZVAL_IS_STRING(*zpp)) { /* Security */
			php_stream_write(stream,keyp,keylen-1);
			php_stream_write(stream," ",1);
			php_stream_write(stream,Z_STRVAL_PP(zpp),Z_STRLEN_PP(zpp));
			php_stream_write(stream,"\n",1);
		}
		zend_hash_move_forward_ex(arrht, &pos);
	}

	php_stream_close(stream);
}

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_set_mp_property(zval * obj, Automap_Mnt_Info * mp TSRMLS_DC)
{
	add_property_stringl_ex(obj, "m", 2, (char *)(&mp), sizeof(mp),1 TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* Run  spl_autoload_register('Automap::autoload_hook'); */

static void Automap_register_autoload_hook(TSRMLS_D)
{
	zval *zp;

	MAKE_STD_ZVAL(zp);
	ZVAL_STRINGL(zp,"Automap::autoload_hook"
		,sizeof("Automap::autoload_hook")-1,1);

	ut_call_user_function_void(NULL,&CZVAL(spl_autoload_register),1
		,&zp TSRMLS_CC);
	zval_ptr_dtor(&zp); /* This way, spl_autoload_register can keep the zval */
}

/*---------------------------------------------------------------*/

static ZEND_BEGIN_ARG_INFO_EX(Automap_mount_arginfo, 0, 1, 1)
ZEND_ARG_INFO(0, path)
ZEND_ARG_INFO(0, base_dir)
ZEND_ARG_INFO(0, mnt)
ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

static ZEND_BEGIN_ARG_INFO_EX(Automap_autoload_hook_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, symbol)
ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

static zend_function_entry Automap_functions[] = {
	PHP_ME(Automap, is_mounted, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, validate, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, umount, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, instance, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, mnt_list, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, path_to_mnt, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, mount, Automap_mount_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, register_failure_handler, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, register_success_handler, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_type_string, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, string_to_type, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, key, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_type_from_key, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_symbol_from_key, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, autoload_hook, Automap_autoload_hook_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_function, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_constant, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_class, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_extension, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, require_function, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, require_constant, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, require_class, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, require_extension, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, mnt, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, path, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, base_dir, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, flags, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, symbols, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, options, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, version, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, min_version, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, symbol_count, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, option, UT_1arg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, show, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, export, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/

#define AUTOMAP_DECLARE_TYPE_CONSTANT(name) \
	{ \
	char *p=NULL; \
	zval *zp; \
	\
	UT_NEW_PZP(); \
	PALLOCATE(p,2); \
	p[0]=AUTOMAP_ ## name; \
	p[1]='\0'; \
	ZVAL_STRINGL(zp,p,1,0); \
	UT_ADD_PZP_CONST(ce, #name); \
	}

static void set_constants(zend_class_entry * ce)
{
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_FUNCTION,"T_FUNCTION");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_CONSTANT,"T_CONSTANT");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_CLASS,"T_CLASS");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_EXTENSION,"T_EXTENSION");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_F_SCRIPT,"F_SCRIPT");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_F_EXTENSION,"F_EXTENSION");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_F_PACKAGE,"F_PACKAGE");

	UT_DECLARE_STRING_CONSTANT(AUTOMAP_MAGIC,"MAGIC");
}

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_Automap(TSRMLS_D)
{
	zend_class_entry ce, *entry;

	/*--- Init persistent data */

	Automap_init_persistent_data(TSRMLS_C);

	/*--- Check that SPL is present */

	if (!ut_extension_loaded("spl",3 TSRMLS_CC)) {
		THROW_EXCEPTION("Automap requires the SPL extension");
		return FAILURE;
	}

	/*--- Init class and set constants*/

	INIT_CLASS_ENTRY(ce, "Automap", Automap_functions);
	entry = zend_register_internal_class(&ce TSRMLS_CC);
	set_constants(entry);

	return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Module shutdown                                               */

static int MSHUTDOWN_Automap(TSRMLS_D)
{
	Automap_shutdown_persistent_data(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap(TSRMLS_D)
{
	int i;

	if (AUTOMAP_G(mcount)) {
		zend_hash_destroy(AUTOMAP_G(mtab));
		EALLOCATE(AUTOMAP_G(mtab),0);

		EALLOCATE(AUTOMAP_G(mount_order),0);

		AUTOMAP_G(mcount)=0;
	}

	if (AUTOMAP_G(fh_count)) {
		for (i=0;i<AUTOMAP_G(fh_count);i++) {
			zval_ptr_dtor(AUTOMAP_G(failure_handlers)+i);
		}
		EALLOCATE(AUTOMAP_G(failure_handlers),0);
		AUTOMAP_G(fh_count)=0;
	}

	if (AUTOMAP_G(sh_count)) {
		for (i=0;i<AUTOMAP_G(sh_count);i++) {
			zval_ptr_dtor(AUTOMAP_G(success_handlers)+i);
		}
		EALLOCATE(AUTOMAP_G(success_handlers),0);
		AUTOMAP_G(sh_count)=0;
	}

	return SUCCESS;
}

/*---------------------------------------------------------------*/
