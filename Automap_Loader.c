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
/* {{{ proto void Automap::autoload_hook(string symbol [, string type ]) */

static PHP_METHOD(Automap, autoload_hook)
{
	char *symbol,*type;
	int slen,tlen;

	type=NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s|s",&symbol, &slen
		,&type,&tlen)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	DBG_MSG1("Starting Automap::autoload_hook(%s)",symbol);

	Automap_resolve_symbol((type ? (*type) : AUTOMAP_T_CLASS), symbol
		, slen, 1, 0 TSRMLS_CC);

	DBG_MSG("Ending Automap::autoload_hook");
}

/* }}} */
/*---------------------------------------------------------------*/
/* Run  spl_autoload_register('Automap::autoload_hook'); */

static void Automap_Loader_register_hook(TSRMLS_D)
{
	zval *zp;

	MAKE_STD_ZVAL(zp);
	ZVAL_STRINGL(zp,"Automap::autoload_hook",22,1);
	ut_call_user_function_void(NULL,ZEND_STRL("spl_autoload_register"),1,&zp TSRMLS_CC);
	ut_ezval_ptr_dtor(&zp);
}

/*---------------------------------------------------------------*/
/* Returns SUCCESS/FAILURE */

static int Automap_resolve_symbol(char type, char *symbol, int slen, int autoload
	, int exception TSRMLS_DC)
{
	zval *zkey;
	unsigned long hash;
	char *ts;
	int i;
	Automap_Mnt *mp;

	DBG_MSG2("Starting Automap_resolve_symbol(%c,%s)",type,symbol);

	/* If executed from the autoloader, no need to check for symbol existence */
	if ((!autoload) && Automap_symbol_is_defined(type,symbol,slen TSRMLS_CC)) {
		return 1;
	}

	if ((i=AUTOMAP_G(mcount))==0) return 0;

	ALLOC_INIT_ZVAL(zkey);
	Automap_key(type,symbol,slen,zkey TSRMLS_CC);

	hash=ZSTRING_HASH(zkey);

	while ((--i) >= 0) {
		mp=AUTOMAP_G(mount_order)[i];
		if (!mp) continue;
		if (Automap_Mnt_resolve_key(mp, zkey, hash TSRMLS_CC)==SUCCESS) {
			DBG_MSG2("Found key %s in map %s",Z_STRVAL_P(zkey),Z_STRVAL_P(mp->map->zmnt));
			ut_ezval_ptr_dtor(&zkey);
			return SUCCESS;
		}
	}

	Automap_call_failure_handlers(type, symbol, slen TSRMLS_CC);

	if ((exception)&&(!EG(exception))) {
		ts=Automap_type_to_string(type TSRMLS_CC);
		THROW_EXCEPTION_2("Automap: Unknown %s: %s",ts,symbol);
	}

	ut_ezval_ptr_dtor(&zkey);
	return FAILURE;
}

/*---------------------------------------------------------------*/

#define AUTOMAP_GET_FUNCTION(_name,_type) \
	AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,get,0)

#define AUTOMAP_REQUIRE_FUNCTION(_name,_type) \
	AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,require,1)

#define AUTOMAP_GET_REQUIRE_FUNCTION(_name,_type,_gtype,_exception) \
	static PHP_METHOD(Automap, _gtype ## _ ## _name) \
	{ \
		char *symbol; \
		int slen; \
 \
		if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&symbol \
			, &slen)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters"); \
 \
		RETURN_BOOL(Automap_resolve_symbol(_type, symbol, slen \
			, 0, _exception TSRMLS_CC)==SUCCESS); \
	}

AUTOMAP_GET_FUNCTION(function,AUTOMAP_T_FUNCTION)
AUTOMAP_GET_FUNCTION(constant,AUTOMAP_T_CONSTANT)
AUTOMAP_GET_FUNCTION(class,AUTOMAP_T_CLASS)
AUTOMAP_GET_FUNCTION(extension,AUTOMAP_T_EXTENSION)

AUTOMAP_REQUIRE_FUNCTION(function,AUTOMAP_T_FUNCTION)
AUTOMAP_REQUIRE_FUNCTION(constant,AUTOMAP_T_CONSTANT)
AUTOMAP_REQUIRE_FUNCTION(class,AUTOMAP_T_CLASS)
AUTOMAP_REQUIRE_FUNCTION(extension,AUTOMAP_T_EXTENSION)

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
	Automap_Loader_register_hook(TSRMLS_C);

	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Loader(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
