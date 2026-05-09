/* iWord PECL Library */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_iword.h"
#include "include/iword.h"

static int le_iword;

// 関数の登録 PHP_FE(関数, 参照)
const zend_function_entry iword_functions[] = {
	PHP_FE(iword_set, NULL)
	PHP_FE(iword_unset, NULL)
	PHP_FE(iword_limit, NULL)
	PHP_FE(iword_dictionary, NULL)
	PHP_FE(iword_map, NULL)
	PHP_FE(iword_get_key, NULL)
	PHP_FE(iword_get_spam, NULL)
	PHP_FE(iword_get_adult, NULL)
	PHP_FE(iword_exists, NULL)
	PHP_FE_END
};

/*
	// マップ用メモリ
	long long *imap;
	// 読み込み文字列とその長さ
	char *str; int str_len;
//*/

ZEND_DECLARE_MODULE_GLOBALS(iword)
// グローバル変数化

#define imap_g (IWORD_G(imap_global))
#define str_g (IWORD_G(str_global))
#define str_len_g (IWORD_G(str_len_global))

zend_module_entry iword_module_entry = {
	STANDARD_MODULE_HEADER,
	"iword",
	iword_functions,
	NULL,
	NULL,
	PHP_RINIT(iword),
	PHP_RSHUTDOWN(iword),
	PHP_MINFO(iword),
	IWORD_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_IWORD
ZEND_GET_MODULE(iword)
#endif

// マップ用メモリの初期化
void iword_initialize() {
	if (imap_g) free(imap_g), imap_g = NULL;
	if (str_g) free(str_g), str_g = NULL, str_len_g = 0;
}

PHP_RINIT_FUNCTION(iword)
 { imap_g = NULL; str_g = NULL; str_len_g = 0; }

PHP_RSHUTDOWN_FUNCTION(iword)
 { iword_initialize(); }

PHP_MINFO_FUNCTION(iword)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "iword support", "enabled");
	php_info_print_table_header(2, "iword version", IWORD_VERSION);
	php_info_print_table_end();
}
