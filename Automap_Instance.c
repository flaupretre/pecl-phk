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

static zval *Automap_instance_by_mp(Automap_Mnt *mp TSRMLS_DC)
{
	if (!mp->instance) {
		mp->instance=ut_new_instance(ZEND_STRL("Automap"), 0, 0, NULL TSRMLS_CC);
		zend_update_property_long(Z_OBJCE_P(mp->instance),mp->instance
			,"m",1,(long)(mp->order) TSRMLS_CC);
	}

	return mp->instance;
}

/*---------------------------------------------------------------*/

static zval *Automap_instance(zval * mnt, ulong hash TSRMLS_DC)
{
	Automap_Mnt *mp;

	mp = Automap_Mnt_get(mnt, hash, 1 TSRMLS_CC);
	if (EG(exception)) return NULL;

	return Automap_instance_by_mp(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto Automap Automap::instance(string mnt) */

static PHP_METHOD(Automap, instance)
{
	zval *mnt, *instance;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &mnt) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");

	/*DBG_MSG1("Entering Automap::instance(%s)",Z_STRVAL_P(mnt)); */

	instance = Automap_instance(mnt, 0 TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(instance);
}

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto bool $map->is_valid()  */

static PHP_METHOD(Automap, is_valid)
{
	zval **_tmp;

	RETVAL_BOOL(FIND_HKEY(Z_OBJPROP_P(getThis()),mp_property_name,&_tmp)==SUCCESS);
}

/*---------------------------------------------------------------*/
/* Instance methods. The instance object is returned by Automap::instance() */

#define AUTOMAP_GET_PROPERTY_METHOD_START(_field) \
	static PHP_METHOD(Automap, _field) \
	{ \
		AUTOMAP_GET_INSTANCE_DATA(_field) \

#define AUTOMAP_GET_PROPERTY_METHOD_END(_field) \
	}

/*---------------------------------------------------------------*/
/* {{{ proto string Automap::mnt() */

AUTOMAP_GET_PROPERTY_METHOD_START(mnt)
	RETVAL_BY_VAL(mp->map->zmnt);
AUTOMAP_GET_PROPERTY_METHOD_END(mnt)

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::path() */

AUTOMAP_GET_PROPERTY_METHOD_START(path)
	RETVAL_BY_REF(mp->zpath);
AUTOMAP_GET_PROPERTY_METHOD_END(path)

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::base_dir() */

AUTOMAP_GET_PROPERTY_METHOD_START(base_dir)
		RETVAL_BY_REF(mp->zbase);
AUTOMAP_GET_PROPERTY_METHOD_END(base_dir)

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::flags() */

AUTOMAP_GET_PROPERTY_METHOD_START(flags)
	RETVAL_LONG(mp->flags);
AUTOMAP_GET_PROPERTY_METHOD_END(flags)

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto array Automap::options() */

AUTOMAP_GET_PROPERTY_METHOD_START(options)
	RETVAL_BY_VAL(mp->map->zoptions);
AUTOMAP_GET_PROPERTY_METHOD_END(options)
/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::version() */

AUTOMAP_GET_PROPERTY_METHOD_START(version)
	RETVAL_BY_VAL(mp->map->zversion);
AUTOMAP_GET_PROPERTY_METHOD_END(version)

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto string Automap::min_version() */

AUTOMAP_GET_PROPERTY_METHOD_START(min_version)
	RETVAL_BY_VAL(mp->map->zmin_version);
AUTOMAP_GET_PROPERTY_METHOD_END(min_version)

/* }}} */
/*---------------------------------------------------------------*/

static void Automap_Instance_export_entry(Automap_Mnt *mp, Automap_Pmap_Entry *pep
	, zval *zp TSRMLS_DC)
{
	char str[2],*abspath;
	int len;

	array_init(zp);
	str[1]='\0';

	str[0]=pep->stype;
	add_assoc_stringl(zp,"stype",str,1,1);
	add_assoc_stringl(zp,"symbol",Z_STRVAL(pep->zsname),Z_STRLEN(pep->zsname),1);
	str[0]=pep->ftype;
	add_assoc_stringl(zp,"ptype",str,1,1);
	add_assoc_stringl(zp,"rpath",Z_STRVAL(pep->zfpath),Z_STRLEN(pep->zfpath),1);
	abspath=Automap_Mnt_abs_path(mp,pep,&len TSRMLS_CC);
	add_assoc_stringl(zp,"path",abspath,len,0);
}

/*---------------------------------------------------------------*/
/* {{{ proto array Automap::symbols() */
/* return array(array(
	'stype'  => symbol type,
	'symbol' => symbol name,
	'ptype'  => path type,
	'rpath' => relative path as it appears in the map,
	'path'   => absolute path)) */

static PHP_METHOD(Automap, symbols) \
{
	zval *zp;
	Automap_Pmap_Entry *pep;
	HashTable *arrht;
	HashPosition pos;

	AUTOMAP_GET_INSTANCE_DATA(symbols)
	array_init(return_value);

	arrht=Z_ARRVAL_P(mp->map->zsymbols);
	for (zend_hash_internal_pointer_reset_ex(arrht,&pos);;
		zend_hash_move_forward_ex(arrht,&pos)) {
		if (zend_hash_has_more_elements_ex(arrht,&pos)!=SUCCESS) break;
		zend_hash_get_current_data_ex(arrht,(void **)(&pep),&pos);
		ALLOC_INIT_ZVAL(zp);
		Automap_Instance_export_entry(mp,pep,zp TSRMLS_CC);
		add_next_index_zval(return_value,zp);
	}
AUTOMAP_GET_PROPERTY_METHOD_END(symbols)

/* }}} */
/*---------------------------------------------------------------*/
/* {{{ proto array Automap::get_symbol($type,$symbol) */

static PHP_METHOD(Automap, get_symbol)
{
	char *type,*symbol;
	int type_len,symbol_len;
	Automap_Pmap_Entry *pep;
	zval *zkey;
	unsigned long hash;

	AUTOMAP_GET_INSTANCE_DATA(get_symbol)
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ss", &type, &type_len
		, &symbol, &symbol_len) == FAILURE)
		EXCEPTION_ABORT("Cannot parse parameters");
	ALLOC_INIT_ZVAL(zkey);
	Automap_key(type[0],symbol,symbol_len,zkey TSRMLS_CC);
	hash=ZSTRING_HASH(zkey);
	pep=Automap_Pmap_find_key(mp->map,zkey,hash TSRMLS_CC);
	ut_ezval_ptr_dtor(&zkey);
	if (!pep) RETURN_FALSE;

	Automap_Instance_export_entry(mp,pep,return_value TSRMLS_CC);
}

/* }}} */
/*---------------------------------------------------------------*/

static unsigned long Automap_symbol_count(Automap_Mnt *mp TSRMLS_DC)
{
	return zend_hash_num_elements(Z_ARRVAL_P(mp->map->zsymbols));
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
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	DBG_MSG1("Entering AUTOMAP::option(%s)", opt);

	if (zend_hash_find(Z_ARRVAL_P(mp->map->zoptions), opt, optlen + 1,
		(void **)(&zpp)) == SUCCESS) {
		RETVAL_BY_VAL(*zpp);
	}
}

/* }}} */
/*---------------------------------------------------------------*/

ZEND_BEGIN_ARG_INFO_EX(Automap_mount_arginfo, 0, 1, 1)
ZEND_ARG_INFO(0, path)
ZEND_ARG_INFO(0, base_dir)
ZEND_ARG_INFO(0, mnt)
ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(Automap_autoload_hook_arginfo, 0, 0, 1)
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
	PHP_MALIAS(Automap, unload, umount, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, instance, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, mnt_list, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, path_id, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, mount, Automap_mount_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_MALIAS(Automap, load, mount, Automap_mount_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, register_failure_handler, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, register_success_handler, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, type_to_string, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, string_to_type, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, key, UT_2args_arginfo,
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
	PHP_ME(Automap, min_map_version, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, mnt, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, path, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, base_dir, UT_noarg_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, flags, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, symbols, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, options, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, version, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, min_version, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, get_symbol, UT_2args_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, symbol_count, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, option, UT_1arg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, is_valid, UT_noarg_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(Automap, using_accelerator, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(Automap, accel_techinfo, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/

static void Automap_Instance_set_constants(zend_class_entry * ce)
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

/*===============================================================*/

static int MINIT_Automap_Instance(TSRMLS_D)
{
	zend_class_entry ce, *entry;

	INIT_CLASS_ENTRY(ce, "Automap", Automap_functions);
	entry = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_property_null(entry,"m",1,ZEND_ACC_PRIVATE TSRMLS_CC);
	Automap_Instance_set_constants(entry);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Instance(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Instance(TSRMLS_D)
{
	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Instance(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
