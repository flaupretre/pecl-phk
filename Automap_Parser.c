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
/* {{{ proto string _automap_file_get_contents(string path) */

static PHP_NAMED_FUNCTION(Automap_Ext_file_get_contents)
{
	char *path,*buf=NULL;
	int pathlen;
	FILE *fp;
	struct stat st;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s",&path,&pathlen
		)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	fp=fopen(path,"rb");
	if (!fp) {
		EXCEPTION_ABORT_1("%s: Cannot open file",path);
	}
	fstat(fileno(fp),&st);
	if (!S_ISREG(st.st_mode)) EXCEPTION_ABORT_1("%s: File is not a regular file",path);
	EALLOCATE(buf,st.st_size+1);
	while (!fread(buf,st.st_size,1,fp)) {}
	buf[st.st_size]='\0';
	fclose(fp);
	RETVAL_STRINGL(buf,st.st_size,0);
}

/*---------------------------------------------------------------*/
/* {{{ proto array _automap_parse_tokens(string buf, bool skip_blocks) */

static PHP_NAMED_FUNCTION(Automap_Ext_parse_tokens)
{
	zval *zbuf;
	zend_bool skip_blocks;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "zb",&zbuf,&skip_blocks
		)==FAILURE) EXCEPTION_ABORT("Cannot parse parameters");

	Automap_parse_tokens(zbuf, skip_blocks, return_value TSRMLS_CC);
}

/*---------------------------------------------------------------*/

static void Automap_Parser_add_symbol(zval *arr,char type,char *ns,int nslen
	,char *name,int nalen)
{
	char *key,*p;
	int slen;

	if (nslen) ns=ut_trim_char(ns,&nslen,'\\');

	slen=nalen+1;
	if (nslen) slen += (nslen+1);
	key=ut_eallocate(NULL,slen+1);
	p=key;
	(*(p++))=type;
	if (nslen) {
		memmove(p,ns,nslen);
		p += nslen;
		*(p++)='\\';
	}
	memmove(p,name,nalen);
	p += nalen;
	*(p++)='\0';
	add_next_index_stringl(arr,key,slen,0);
}

/*---------------------------------------------------------------*/

#define INIT_AUTOMAP_PARSE_TOKENS() \
	{ \
	INIT_ZVAL(ztokens); \
	}

#define CLEANUP_AUTOMAP_PARSE_TOKENS() \
	{ \
	ut_ezval_dtor(&ztokens); \
	}

#define RETURN_FROM_AUTOMAP_PARSE_TOKENS() \
	{ \
	CLEANUP_AUTOMAP_PARSE_TOKENS(); \
	return; \
	}

#define ABORT_AUTOMAP_PARSE_TOKENS() \
	{ \
	RETURN_FROM_AUTOMAP_PARSE_TOKENS(); \
	}

#define TVALUE_IS_CHAR(_c)	((tvlen==1) && ((*tvalue)==_c))

