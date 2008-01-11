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

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_version.h"
#include "php_streams.h"
#include "zend_objects_API.h"
#include "ext/standard/php_versioning.h"

#include "php_phk.h"
#include "utils.h"
#include "PHK.h"
#include "PHK_Mgr.h"
#include "phk_stream_parse_uri2.h"

ZEND_EXTERN_MODULE_GLOBALS(phk)

/*---------------------------------------------------------------*/

typedef struct {				/* Private */
	unsigned long nb_hits;
	unsigned long refcount;
	time_t ctime;

	zval min_version;			/* String */
	zval options;				/* Array */
	zval build_info;			/* Array */

	/* Shortcuts */

	int crc_check;				/* Bool */
	int no_cache;				/* Bool */
	int no_opcode_cache;		/* Bool */
	int web_main_redirect;		/* Bool */
	int auto_umount;			/* Bool */
	zval *mime_types;			/* Array zval or null */
	zval *web_run_script;		/* String zval or null */
	zval *plugin_class;			/* String zval or null */
	zval *web_access;			/* String|Array zval or null */
	zval *min_php_version;		/* String zval or null */
	zval *max_php_version;		/* String zval or null */

	/* Pre-computed constant values */

	zval base_uri;				/* String zval */
	zval automap_uri;			/* String zval or znull if map not defined */
	zval mount_script_uri;		/* String zval or znull if not defined */
	zval umount_script_uri;		/* String zval or znull if not defined */
	zval lib_run_script_uri;	/* String zval or znull if not defined */
	zval cli_run_command;		/* String zval or znull if not defined */
} PHK_Mnt_Persistent_Data;

static HashTable persistent_mtab;

static int tmp_mnt_num;

MutexDeclare(persistent_mtab);

/*---------------------------------------------------------------*/

static void PHK_Mgr_init_persistent_data(TSRMLS_D);
static void PHK_Mgr_shutdown_persistent_data(TSRMLS_D);
static void PHK_Mgr_mnt_info_dtor(PHK_Mnt_Info * mp);
static void PHK_Mgr_remove_mnt_info(zval * mnt TSRMLS_DC);
static PHK_Mnt_Info *PHK_Mgr_new_mnt_info(zval * mnt, ulong hash TSRMLS_DC);
static PHK_Mnt_Info *PHK_Mgr_get_mnt_info(zval * mnt, ulong hash, int exception TSRMLS_DC);
static void PHK_Mgr_validate(zval * mnt, ulong hash TSRMLS_DC);
static void PHK_Mgr_umount(zval * mnt, ulong hash TSRMLS_DC);
static void PHK_Mgr_umount_mnt_info(PHK_Mnt_Info * mp TSRMLS_DC);
static zval *PHK_Mgr_instance_by_mp(PHK_Mnt_Info * mp TSRMLS_DC);
static zval *PHK_Mgr_instance(zval * mnt, ulong hash TSRMLS_DC);
static zval *PHK_Mgr_proxy_by_mp(PHK_Mnt_Info * mp TSRMLS_DC);
static zval *PHK_Mgr_proxy(zval * mnt, ulong hash TSRMLS_DC);
static void PHK_Mgr_mnt_list(zval * ret TSRMLS_DC);
static int PHK_Mgr_is_a_phk_uri(zval * path TSRMLS_DC);
static void PHK_Mgr_uri(zval * mnt, zval * path, zval * ret TSRMLS_DC);
static void PHK_Mgr_command_uri(zval * mnt, zval * command,	zval * ret TSRMLS_DC);
static void PHK_Mgr_section_uri(zval * mnt, zval * section,	zval * ret TSRMLS_DC);
static void PHK_Mgr_normalize_uri(zval * uri, zval * ret TSRMLS_DC);
static void compute_automap_uri(zval * mnt, zval * ret TSRMLS_DC);
static void compute_base_uri(zval * mnt, zval * ret TSRMLS_DC);
static void PHK_Mgr_uri_to_mnt(zval * uri, zval * ret TSRMLS_DC);
static void PHK_Mgr_php_version_check(TSRMLS_D);
static void PHK_Mgr_set_cache(zval * zp TSRMLS_DC);
static int PHK_Mgr_cache_enabled(zval * mnt, ulong hash, zval * command, zval * params, zval * path TSRMLS_DC);
static void PHK_Mgr_path_to_mnt(zval * path, zval * mnt TSRMLS_DC);
static void PHK_Mgr_compute_mnt(zval * path, PHK_Mnt_Info ** parent_mpp,zval ** mnt, zval ** mtime TSRMLS_DC);
static PHK_Mnt_Info *PHK_Mgr_mount(zval * path, int flags TSRMLS_DC);
static PHK_Mnt_Persistent_Data *PHK_Mgr_get_persistent_data(zval * mnt, ulong hash TSRMLS_DC);
static PHK_Mnt_Persistent_Data *PHK_Mgr_get_or_create_persistent_data(zval * mnt,ulong hash TSRMLS_DC);
static void PHK_Mgr_populate_persistent_data(zval * mnt, ulong hash, PHK_Mnt_Info * mp TSRMLS_DC);
static void PHK_Mgr_Persistent_Data_dtor(PHK_Mnt_Persistent_Data * entry);

/*---------------------------------------------------------------*/
/*--- Init persistent data */

