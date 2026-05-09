#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "iword.h"
#include "include/iword.h"


// iword_set — iwordで処理する文字列の設定
void iword_set(char *arg, size_t arg_len, zend_long mode) {
	// グローバル変数の初期化
	iword_initialize();
	// 文字列をコピーする
	str_g = (char *)malloc(arg_len);
	memcpy(str_g, arg, str_len_g = arg_len);
	// 単語マップを取得
	imap_g = iword_map(str_g, str_len_g, mode);
}

// ZValとして与える
void iword_set_zval(zval *arg, zend_long mode)
 { iword_set(Z_STRVAL_P(arg), Z_STRLEN_P(arg), mode | IWORD_MODE_FORBID); }

PHP_FUNCTION(iword_set) {
	zend_long mode = IWORD_MODE_HTML | IWORD_MODE_FORBID;
	char *arg; size_t arg_len;

	// PHPから引数を受け取る
	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "s|l", &arg, &arg_len, &mode) == FAILURE) return;
	// 文字列をコピーする
	iword_set(arg, arg_len, mode);
}

// iword_limit ― iwordで処理する限界数の設定
PHP_FUNCTION(iword_limit)
{
	zend_long limit;

	// PHPから引数を受け取る
	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "l", &limit) == FAILURE) return;
	// 限界数の設定
	iword_set_limit(limit);
}

// iword_dictionary ― iwordで処理する辞書の設定
PHP_FUNCTION(iword_dictionary)
{
	char *str; size_t len;

	// PHPから引数を受け取る
	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "s", &str, &len) == FAILURE) return;
	// 限界数の設定
	iword_set_strkey(str, len);
}

// iword_unset — iwordで処理する文字列の解放
PHP_FUNCTION(iword_unset)
 { iword_initialize(); }

// iword_map — 全単語を抽出する
PHP_FUNCTION(iword_map)
{
	int i; zval *src = NULL; zend_long mode = IWORD_MODE_HTML;

	// PHPから引数を受け取る
	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "|zl", &src, &mode) == FAILURE) return;
	// 引数が渡された場合
	if (src != NULL) {
		// 引数が文字列でなければ
		if (Z_TYPE_P(src) == IS_STRING) iword_set_zval(src, mode);
		// 値がTrueであれば中断
		else if (Z_LVAL_P(src)) RETURN_FALSE;
	}
	// 単語マップの取得に失敗した時は元のFalseを返す
	if (imap_g == NULL) RETURN_FALSE;
	// 返すための配列を用意(return_valueはPHPが宣言済み)
	array_init(return_value);
	// return_value配列に順に格納していく
	for (i = 0; imap_g[i]; i++)
	 if ((mode & IWORD_MODE_FORBID) || IWORD_PUBLIC(imap_g[i]))
	  add_index_long(return_value, imap_g[i] >> 16, imap_g[i] & 0xff);
}

void iword_get_key(zval *return_value, zval *src, int key, zend_long mode) {
	int i;

	// 引数が渡された場合
	if (src != NULL) {
		// 引数が文字列でなければ
		if (Z_TYPE_P(src) == IS_STRING) iword_set_zval(src, mode);
		// 値がTrueであれば中断
		else if (Z_LVAL_P(src)) RETURN_FALSE;
	}
	// 単語マップの取得に失敗した時はFalseを返す
	if (imap_g == NULL) RETURN_FALSE;
	// 返すための配列を用意(return_valueはPHPが宣言済み)
	array_init(return_value);
	// return_value配列に順に格納していく
	for (i = 0, key <<= 8; imap_g[i]; i++)
	 if ((imap_g[i] & 0xff00) == key)
	  add_index_long(return_value, imap_g[i] >> 16, imap_g[i] & 0xff);
}

// iword_get_key — 特定グループの単語を抽出する
PHP_FUNCTION(iword_get_key)
{
	zend_long key; zval *src = NULL; zend_long mode = IWORD_MODE_HTML;

	// PHPから引数を受け取る
	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "l|zl", &key, &src, &mode) == FAILURE) return;
	// スパム語を抜き取る
	iword_get_key(return_value, src, key, mode);
}

// iword_get_spam — SPAM単語を抽出する
PHP_FUNCTION(iword_get_spam)
{
	zval *src = NULL; zend_long mode = IWORD_MODE_HTML;

	// PHPから引数を受け取る
	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "|zl", &src, &mode) == FAILURE) return;
	// スパム語を抜き取る
	iword_get_key(return_value, src, IWORD_KEY_SPAM, mode);
}

// iword_get_adult — アダルト単語を抽出する
PHP_FUNCTION(iword_get_adult)
{
	zval *src = NULL; zend_long mode = IWORD_MODE_HTML;

	// PHPから引数を受け取る
	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "|zl", &src, &mode) == FAILURE) return;
	// スパム語を抜き取る
	iword_get_key(return_value, src, IWORD_KEY_ADULT, mode);
}

PHP_FUNCTION(iword_exists)
{
	zend_long key = 9;

	if (zend_parse_parameters(ZEND_NUM_ARGS(),
	 "|l", &key) == FAILURE) return;
	if (iword_mask() & (1 << key)) RETURN_TRUE;
	RETURN_FALSE;
}
