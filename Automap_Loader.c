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
/* {{{ proto void \Automap\Mgr::autoloadHook(string symbol [, string type ]) */

static PHP_METHOD(Automap, autoloadHook)
{
	char *symbol,*type;
	int slen,tlen;
	zend_string *zs;

	type=NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s|s",&symbol, &slen
		,&type,&tlen)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	DBG_MSG1("Starting \\Automap\\Mgr::autoloadHook(%s)",symbol);

	zs=zend_string_init(symbol,slen,0);
	Automap_resolve_symbol((type ? (*type) : AUTOMAP_T_CLASS), zs, 1, 0 TSRMLS_CC);
	zend_string_release(zs);

	DBG_MSG("Ending \\Automap\\Mgr::autoloadHook");
}

/* }}} */
/*---------------------------------------------------------------*/
/* Run  spl_autoload_register('\Automap\Mgr::autoloadHook'); */

static void Automap_Loader_register_hook(TSRMLS_D)
{
	zval zv;

	INIT_ZVAL(zv);
	ZVAL_STRINGL(&zv,ZEND_STRL("Automap\\Mgr::autoloadHook"),0);
	ut_call_user_function_void(NULL,ZEND_STRL("spl_autoload_register"),1,&zv TSRMLS_CC);
}

/*---------------------------------------------------------------*/
/* Returns SUCCESS/FAILURE */

static int Automap_resolve_symbol(char type, zend_string *symbol, int autoload
	, int exception TSRMLS_DC)
{
	zend_string *key;
	char *ts;
	int i;
	Automap_Mnt *mp;

	DBG_MSG2("Starting Automap_resolve_symbol(%c,%s)",type,ZSTR_VAL(symbol));

	/* If executed from the autoloader, no need to check for symbol existence */
	if ((!autoload) && Automap_symbol_is_defined(type,symbol TSRMLS_CC)) {
		return 1;
	}

	if ((i=PHK_G(map_count))==0) return 0;

	key=Automap_key(type,symbol TSRMLS_CC);

	while ((--i) >= 0) {
		mp=PHK_G(map_array)[i];
		if (!mp) continue;
		if (Automap_Mnt_resolve_key(mp, key TSRMLS_CC)==SUCCESS) {
			DBG_MSG2("Found key %s in map %ld",ZSTR_VAL(key),mp->id);
			zend_string_release(key);
			return SUCCESS;
		}
	}

	Automap_callFailureHandlers(type, symbol TSRMLS_CC);

	if ((exception)&&(!EG(exception))) {
		ts=Automap_type_to_string(type TSRMLS_CC);
		THROW_EXCEPTION_2("Automap: Unknown %s: %s",ts,ZSTR_VAL(symbol));
	}

	zend_string_release(key);
	return FAILURE;
}

/*---------------------------------------------------------------*/

#define AUTOMAP_GET_FUNCTION(_name,_type) \
	AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,get,0)

#define AUTOMAP_REQUIRE_FUNCTION(_name,_type) \
	AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,require,1)

#ifdef PHPNG
#define AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,_gtype,_exception) \
	static PHP_METHOD(Automap, _gtype ## _name) \
	{ \
		int status; \
		zend_string *zs; \
 \
 		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S",&zs \
			)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters"); \
 \
		status=Automap_resolve_symbol(_type, zs, 0, _exception TSRMLS_CC); \
		RETURN_BOOL(status==SUCCESS); \
	}
#else
#define AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,_gtype,_exception) \
	static PHP_METHOD(Automap, _gtype ## _name) \
	{ \
		char *symbol; \
		int slen, status; \
		zend_string *zs; \
 \
 		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol \
			, &slen)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters"); \
 \
		zs=zend_string_init(symbol,slen,0); \
		status=Automap_resolve_symbol(_type, zs, 0, _exception TSRMLS_CC); \
		zend_string_release(zs); \
		RETURN_BOOL(status==SUCCESS); \
	}
#endif

AUTOMAP_GET_FUNCTION(Function,AUTOMAP_T_FUNCTION)
AUTOMAP_GET_FUNCTION(Constant,AUTOMAP_T_CONSTANT)
AUTOMAP_GET_FUNCTION(Class,AUTOMAP_T_CLASS)
AUTOMAP_GET_FUNCTION(Extension,AUTOMAP_T_EXTENSION)

AUTOMAP_REQUIRE_FUNCTION(Function,AUTOMAP_T_FUNCTION)
AUTOMAP_REQUIRE_FUNCTION(Constant,AUTOMAP_T_CONSTANT)
AUTOMAP_REQUIRE_FUNCTION(Class,AUTOMAP_T_CLASS)
AUTOMAP_REQUIRE_FUNCTION(Extension,AUTOMAP_T_EXTENSION)

/*===============================================================*/

static int MINIT_Automap_Loader(TSRMLS_D)
{
	/*--- Check that SPL is present */

	if (!ut_extension_loaded("spl",3 TSRMLS_CC)) {
		THROW_EXCEPTION("Automap requires the SPL extension");
		return FAILURE;
	}

	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Loader(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Loader(TSRMLS_D)
{
	if (PHK_G(ext_is_enabled)) {
		Automap_Loader_register_hook(TSRMLS_C);
	}

	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Loader(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