static void Automap_parse_tokens(zval *zbuf, int skip_blocks, zval *ret TSRMLS_DC)
{
	int block_level,nalen,nslen,tnum,tvlen;
	char ns[AUTOMAP_SYMBOL_MAX],*tvalue,schar;
	char state;
	zval ztokens,**ztoken, *args[1], **zpp;
	HashTable *ht_tokens,*ht;

	array_init(ret);

	block_level=0;
	state=AUTOMAP_ST_OUT;
	nalen=nslen=0;

	/* Loop on token_get_all() result */

	args[0]=zbuf;
	ut_call_user_function_array(NULL,"token_get_all",13,&ztokens,1,args TSRMLS_CC);
	if (EG(exception)) ABORT_AUTOMAP_PARSE_TOKENS();
	if (!ZVAL_IS_ARRAY(&ztokens)) {
		THROW_EXCEPTION("token_get_all() should return an array");
		ABORT_AUTOMAP_PARSE_TOKENS();
	}
	ht_tokens=Z_ARRVAL(ztokens);
	zend_hash_internal_pointer_reset(ht_tokens);
	while(1) {
		if (zend_hash_has_more_elements(ht_tokens) == FAILURE) break;
		zend_hash_get_current_data(ht_tokens,(void **)(&ztoken));
		zend_hash_move_forward(ht_tokens);
		if (ZVAL_IS_STRING(*ztoken)) {
			tvalue=Z_STRVAL_PP(ztoken);
			tvlen=Z_STRLEN_PP(ztoken);
			tnum=-1;
			}
		else { /* Array */
			ht=Z_ARRVAL_PP(ztoken);
			zend_hash_internal_pointer_reset(ht);
			zend_hash_get_current_data(ht,(void **)(&zpp));
			tnum=Z_LVAL_PP(zpp);
			zend_hash_move_forward(ht);
			zend_hash_get_current_data(ht,(void **)(&zpp));
			tvalue=Z_STRVAL_PP(zpp);
			tvlen=Z_STRLEN_PP(zpp);
			}
			
		if ((tnum==T_COMMENT)||(tnum==T_DOC_COMMENT)) continue;
		if ((tnum==T_WHITESPACE)&&(state!=AUTOMAP_ST_NAMESPACE_FOUND)) continue;

		switch(state) {
			case AUTOMAP_ST_OUT:
				switch(tnum) {
					case T_FUNCTION:
						state=AUTOMAP_ST_FUNCTION_FOUND;
						break;
					case T_CLASS:
					case T_INTERFACE:
					case T_TRAIT:
						state=AUTOMAP_ST_CLASS_FOUND;
						break;
					case T_NAMESPACE:
						state=AUTOMAP_ST_NAMESPACE_FOUND;
						nslen=0;
						break;
					case T_CONST:
						state=AUTOMAP_ST_CONST_FOUND;
						break;
					case T_STRING:
						if (!strcmp(tvalue,"define")) state=AUTOMAP_ST_DEFINE_FOUND;
						nalen=0;
						break;
					// If this flag is set, we skip anything enclosed
					// between {} chars, ignoring any conditional block.
					case -1:
						if (TVALUE_IS_CHAR('{') && skip_blocks) {
							state=AUTOMAP_ST_SKIPPING_BLOCK_NOSTRING;
							block_level=1;
							}
						break;
					}
				break;

			case AUTOMAP_ST_NAMESPACE_FOUND:
				state=(tnum==T_WHITESPACE) ? AUTOMAP_ST_NAMESPACE_2 : AUTOMAP_ST_OUT;
				break;
				
			case AUTOMAP_ST_NAMESPACE_2:
				switch(tnum) {
					case T_STRING:
						if ((nslen+tvlen)<AUTOMAP_SYMBOL_MAX) {
							memmove(ns+nslen,tvalue,tvlen);
							nslen+=tvlen;
						}
						break;
					case T_NS_SEPARATOR:
						ns[nslen++]='\\'; //PROT
						break;
					default:
						state=AUTOMAP_ST_OUT;
					}
				break;
				

			case AUTOMAP_ST_FUNCTION_FOUND:
				if ((tnum==-1) && TVALUE_IS_CHAR('(')) {
					// Closure : Ignore (no function name to get here)
					state=AUTOMAP_ST_OUT;
					break;
					}
				 //-- Function returning ref: keep looking for name
				 if ((tnum==-1) && TVALUE_IS_CHAR('&')) break;
				// No break here !
			case AUTOMAP_ST_CLASS_FOUND:
				if (tnum==T_STRING) {
					Automap_Parser_add_symbol(ret,state,ns,nslen,tvalue,tvlen);
					}
				else {
					THROW_EXCEPTION_2("Unrecognized token for class/function definition (type=%d;value='%s'). String expected",tnum,tvalue);
					ABORT_AUTOMAP_PARSE_TOKENS();
					}
				state=AUTOMAP_ST_SKIPPING_BLOCK_NOSTRING;
				block_level=0;
				break;

			case AUTOMAP_ST_CONST_FOUND:
				if (tnum==T_STRING) {
					Automap_Parser_add_symbol(ret,AUTOMAP_T_CONSTANT,ns,nslen,tvalue,tvlen);
					}
				else {
					THROW_EXCEPTION_2("Unrecognized token for constant definition (type=%d;value='%s'). String expected",tnum,tvalue);
					ABORT_AUTOMAP_PARSE_TOKENS();
					}
				state=AUTOMAP_ST_OUT;
				break;

			case AUTOMAP_ST_SKIPPING_BLOCK_STRING:
				if (tnum==-1 && TVALUE_IS_CHAR('"')) {
					state=AUTOMAP_ST_SKIPPING_BLOCK_NOSTRING;
				}
				break;

			case AUTOMAP_ST_SKIPPING_BLOCK_NOSTRING:
				if (((tnum==-1) || (tnum==T_CURLY_OPEN)) && (tvlen==1)) {
					switch(tvalue[0]) {
						case '"':
							state=AUTOMAP_ST_SKIPPING_BLOCK_STRING;
							break;
						case '{':
							block_level++;
							//TRACE echo "block_level=block_level\n";
							break;
						case '}':
							block_level--;
							if (block_level==0) state=AUTOMAP_ST_OUT;
							//TRACE echo "block_level=block_level\n";
							break;
						}
					}
				break;

			case AUTOMAP_ST_DEFINE_FOUND:
				if (tnum==-1 && TVALUE_IS_CHAR('(')) state=AUTOMAP_ST_DEFINE_2;
				else {
					THROW_EXCEPTION_2("Unrecognized token for constant definition (type=%d;value='%s'). Expected '('",tnum,tvalue);
					ABORT_AUTOMAP_PARSE_TOKENS();
					}
				break;

			case AUTOMAP_ST_DEFINE_2:
				// Remember: T_STRING is incorrect in 'define' as constant name.
				// Current namespace is ignored in 'define' statement.
				if (tnum==T_CONSTANT_ENCAPSED_STRING) {
					schar=tvalue[0];
					if (schar=='\'' || schar=='"') { /* trim(tvalue,schar) */
						tvalue=ut_trim_char(tvalue,&tvlen,schar);
						}
					Automap_Parser_add_symbol(ret,AUTOMAP_T_CONSTANT,"",0,tvalue,tvlen);
					}
				else {
					THROW_EXCEPTION_2("Unrecognized token for constant definition (type=%d;value='%s'). Expected quoted string constant",tnum,tvalue);
					ABORT_AUTOMAP_PARSE_TOKENS();
					}
				state=AUTOMAP_ST_SKIPPING_TO_EOL;
				break;

			case AUTOMAP_ST_SKIPPING_TO_EOL:
				if ((tnum==-1) && TVALUE_IS_CHAR(';')) state=AUTOMAP_ST_OUT;
				break;
			}
		}
RETURN_FROM_AUTOMAP_PARSE_TOKENS();
}

/*===============================================================*/

static int MINIT_Automap_Parser(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int MSHUTDOWN_Automap_Parser(TSRMLS_D)
{
	return SUCCESS;
}

/*---------------------------------------------------------------*/

static int RINIT_Automap_Parser(TSRMLS_D)
{
	return SUCCESS;
}
/*---------------------------------------------------------------*/

static int RSHUTDOWN_Automap_Parser(TSRMLS_D)
{
	return SUCCESS;
}

/*===============================================================*/
