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

	type=NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s|s",&symbol, &slen
		,&type,&tlen)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	DBG_MSG1("Starting \\Automap\\Mgr::autoloadHook(%s)",symbol);

	Automap_resolve_symbol((type ? (*type) : AUTOMAP_T_CLASS), symbol
		, slen, 1, 0 TSRMLS_CC);

	DBG_MSG("Ending \\Automap\\Mgr::autoloadHook");
}

/* }}} */
/*---------------------------------------------------------------*/
/* Run  spl_autoload_register('\Automap\Mgr::autoloadHook'); */

static void Automap_Loader_register_hook(TSRMLS_D)
{
	zval *zp;

	MAKE_STD_ZVAL(zp);
	ZVAL_STRINGL(zp,"Automap\\Mgr::autoloadHook",25,1);
	ut_call_user_function_void(NULL,ZEND_STRL("spl_autoload_register"),1,&zp TSRMLS_CC);
	ut_ezval_ptr_dtor(&zp);
}

/*---------------------------------------------------------------*/
/* Returns SUCCESS/FAILURE */

static int Automap_resolve_symbol(char type, char *symbol, int slen, zend_bool autoload
	, zend_bool exception TSRMLS_DC)
{
	zval zkey;
	unsigned long hash;
	char *ts;
	int i;
	Automap_Mnt *mp;

	DBG_MSG2("Starting Automap_resolve_symbol(%c,%s)",type,symbol);

	/* If executed from the autoloader, no need to check for symbol existence */
	if ((!autoload) && Automap_symbol_is_defined(type,symbol,slen TSRMLS_CC)) {
		return 1;
	}

	if ((i=PHK_G(map_count))==0) return 0;

	INIT_ZVAL(zkey);
	Automap_key(type,symbol,slen,&zkey TSRMLS_CC);

	hash=ZSTRING_HASH(&zkey);

	while ((--i) >= 0) {
		mp=PHK_G(map_array)[i];
		if ((!mp)||(mp->flags & AUTOMAP_FLAG_NO_AUTOLOAD)) continue;
		if (Automap_Mnt_resolve_key(mp, &zkey, hash TSRMLS_CC)==SUCCESS) {
			DBG_MSG2("Found key %s in map %ld",Z_STRVAL(zkey),mp->id);
			ut_ezval_dtor(&zkey);
			return SUCCESS;
		}
	}

	Automap_callFailureHandlers(type, symbol, slen TSRMLS_CC);

	if ((exception)&&(!EG(exception))) {
		ts=Automap_typeToString(type TSRMLS_CC);
		THROW_EXCEPTION_2("Automap: Unknown %s: %s",ts,symbol);
	}

	ut_ezval_dtor(&zkey);
	return FAILURE;
}

/*---------------------------------------------------------------*/
/* {{{ proto void \Automap\Mgr::resolve(string type, string symbol [, bool autoload, bool exception]) */

static PHP_METHOD(Automap, resolve)
{
	char *symbol,*type;
	int slen,tlen;
	zend_bool autoload = 0, exception = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ss|bb",&symbol, &slen
		,&type,&tlen,&autoload,&exception)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	RETURN_BOOL(Automap_resolve_symbol(*type, symbol, slen, autoload
		, exception TSRMLS_CC)==SUCCESS);
}

/* }}} */
/*---------------------------------------------------------------*/

#define AUTOMAP_GET_FUNCTION(_name,_type) \
	AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,get,0)

#define AUTOMAP_REQUIRE_FUNCTION(_name,_type) \
	AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,require,1)

#define AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,_gtype,_exception) \
	static PHP_METHOD(Automap, _gtype ## _name) \
	{ \
		char *symbol; \
		int slen; \
		zend_bool autoload = 0; \
 \
		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s|b",&symbol \
			, &slen,&autoload)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters"); \
 \
		RETURN_BOOL(Automap_resolve_symbol(_type, symbol, slen \
			, autoload, _exception TSRMLS_CC)==SUCCESS); \
	}

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
