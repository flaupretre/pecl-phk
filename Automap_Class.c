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
/* {{{ proto string Automap::__construct() */
/* This private method is never called. It exists only to prevent instanciation
* from user code using a 'new Automap' directive */

static PHP_METHOD(Automap, __construct) {}

/*---------------------------------------------------------------*/

static zval *Automap_map_object_by_mp(Automap_Mnt *mp TSRMLS_DC)
{
	zval *args[2],*flags_zp;
	
	if (!mp->map_object) {
		args[0]=mp->zpath;
		MAKE_STD_ZVAL(flags_zp);
		ZVAL_LONG(flags_zp,mp->flags);
		args[1]=flags_zp;
		mp->map_object=ut_new_instance(ZEND_STRL("Automap_Map"), YES, 2
			, args TSRMLS_CC);
		ut_ezval_ptr_dtor(&flags_zp);
	}

	return mp->map_object;
}

/*---------------------------------------------------------------*/

static zval *Automap_map(long id TSRMLS_DC)
{
	Automap_Mnt *mp;

	mp = Automap_Mnt_get(id, 1 TSRMLS_CC);
	if (EG(exception)) return NULL;

	return Automap_map_object_by_mp(mp TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* {{{ proto Automap_Map Automap::map(long id) */

static PHP_METHOD(Automap, map)
{
	zval *zid;
	long id;
	Automap_Mnt *mp;
	zval *map_object;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &zid) ==
		FAILURE) EXCEPTION_ABORT("Cannot parse parameters");
	convert_to_long(zid);
	id=Z_LVAL_P(zid);

	mp = Automap_Mnt_get(id, 1 TSRMLS_CC);
	if (EG(exception)) return;

	map_object = Automap_map(id TSRMLS_CC);
	if (EG(exception)) return;

	RETVAL_BY_REF(map_object);
}

/* }}} */
/*---------------------------------------------------------------*/

ZEND_BEGIN_ARG_INFO_EX(Automap_load_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, path)
ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(Automap_autoload_hook_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, symbol)
ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

static zend_function_entry Automap_functions[] = {
	PHP_ME(Automap, __construct, UT_noarg_arginfo, ZEND_ACC_PRIVATE)
	PHP_ME(Automap, register_failure_handler, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, register_success_handler, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, key, UT_2args_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, type_to_string, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, string_to_type, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, id_is_active, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, map, UT_1arg_ref_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, active_ids, UT_noarg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, load, Automap_load_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, unload, UT_1arg_arginfo,
		   ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME(Automap, using_accelerator, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
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
	PHP_ME(Automap, accel_techinfo, UT_noarg_arginfo,
		   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL, 0, 0}
};

/*---------------------------------------------------------------*/

static void Automap_Class_set_constants(zend_class_entry * ce)
{
	UT_DECLARE_STRING_CONSTANT(AUTOMAP_VERSION,"VERSION");
	UT_DECLARE_STRING_CONSTANT(AUTOMAP_MIN_MAP_VERSION,"MIN_MAP_VERSION");
	UT_DECLARE_STRING_CONSTANT(AUTOMAP_MAGIC,"MAGIC");

	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_FUNCTION,"T_FUNCTION");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_CONSTANT,"T_CONSTANT");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_CLASS,"T_CLASS");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_T_EXTENSION,"T_EXTENSION");

	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_F_SCRIPT,"F_SCRIPT");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_F_EXTENSION,"F_EXTENSION");
	UT_DECLARE_CHAR_CONSTANT(AUTOMAP_F_PACKAGE,"F_PACKAGE");

	UT_DECLARE_LONG_CONSTANT(AUTOMAP_FLAG_NO_AUTOLOAD,"NO_AUTOLOAD");
	UT_DECLARE_LONG_CONSTANT(AUTOMAP_FLAG_CRC_CHECK,"CRC_CHECK");
	UT_DECLARE_LONG_CONSTANT(AUTOMAP_FLAG_PECL_LOAD,"PECL_LOAD");

}

/*===============================================================*/

static int MINIT_Automap_Class(TSRMLS_D)
{
	zend_class_entry ce, *entry;

	INIT_CLASS_ENTRY(ce, "Automap", Automap_functions);
	entry = zend_register_internal_class(&ce TSRMLS_CC);

	Automap_Class_set_constants(entry);

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Class(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Class(TSRMLS_D)
{
	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Class(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