static void PHK_Mgr_init_persistent_data(TSRMLS_D)
{
	MutexSetup(persistent_mtab);

	zend_hash_init(&persistent_mtab, 16, NULL,
				   (dtor_func_t) PHK_Mgr_Persistent_Data_dtor, 1);

	tmp_mnt_num = 0;
}

/*---------------------------------------------------------------*/

static void PHK_Mgr_shutdown_persistent_data(TSRMLS_D)
{
	zend_hash_destroy(&persistent_mtab);

	MutexShutdown(persistent_mtab);
}

/*---------------------------------------------------------------*/
/* Here, we check every pointers because the function can be called during
   the creation of the structure (mount failure) */

static void PHK_Mgr_mnt_info_dtor(PHK_Mnt_Info * mp)
{
	if (mp->refcountp) (*(mp->refcountp))--;

	EALLOCATE(mp->children,0);

	if (mp->mnt) zval_ptr_dtor(&(mp->mnt));
	if (mp->instance) zval_ptr_dtor(&(mp->instance));
	if (mp->proxy) zval_ptr_dtor(&(mp->proxy));
	if (mp->path) zval_ptr_dtor(&(mp->path));
	if (mp->plugin)	zval_ptr_dtor(&(mp->plugin));
	if (mp->flags) zval_ptr_dtor(&(mp->flags));
	if (mp->caching) zval_ptr_dtor(&(mp->caching));
	if (mp->mtime) zval_ptr_dtor(&(mp->mtime));
	if (mp->backend) zval_ptr_dtor(&(mp->backend));

	/* The following calls just decrement the refcount, as it is always >1 */

	if (mp->min_version) zval_ptr_dtor(&(mp->min_version));
	if (mp->options) zval_ptr_dtor(&(mp->options));
	if (mp->build_info) zval_ptr_dtor(&(mp->build_info));

	if (mp->mime_types) zval_ptr_dtor(&(mp->mime_types));
	if (mp->web_run_script) zval_ptr_dtor(&(mp->web_run_script));
	if (mp->plugin_class) zval_ptr_dtor(&(mp->plugin_class));
	if (mp->web_access) zval_ptr_dtor(&(mp->web_access));
	if (mp->min_php_version) zval_ptr_dtor(&(mp->min_php_version));
	if (mp->max_php_version) zval_ptr_dtor(&(mp->max_php_version));

	if (mp->base_uri) zval_ptr_dtor(&(mp->base_uri));
	if (mp->automap_uri) zval_ptr_dtor(&(mp->automap_uri));
	if (mp->mount_script_uri) zval_ptr_dtor(&(mp->mount_script_uri));
	if (mp->umount_script_uri)zval_ptr_dtor(&(mp->umount_script_uri));
	if (mp->lib_run_script_uri) zval_ptr_dtor(&(mp->lib_run_script_uri));
	if (mp->cli_run_command) zval_ptr_dtor(&(mp->cli_run_command));
}

/*---------------------------------------------------------------*/

static void PHK_Mgr_remove_mnt_info(zval * mnt TSRMLS_DC)
{
	if (PHK_G(mtab)) {
		(void) zend_hash_del(PHK_G(mtab), Z_STRVAL_P(mnt), Z_STRLEN_P(mnt) + 1);
	}
}

/*---------------------------------------------------------------*/
/* If the mnt_info cannot be created, clear it in array and return NULL */

static PHK_Mnt_Info *PHK_Mgr_new_mnt_info(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt_Info tmp, *mp;

	if (!hash) hash = ZSTRING_HASH(mnt);

	if (!PHK_G(mtab)) {
		EALLOCATE(PHK_G(mtab),sizeof(HashTable));
		zend_hash_init(PHK_G(mtab), 16, NULL,
					   (dtor_func_t) PHK_Mgr_mnt_info_dtor, 0);
	}

	memset(&tmp, 0, sizeof(tmp));	/* Init everything to 0/NULL */

	zend_hash_quick_update(PHK_G(mtab), Z_STRVAL_P(mnt),
						   Z_STRLEN_P(mnt) + 1, hash, &tmp, sizeof(tmp),
						   (void **) &mp);

	mp->hash = hash;

	mp->mnt = mnt;				/* Don't add ref */

	return mp;
}

/*---------------------------------------------------------------*/

static PHK_Mnt_Info *PHK_Mgr_get_mnt_info(zval * mnt, ulong hash,
										  int exception TSRMLS_DC)
{
	PHK_Mnt_Info *mp;
	int found;

	if (Z_TYPE_P(mnt) != IS_STRING) {
		EXCEPTION_ABORT_RET_1(NULL,
							  "PHK_Mgr_get_mnt_info: Mount point should be a string (type=%s)",
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
/* {{{ proto bool PHK_Mgr::is_mounted(string mnt) */

static PHP_METHOD(PHK_Mgr, is_mounted)
{
	zval *mnt;
	int retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	retval = (PHK_Mgr_get_mnt_info(mnt, 0, 0 TSRMLS_CC) != NULL);
	RETVAL_BOOL(retval);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_validate(zval * mnt, ulong hash TSRMLS_DC)
{
	if (!hash) hash = ZSTRING_HASH(mnt);
	(void) PHK_Mgr_get_mnt_info(mnt, hash, 1 TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::validate(string mnt) */

static PHP_METHOD(PHK_Mgr, validate)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	(void) PHK_Mgr_validate(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
// Must accept an invalid mount point without error

static void PHK_Mgr_umount(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt_Info *mp;

	mp = PHK_Mgr_get_mnt_info(mnt, hash, 0 TSRMLS_CC);
	if (!mp) return;

	PHK_Mgr_umount_mnt_info(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void PHK_Mgr_umount_mnt_info(PHK_Mnt_Info * mp TSRMLS_DC)
{
	int i;

	if (mp->nb_children) {
		for (i = 0; i < mp->nb_children; i++) {
			if (mp->children[i])
				PHK_Mgr_umount_mnt_info(mp->children[i] TSRMLS_CC);
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

	PHK_Mgr_remove_mnt_info(mp->mnt TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::umount(string mnt) */

static PHP_METHOD(PHK_Mgr, umount)
{
	zval *mnt;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_umount(mnt, 0 TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static zval *PHK_Mgr_instance_by_mp(PHK_Mnt_Info * mp TSRMLS_DC)
{
	if (!mp->instance) {
		ut_new_instance(&(mp->instance), &CZVAL(PHK), 0, 0,	NULL TSRMLS_CC);
		PHK_set_mp_property(mp->instance, mp TSRMLS_CC);
	}

	return mp->instance;
}

/*---------------------------------------------------------------*/

static zval *PHK_Mgr_instance(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt_Info *mp;

	mp = PHK_Mgr_get_mnt_info(mnt, hash, 1 TSRMLS_CC);
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
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

/*DBG_MSG1("Entering PHK_Mgr::instance(%s)",Z_STRVAL_P(mnt));*/

	instance = PHK_Mgr_instance(mnt, 0 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(instance);
}

/* }}} */
/*---------------------------------------------------------------*/

static zval *PHK_Mgr_proxy_by_mp(PHK_Mnt_Info * mp TSRMLS_DC)
{
	zval *args[2];

	if (!(mp->proxy)) {
		args[0] = mp->path;
		args[1] = mp->flags;
		ut_new_instance(&(mp->proxy), &CZVAL(PHK_Proxy), 1, 2,
						args TSRMLS_CC);
	}

	return mp->proxy;
}

/*---------------------------------------------------------------*/

static zval *PHK_Mgr_proxy(zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt_Info *mp;

	mp = PHK_Mgr_get_mnt_info(mnt, hash, 1 TSRMLS_CC);
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
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	proxy = PHK_Mgr_proxy(mnt, 0 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(proxy);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_mnt_list(zval * ret TSRMLS_DC)
{
	char *mnt_p;
	int mnt_len;
	ulong dummy;
	HashPosition pos;

	array_init(ret);

	if (PHK_G(mtab)) {
		zend_hash_internal_pointer_reset_ex(PHK_G(mtab), &pos);
		while (zend_hash_get_current_key_ex(PHK_G(mtab), &mnt_p, &mnt_len
			, &dummy, 1, &pos) != HASH_KEY_NON_EXISTANT) {
			add_next_index_stringl(ret, mnt_p, mnt_len - 1, 1);
			zend_hash_move_forward_ex(PHK_G(mtab), &pos);
		}
	}
}

/*---------------------------------------------------------------*/
/* {{{ proto array PHK_Mgr::mnt_list() */

static PHP_METHOD(PHK_Mgr, mnt_list)
{
	PHK_Mgr_mnt_list(return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static int PHK_Mgr_is_a_phk_uri(zval * path TSRMLS_DC)
{
	return ((Z_STRVAL_P(path)[0] == 'p')
			&& (Z_STRVAL_P(path)[1] == 'h')
			&& (Z_STRVAL_P(path)[2] == 'k')
			&& (Z_STRVAL_P(path)[3] == ':')
			&& (Z_STRVAL_P(path)[4] == '/')
			&& (Z_STRVAL_P(path)[5] == '/'));
}

/*---------------------------------------------------------------*/
/* {{{ proto bool PHK_Mgr::is_a_phk_uri(string uri) */

static PHP_METHOD(PHK_Mgr, is_a_phk_uri)
{
	zval *path;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	RETVAL_BOOL(PHK_Mgr_is_a_phk_uri(path TSRMLS_CC));
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
	ZVAL_STRINGL(ret,(_currentp=eallocate(NULL,len+1)),len,0); \
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
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_uri(mnt, path, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_command_uri(zval * mnt, zval * command,
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
/* {{{ proto string PHK_Mgr::command_uri(string mnt, string command) */

static PHP_METHOD(PHK_Mgr, command_uri)
{
	zval *mnt, *command;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &mnt, &command) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_command_uri(mnt, command, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_section_uri(zval * mnt, zval * section,
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
/* {{{ proto string PHK_Mgr::section_uri(string mnt, string section) */

static PHP_METHOD(PHK_Mgr, section_uri)
{
	zval *mnt, *section;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zz", &mnt, &section) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_section_uri(mnt, section, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_normalize_uri(zval * uri, zval * ret TSRMLS_DC)
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
/* {{{ proto string PHK_Mgr::normalize_uri(string uri) */

static PHP_METHOD(PHK_Mgr, normalize_uri)
{
	zval *uri;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &uri) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_normalize_uri(uri, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static void compute_automap_uri(zval * mnt, zval * ret TSRMLS_DC)
{
	PHK_ZSTR_BASE_DECL()

	PHK_ZSTR_ALLOC_ZSTRING(Z_STRLEN_P(mnt) + 28);

	PHK_ZSTR_INIT_URI();
	PHK_ZSTR_APPEND_ZVAL(mnt);

	PHK_ZSTR_APPEND_STRING("/?section&name=AUTOMAP", 22);
}

/*---------------------------------------------------------------*/
/* {{{ proto string|null PHK_Mgr::automap_uri(string mnt) */

static PHP_METHOD(PHK_Mgr, automap_uri)
{
	zval *mnt;
	PHK_Mnt_Info *mp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	mp = PHK_Mgr_get_mnt_info(mnt, 0, 1 TSRMLS_CC);
	if (EG(exception) || (!mp->automap_uri)) return;

	RETVAL_BY_REF(mp->automap_uri);
}

/* }}} */
/*---------------------------------------------------------------*/

static void compute_base_uri(zval * mnt, zval * ret TSRMLS_DC)
{
	PHK_ZSTR_BASE_DECL()

	PHK_ZSTR_ALLOC_ZSTRING(Z_STRLEN_P(mnt) + 7);

	PHK_ZSTR_INIT_URI();
	PHK_ZSTR_APPEND_ZVAL(mnt);
	PHK_ZSTR_APPEND_STRING("/", 1);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::base_uri(string mnt) */

static PHP_METHOD(PHK_Mgr, base_uri)
{
	zval *mnt;
	PHK_Mnt_Info *mp;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	mp = PHK_Mgr_get_mnt_info(mnt, 0, 1 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(mp->base_uri);
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_uri_to_mnt(zval * uri, zval * ret TSRMLS_DC)
{
	char *bp, *p;
	int len;

	if (!PHK_Mgr_is_a_phk_uri(uri TSRMLS_CC)) {
		EXCEPTION_ABORT_1("%s: Not a PHK URI", Z_STRVAL_P(uri));
	}

	bp = p = Z_STRVAL_P(uri) + 6;
	while ((*p) && ((*p) != '/') && ((*p) != '\\') && ((*p) != ' '))
		p++;
	len = p - bp;

	zval_dtor(ret);
	ZVAL_STRINGL(ret, bp, len, 1);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::uri_to_mnt(string uri) */

static PHP_METHOD(PHK_Mgr, uri_to_mnt)
{
	zval *uri;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &uri) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_uri_to_mnt(uri, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* Empty as long as PHK does not require more than PHP 5.1 as, if version
* is <5.1, we don't even get here */

static void PHK_Mgr_php_version_check(TSRMLS_D)
{
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::php_version_check() */

static PHP_METHOD(PHK_Mgr, php_version_check)
{
}

/* }}} */
/*---------------------------------------------------------------*/

static void PHK_Mgr_set_cache(zval * zp TSRMLS_DC)
{
	if ((Z_TYPE_P(zp) != IS_NULL) && (Z_TYPE_P(zp) != IS_BOOL)) {
		EXCEPTION_ABORT_1("set_cache value can be only bool or null", 0);
	}

	PHK_G(caching) = (*zp);	/* No need to copy_ctor() as it can only be null/bool */
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::set_cache(mixed toggle) */

static PHP_METHOD(PHK_Mgr, set_cache)
{
	zval *caching;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &caching) ==
		FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_set_cache(caching TSRMLS_CC);
}


/* }}} */
/*---------------------------------------------------------------*/

static int PHK_Mgr_cache_enabled(zval * mnt, ulong hash, zval * command,
								 zval * params, zval * path TSRMLS_DC)
{
	PHK_Mnt_Info *mp;

	if (Z_TYPE(PHK_G(caching)) != IS_NULL) return Z_BVAL(PHK_G(caching));

	if (ZVAL_IS_NULL(mnt)) return 0; /* Default for global commands: no cache */

	mp = PHK_Mgr_get_mnt_info(mnt, hash, 1 TSRMLS_CC);
	if (EG(exception)) return 0;

	return PHK_cache_enabled(mp, command, params, path TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* In case of error, free mem allocated by compute_mnt() */

static void PHK_Mgr_path_to_mnt(zval * path, zval * mnt TSRMLS_DC)
{
	zval *tmp_mnt;

	PHK_Mgr_compute_mnt(path, NULL, &tmp_mnt, NULL TSRMLS_CC);
	if (EG(exception)) return;

	PHK_Mgr_get_mnt_info(tmp_mnt, 0, 1 TSRMLS_CC);
	if (EG(exception)) {
		zval_ptr_dtor(&tmp_mnt);
		return;
	}

	(*mnt) = (*tmp_mnt);
	zval_copy_ctor(mnt);

	zval_ptr_dtor(&tmp_mnt);
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::path_to_mnt(string path) */

static PHP_METHOD(PHK_Mgr, path_to_mnt)
{
	zval *path;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &path) ==
		FAILURE) EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	PHK_Mgr_path_to_mnt(path, return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* On entry, parent_mnt, mnt, and mtime can be null */

#define INIT_PHK_MGR_COMPUTE_MNT() \
	{ \
	INIT_ZVAL(subpath); \
	INIT_ZVAL(tmp_parent_mnt); \
	INIT_ZVAL(tmp_mnt); \
	}

#define CLEANUP_PHK_MGR_COMPUTE_MNT() \
	{ \
	zval_dtor(&subpath); \
	zval_dtor(&tmp_parent_mnt); \
	zval_dtor(&tmp_mnt); \
	}

#define ABORT_PHK_MGR_COMPUTE_MNT() \
	{ \
	CLEANUP_PHK_MGR_COMPUTE_MNT(); \
	return; \
	}

static void PHK_Mgr_compute_mnt(zval * path, PHK_Mnt_Info ** parent_mpp,
								zval ** mnt, zval ** mtime TSRMLS_DC)
{
	zval subpath, tmp_mnt, tmp_parent_mnt;
	int len;
	char *p;
	php_stream_statbuf ssb;
	PHK_Mnt_Info *parent_mp;
	time_t mti;

	INIT_PHK_MGR_COMPUTE_MNT();

	if (PHK_Mgr_is_a_phk_uri(path TSRMLS_CC)) {	/* Sub-package */
		_phk_stream_parse_uri2(path, NULL, NULL, &tmp_parent_mnt,
							   &subpath TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_COMPUTE_MNT();

		parent_mp = PHK_Mgr_get_mnt_info(&tmp_parent_mnt, 0, 1 TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_COMPUTE_MNT();

		if (parent_mpp) (*parent_mpp) = parent_mp;

		if (mnt) {
			p = Z_STRVAL(subpath);
			while (*p) {
				if ((*p) == '/')
					(*p) = '*';
				p++;
			}
			len = Z_STRLEN(tmp_parent_mnt) + Z_STRLEN(subpath) + 1;
			spprintf(&p, len, "%s#%s", Z_STRVAL(tmp_parent_mnt),
					 Z_STRVAL(subpath));
			if (*mnt) zval_ptr_dtor(mnt);
			MAKE_STD_ZVAL(*mnt);
			ZVAL_STRINGL((*mnt), p, len, 0);
		}

		if (mtime) {
			(*mtime) = parent_mp->mtime;	/*Add ref to parent's mtime */
			ZVAL_ADDREF(parent_mp->mtime);
		}
	} else {
		if (php_stream_stat_path(Z_STRVAL_P(path), &ssb) != 0) {
			THROW_EXCEPTION_1("%s: File not found", Z_STRVAL_P(path));
			ABORT_PHK_MGR_COMPUTE_MNT();
		}
#ifdef NETWARE
		mti = ssb.sb.st_mtime.tv_sec;
#else
		mti = ssb.sb.st_mtime;
#endif

		if (parent_mpp) (*parent_mpp) = NULL;

		if (mnt) {
			spprintf(&p, 256, "%lX_%lX_%lX",
					 (unsigned long) (ssb.sb.st_dev)
					 , (unsigned long) (ssb.sb.st_ino),
					 (unsigned long) mti);
			MAKE_STD_ZVAL(*mnt);
			ZVAL_STRING((*mnt), p, 0);
		}

		if (mtime) {
			if (*mtime) zval_ptr_dtor(mtime);
			MAKE_STD_ZVAL((*mtime));
			ZVAL_LONG((*mtime), mti);
		}
	}
	CLEANUP_PHK_MGR_COMPUTE_MNT();
}

/*---------------------------------------------------------------*/

#define INIT_PHK_MGR_MOUNT() \
	{ \
	mp=NULL; \
	mnt=mtime=NULL; \
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
		PHK_Mgr_remove_mnt_info(mp->mnt TSRMLS_CC); \
	} \
	return NULL; \
	}

static PHK_Mnt_Info *PHK_Mgr_mount(zval * path, int flags TSRMLS_DC)
{
	zval *args[5], *mnt, *mtime;
	PHK_Mnt_Info *mp, *parent_mp;
	char *p;
	ulong hash;
	int len;

	DBG_MSG2("Entering PHK_Mgr_mount(%s,%d)", Z_STRVAL_P(path), flags);

	INIT_PHK_MGR_MOUNT();

	if (!ZVAL_IS_STRING(path)) convert_to_string(path);

	if (flags & PHK_F_CREATOR) {
		spprintf(&p, 32, "_tmp_mnt_%d", tmp_mnt_num++);
		MAKE_STD_ZVAL(mnt);
		ZVAL_STRING(mnt, p, 0);

		mp = PHK_Mgr_new_mnt_info(mnt, 0 TSRMLS_CC);
		if (!mp) {
			zval_dtor(mnt);
			ABORT_PHK_MGR_MOUNT();
		}

		SEPARATE_ARG_IF_REF(path);
		mp->path = path;

		MAKE_STD_ZVAL(mp->flags);
		ZVAL_LONG(mp->flags, flags);

		ALLOC_INIT_ZVAL(mp->caching);

		MAKE_STD_ZVAL((mp->mtime));
		ZVAL_LONG((mp->mtime), time(NULL));

		MAKE_STD_ZVAL(mp->min_version);
		ZVAL_STRINGL((mp->min_version), "", 0, 1);

		MAKE_STD_ZVAL(mp->options);
		array_init(mp->options);

		MAKE_STD_ZVAL(mp->build_info);
		array_init(mp->build_info);

		MAKE_STD_ZVAL(mp->base_uri);
		compute_base_uri(mnt, mp->base_uri TSRMLS_CC);

		args[0] = mp->mnt;
		args[1] = mp->path;
		args[2] = mp->flags;
		ut_new_instance(&(mp->instance), &CZVAL(PHK_Creator), 1, 3, args
						TSRMLS_CC);
	} else {
		mnt = mtime = NULL;		/* Security */
		PHK_Mgr_compute_mnt(path, &parent_mp, &mnt, &mtime TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_MOUNT();

		hash = ZSTRING_HASH(mnt);
		if ((mp = PHK_Mgr_get_mnt_info(mnt, hash, 0 TSRMLS_CC)) != NULL) {
			/* Already mounted */
			zval_ptr_dtor(&mnt);
			zval_ptr_dtor(&mtime);
			return mp;
		}

		mp = PHK_Mgr_new_mnt_info(mnt, hash TSRMLS_CC);
		if (!mp) {
			zval_ptr_dtor(&mnt);
			zval_ptr_dtor(&mtime);
			ABORT_PHK_MGR_MOUNT();
		}

		mp->mtime = mtime;

		ALLOC_INIT_ZVAL(mp->path);
		p=ut_mk_absolute_path(Z_STRVAL_P(path),Z_STRLEN_P(path),&len,0 TSRMLS_CC);
		ZVAL_STRINGL(mp->path, p, len, 0);

		if (parent_mp) {
			mp->parent = parent_mp;
			EALLOCATE(parent_mp->children
				,(parent_mp->nb_children + 1) * sizeof(PHK_Mnt_Info *));
			parent_mp->children[parent_mp->nb_children++] = mp;
		}

		MAKE_STD_ZVAL(mp->flags);
		ZVAL_LONG(mp->flags, flags);

		ALLOC_INIT_ZVAL(mp->caching);

		PHK_Mgr_populate_persistent_data(mnt, hash, mp TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_MGR_MOUNT();

		PHK_init(mp TSRMLS_CC);

		/* Instance object is created when needed by PHK_Mgr_instance() */
	}

	if (EG(exception)) ABORT_PHK_MGR_MOUNT();
	return mp;
}

/*---------------------------------------------------------------*/
/* {{{ proto string PHK_Mgr::mount(string path [, int flags ]) */

static PHP_METHOD(PHK_Mgr, mount)
{
	zval *path;
	int flags;
	PHK_Mnt_Info *mp;

	flags = 0;
	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "z|l", &path, &flags) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	mp = PHK_Mgr_mount(path, flags TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(mp->mnt);
}

/* }}} */
/*---------------------------------------------------------------*/

static PHK_Mnt_Persistent_Data *PHK_Mgr_get_persistent_data(
	zval * mnt, ulong hash TSRMLS_DC)
{
	PHK_Mnt_Persistent_Data *pmp;

	if (!ZVAL_IS_STRING(mnt)) {
		EXCEPTION_ABORT_RET_1(NULL,"PHK_get_persistent_data: Mount point should be a string (type=%s)",
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
	INIT_ZVAL(min_version); \
	INIT_ZVAL(options); \
	}

#define CLEANUP_PHK_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	if (!ZVAL_IS_NULL(&min_version)) zval_dtor(&min_version); \
	if (!ZVAL_IS_NULL(&options)) zval_dtor(&options); \
	}
	
#define RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(_ret) \
	{ \
	MutexUnlock(persistent_mtab); \
	return _ret; \
	}

#define ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA() \
	{ \
	CLEANUP_PHK_GET_OR_CREATE_PERSISTENT_DATA(); \
	RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(NULL); \
	}

static PHK_Mnt_Persistent_Data *PHK_Mgr_get_or_create_persistent_data(zval * mnt,
	ulong hash TSRMLS_DC)
{
	PHK_Mnt_Persistent_Data tmp_entry, *entry;
	zval min_version, options, build_info, *args[2], **zpp, zv, zv2;
	HashTable *opt_ht;
	char *p;

	DBG_MSG1("Entering PHK_Mgr_get_persistent_data(%s)", Z_STRVAL_P(mnt));

	INIT_PHK_GET_OR_CREATE_PERSISTENT_DATA();

	if (!hash) hash = ZSTRING_HASH(mnt);

	entry=PHK_Mgr_get_persistent_data(mnt, hash TSRMLS_CC);
	if (entry) {
		entry->nb_hits++;
		entry->refcount++;
		RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(entry);
	}

	/* Create entry - slow path */

	PHK_need_php_runtime(TSRMLS_C);

	/* We won't cache this information as they are read only once per
	* thread/process, when the package is opened for the 1st time. */

	args[0] = mnt;
	args[1] = &CZVAL(false);
	ut_call_user_function_string(&CZVAL(PHK_Util)
		, &CZVAL(get_min_version), &min_version,2, args TSRMLS_CC);
	if (EG(exception)) ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	if (Z_TYPE(min_version) != IS_STRING) {
		THROW_EXCEPTION_1("Internal error: PHK_Util::get_min_version should return a string (type=%s)",
						  zend_zval_type_name(&min_version));
		ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	}

	/* Check min_version */

	if (php_version_compare(Z_STRVAL(min_version), PHK_ACCEL_MIN_VERSION) > 0) {
		THROW_EXCEPTION_1("Cannot understand this format. Requires version %s",
							  Z_STRVAL(min_version));
		ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	}

	ut_call_user_function_array(&CZVAL(PHK_Util)
			, &CZVAL(get_options), &options, 2,	args TSRMLS_CC);

	/* Check that the required extensions are present or can be loaded */
	/* The PHP code must check this in PHK::init() for each mount() because
	   it is not sure to remain in the same thread. Here, as dynamic extension
	   loading is not supported in multithread mode, we can check it here. */

	if ((FIND_HKEY(Z_ARRVAL(options),required_extensions,&zpp)==SUCCESS)
		&& (ZVAL_IS_ARRAY(*zpp))) {
		ut_load_extensions(*zpp TSRMLS_CC);
		if (EG(exception)) ABORT_PHK_GET_OR_CREATE_PERSISTENT_DATA();
	}

	/* Now, we can create the entry (it cannot fail any more) */

	zend_hash_quick_update(&persistent_mtab, Z_STRVAL_P(mnt)
						   , Z_STRLEN_P(mnt) + 1, hash, &tmp_entry,
						   sizeof(tmp_entry), (void **) &entry);

	entry->ctime = time(NULL);

	entry->nb_hits=entry->refcount=1;

	ut_persist_zval(&min_version, &(entry->min_version));

	ut_persist_zval(&options, &(entry->options));
	opt_ht = Z_ARRVAL(entry->options);

	INIT_ZVAL(build_info);
	ut_call_user_function_array(&CZVAL(PHK_Util)
		, &CZVAL(get_build_info), &build_info, 2, args TSRMLS_CC);
	ut_persist_zval(&build_info, &(entry->build_info));
	zval_dtor(&build_info);

	/* Set shortcuts */

#define PHK_GPD_BOOL_SHORTCUT(name) \
	entry->name=((FIND_HKEY(opt_ht,name,&zpp)==SUCCESS) \
		&& (ZVAL_IS_BOOL(*zpp))) ? Z_BVAL_PP(zpp) : 0

	PHK_GPD_BOOL_SHORTCUT(crc_check);
	PHK_GPD_BOOL_SHORTCUT(no_cache);
	PHK_GPD_BOOL_SHORTCUT(no_opcode_cache);
	PHK_GPD_BOOL_SHORTCUT(web_main_redirect);
	PHK_GPD_BOOL_SHORTCUT(auto_umount);

	/* If it is present, PHK_Creator ensures that option['mime_types']
		is an array. But it must be checked for security */

#define PHK_GPD_ARRAY_SHORTCUT(name) \
	entry->name=((FIND_HKEY(opt_ht,name,&zpp)==SUCCESS) \
		&& (ZVAL_IS_ARRAY(*zpp))) ? (*zpp) : NULL

	PHK_GPD_ARRAY_SHORTCUT(mime_types);

#define PHK_GPD_STRING_SHORTCUT(name) \
	entry->name=((FIND_HKEY(opt_ht,name,&zpp)==SUCCESS) \
		&& (ZVAL_IS_STRING(*zpp))) ? (*zpp) : NULL

	PHK_GPD_STRING_SHORTCUT(web_run_script);
	PHK_GPD_STRING_SHORTCUT(plugin_class);
	PHK_GPD_STRING_SHORTCUT(web_access);
	PHK_GPD_STRING_SHORTCUT(min_php_version);
	PHK_GPD_STRING_SHORTCUT(max_php_version);

	/* Pre-computed values */

	INIT_ZVAL(entry->base_uri);
	compute_base_uri(mnt, &zv TSRMLS_CC);
	ut_persist_zval(&zv, &(entry->base_uri));
	zval_dtor(&zv);

	INIT_ZVAL(entry->automap_uri);
	if ((FIND_HKEY(Z_ARRVAL(entry->build_info), map_defined, &zpp) ==
		 SUCCESS) && ZVAL_IS_BOOL(*zpp) && Z_BVAL_PP(zpp)) {
		compute_automap_uri(mnt, &zv TSRMLS_CC);
		ut_persist_zval(&zv, &(entry->automap_uri));
		zval_dtor(&zv);
	} else {
		ZVAL_NULL(&(entry->automap_uri));
	}

	INIT_ZVAL(entry->mount_script_uri);
	if ((FIND_HKEY(opt_ht, mount_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		PHK_Mgr_uri(mnt, *zpp, &zv TSRMLS_CC);
		ut_persist_zval(&zv, &(entry->mount_script_uri));
		zval_dtor(&zv);
	} else {
		ZVAL_NULL(&(entry->mount_script_uri));
	}

	INIT_ZVAL(entry->umount_script_uri);
	if ((FIND_HKEY(opt_ht, umount_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		PHK_Mgr_uri(mnt, *zpp, &zv TSRMLS_CC);
		ut_persist_zval(&zv, &(entry->umount_script_uri));
		zval_dtor(&zv);
	} else {
		ZVAL_NULL(&(entry->umount_script_uri));
	}

	INIT_ZVAL(entry->lib_run_script_uri);
	if ((FIND_HKEY(opt_ht, lib_run_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		PHK_Mgr_uri(mnt, *zpp, &zv TSRMLS_CC);
		ut_persist_zval(&zv, &(entry->lib_run_script_uri));
		zval_dtor(&zv);
	} else {
		ZVAL_NULL(&(entry->lib_run_script_uri));
	}

	INIT_ZVAL(entry->cli_run_command);
	if ((FIND_HKEY(opt_ht, cli_run_script, &zpp) == SUCCESS)
		&& (ZVAL_IS_STRING(*zpp))) {
		PHK_Mgr_uri(mnt, *zpp, &zv TSRMLS_CC);
		spprintf(&p, UT_PATH_MAX, "require('%s');", Z_STRVAL(zv));
		ZVAL_STRING(&zv2, p, 0);
		ut_persist_zval(&zv2, &(entry->cli_run_command));
		zval_dtor(&zv);
		zval_dtor(&zv2);
	} else {
		ZVAL_NULL(&(entry->cli_run_command));
	}

	/* Cleanup and return */

	CLEANUP_PHK_GET_OR_CREATE_PERSISTENT_DATA();

	RETURN_FROM_PHK_GET_OR_CREATE_PERSISTENT_DATA(entry);
}

/*---------------------------------------------------------------*/

#define PHK_MGR_PPD_COPY_ZVAL(_field) \
	{ \
		if (ZVAL_IS_NULL(&(entry->_field)))	mp->_field = NULL; \
		else { \
			mp->_field = &(entry->_field); \
			ZVAL_ADDREF(&(entry->_field)); \
		} \
	}

#define PHK_MGR_PPD_COPY_PZVAL(_field) \
	{ \
		mp->_field = entry->_field; \
		if (mp->_field) ZVAL_ADDREF(entry->_field); \
	}

static void PHK_Mgr_populate_persistent_data(zval * mnt, ulong hash,
											 PHK_Mnt_Info * mp TSRMLS_DC)
{
	PHK_Mnt_Persistent_Data *entry;

	DBG_MSG1("Entering PHK_Mgr_populate_persistent_data(%s)",
			 Z_STRVAL_P(mnt));

	entry = PHK_Mgr_get_or_create_persistent_data(mnt, hash TSRMLS_CC);
	if (EG(exception)) return;

	/* Populate mp structure */

	mp->refcountp = &(entry->refcount);

	PHK_MGR_PPD_COPY_ZVAL(min_version);
	PHK_MGR_PPD_COPY_ZVAL(options);
	PHK_MGR_PPD_COPY_ZVAL(build_info);
	PHK_MGR_PPD_COPY_ZVAL(base_uri);
	PHK_MGR_PPD_COPY_ZVAL(automap_uri);
	PHK_MGR_PPD_COPY_ZVAL(mount_script_uri);
	PHK_MGR_PPD_COPY_ZVAL(umount_script_uri);
	PHK_MGR_PPD_COPY_ZVAL(lib_run_script_uri);
	PHK_MGR_PPD_COPY_ZVAL(cli_run_command);

	mp->crc_check = entry->crc_check;
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
}

/*---------------------------------------------------------------*/
/* {{{ proto void PHK_Mgr::mime_header(string mnt, int hash, string path) */

/* Undocumented - called by the PHK runtime code only */

static PHP_METHOD(PHK_Mgr, mime_header)
{
	zval *mnt, *path;
	ulong hash;
	PHK_Mnt_Info *mp;

	if (zend_parse_parameters
		(ZEND_NUM_ARGS()TSRMLS_CC, "zlz", &mnt, &hash, &path) == FAILURE)
		EXCEPTION_ABORT_1("Cannot parse parameters", 0);

	mp = PHK_Mgr_get_mnt_info(mnt, hash, 1 TSRMLS_CC);
	if (EG(exception)) return;

	PHK_mime_header(mp, path TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/
/* zval_dtor works for persistent arrays, but not for persistent strings */

static void PHK_Mgr_Persistent_Data_dtor(PHK_Mnt_Persistent_Data * entry)
{
	ut_persistent_zval_dtor(&(entry->min_version));
	ut_persistent_zval_dtor(&(entry->options));
	ut_persistent_zval_dtor(&(entry->build_info));

	ut_persistent_zval_dtor(&(entry->automap_uri));
	ut_persistent_zval_dtor(&(entry->base_uri));
	ut_persistent_zval_dtor(&(entry->mount_script_uri));
	ut_persistent_zval_dtor(&(entry->umount_script_uri));
}

/*---------------------------------------------------------------*/

static zend_function_entry PHK_Mgr_functions[] = {
	PHP_ME(PHK_Mgr, is_mounted, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, validate, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, instance, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, proxy, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, mnt_list, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, set_cache, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, path_to_mnt, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, mount, UT_2args_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, umount, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, uri, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, is_a_phk_uri, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, base_uri, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, command_uri, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, section_uri, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, automap_uri, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, normalize_uri, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, uri_to_mnt, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, php_version_check, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(PHK_Mgr, mime_header, UT_3args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/
/* Module initialization                                         */

static int MINIT_PHK_Mgr(TSRMLS_D)
{
	zend_class_entry ce;

	/*--- Init class */

	INIT_CLASS_ENTRY(ce, "PHK_Mgr", PHK_Mgr_functions);
	zend_register_internal_class(&ce TSRMLS_CC);

	/*--- Init persistent data */

	PHK_Mgr_init_persistent_data(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Module shutdown                                               */

static int MSHUTDOWN_PHK_Mgr(TSRMLS_D)
{
	PHK_Mgr_shutdown_persistent_data(TSRMLS_C);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_PHK_Mgr(TSRMLS_D)
{
	ZVAL_NULL(&PHK_G(caching));

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RSHUTDOWN_PHK_Mgr(TSRMLS_D)
{
	if (PHK_G(mtab)) {
		zend_hash_destroy(PHK_G(mtab));
		efree(PHK_G(mtab));
		PHK_G(mtab) = NULL;
	}

	return SUCCESS;
}

/*---------------------------------------------------------------*/
