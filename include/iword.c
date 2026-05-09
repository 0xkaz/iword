#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include "iword.h"

// #define IWORD_MMX

#ifdef IWORD_MMX
#include <mmintrin.h>
#include <emmintrin.h>

typedef union {
	short i[8];
	__m128i v;
} int128;

int init_mmx = 0;
int128 atm[256];
int128 mul =
 {{0x8123, 0x4843, 0x9837, 0x5123, 0x7483, 0x9547, 0x7473, 0x9821}},
 add =
 {{0x8371, 0x8923, 0x74a3, 0x93a7, 0x74d9, 0x3813, 0x1829, 0x3903}};
#endif

typedef struct ihash {
	unsigned long long a, b;
	char f;
#ifdef IWORD_MMX
	char d[15];
#endif
} ihash;

int iword_size, iword_bcount, iword_needed, iword_limit, iword_key = IWORD_KEY;
char *iword_object = NULL;

// iWordの辞書設定
void iword_set_key(size_t key) {
	iword_key = 0x7fffffff & ((key * 0x75641723) ^ IWORD_KEY);
}

// iWordの辞書設定
void iword_set_strkey(char *str, size_t len) {
	int i = 0, c = IWORD_KEY;
	for (; i < len; i++)
	 c = (c * 0x37145713 ^ 0x83337618) + str[i];
	iword_key = c & 0x7fffffff;
}

// iWordの単語数制限関数
void iword_set_limit(int num) {
	iword_limit = num;
}

// iWord初期化関数
void iword_construct() {
#ifdef IWORD_MMX
	int128 a, b; int i;
	if (init_mmx) return;
	memset(&a.v, 0, 16); memset(&b.v, 1, 16);
	for (i = 0; i < 256; i++) {
		atm[i].v = a.v;
		a.v = _mm_add_epi8(a.v, b.v);
	}
	init_mmx = 1;
	int out;
	__asm__ __volatile__(
	"#ASM START(%1)\n\t"
	"movdqa (%1), %%xmm6\n\t"
	"movdqa (%2), %%xmm7\n\t"
	"#ASM END\n\t":"=r"(out):"r"(&mul),"r"(&add));
#endif
}

// 最後にshmgetしようとしたメモリサイズを返す
int iword_needed_size() {
	return iword_needed;
}

// 既存iWordオブジェクトの解放
void iword_free() {
	// iWordオブジェクトが存在すれば解放し，オブジェクトをNULLにする
	if (iword_object) shmdt(iword_object), iword_object = NULL;
}

// 既存iWordオブジェクトのロード
int iword_alloc() {
	int shmid;
	
	iword_construct();
	// iWord共有メモリのIDを呼び出す
	shmid = shmget(iword_key, 0, 0);
	// 共有メモリが存在しなければ-1を返す
	if (shmid == -1) return -1;
	// 共有メモリの内容とリンクする
	iword_object = (char *)shmat(shmid, 0, SHM_RDONLY);
	// 権限がない等の理由でリンクに失敗した場合-2を返す
	if (iword_object == (char *)-1)
	 return iword_object = NULL, -2;
	// 成功しているので0を返す
	return 0;
}

// ハッシュリストの大きさの計算
int iword_len() {
	unsigned int *ol = (unsigned int *)iword_object;
	return (int)((ol[*ol] >> 8) + (ol[*ol] & 255) + 1) * 8;
}

// マスクの取得
int iword_mask() {
	unsigned int *ol; int mask;
	// 初期化
	if (iword_alloc()) return 0;
	// iWordオブジェクトを取得
	ol = (unsigned int *)iword_object;
	// マスクを取得
	mask = (int)((unsigned long long *)iword_object)
	 [(int)((ol[*ol] >> 8) + (ol[*ol] & 255))];
	// 後始末
	iword_free();
	// マスクを返す
	return mask;
}

// iWordオブジェクトの内容をコピー
char *iword_data() {
	char *o;
	
	// iWordオブジェクトの取得
	if (iword_alloc()) return NULL;
	// コピー先の領域確保
	o = (char *)malloc(iword_len());
	// 領域確保に成功していればiWordオブジェクトをコピー
	if (o) memcpy(o, iword_object, iword_len());
	// iWordオブジェクトを解放し，コピーを返す
	return iword_free(), o;
}

// 次のハッシュ値を得る関数
void iword_nexthash(ihash *h, char c)
{
#ifndef IWORD_MMX
	//      0x1234567890123456
	h->a *= 0x41f93a761943ba01ULL;
	h->a += (unsigned long long)c
	      * 0xa37b830b1a837477ULL;
	h->b *= 0x97361b83a957c129ULL;
	h->b += (unsigned long long)c
	      * 0x19b4f81749201013ULL;
#else
	int out;
	__asm__ __volatile__ (
	"movdqa (%2), %%xmm1\n\t"
	"movdqa (%1), %%xmm0\n\t"
	"pmullw %%xmm7, %%xmm1\n\t"
	"pmullw %%xmm6, %%xmm0\n\t"
	"paddw %%xmm1, %%xmm0\n\t"
	"movdqa %%xmm0, (%1)\n\t"
	:"=r"(out)
	:"r"(&((int128 *)&(h->a))->v),
	"r"(&atm[(unsigned int)c].v)
	:"%xmm0","%xmm1");
	/*
	((int128 *)&(h->a))->v = _mm_add_epi16(
	 _mm_mullo_epi16(((int128 *)&(h->a))->v, mul.v),
	 _mm_mullo_epi16(atm[(unsigned int)c].v, add.v));
	*/
#endif
}

// ハッシュの生成
void iword_hash(ihash *h, char *s) {
	for (h->a = h->b = 0ULL; *s; ++s)
	 iword_nexthash(h, *s);
}

// ハッシュ比較関数
int iword_hashcmp(const void *va, const void *vb) {
	ihash *a, *b; unsigned long long aa, ba, ab, bb;
	
	// 比較するハッシュを取得
	a = (ihash *)va; b = (ihash *)vb;
	// ハッシュの前半部分(iword_bcount種)を比較
	aa = a->a % (unsigned long long)iword_bcount;
	ba = b->a % (unsigned long long)iword_bcount;
	if (aa != ba) return (aa > ba) ? 1 : -1;
	// ハッシュの後半部分(60ビット)を比較
	ab = a->b >> 4; bb = b->b >> 4;
	if (ab != bb) return (ab > bb) ? 1 : -1;
	// フラグを比較
	if (a->f != b->f) return (a->f > b->f) ? 1 : -1;
	// 同一である(辞書に重複がある)
	return 0;
}

// iWord共有メモリの解放
int iword_unload() {
	int shmid;
	
	// 共有メモリ識別番号を取得
	shmid = shmget(iword_key, 0, 0);
	// 共有メモリの取得に失敗したら1を返す
	if (shmid == -1) return 1;
	// 強制的に解除(失敗時は1，成功時は0)
	return shmctl(shmid, IPC_RMID, 0) == -1;
}

// iWord共有メモリの取得
int iword_shmget(char *s, int size) {
	int shmid; char *m;
	
	// 既存共有メモリがあれば解放
	iword_unload();
	// 要求するメモリ量を記録(デバッグ用)
	iword_needed = size;
	// 共有メモリ識別番号を取得
	shmid = shmget(iword_key, size, 0644 | IPC_CREAT);
	// 失敗時は0以外の値を返す
	if (shmid == -1) return -1;
	// 共有メモリをローカルに割り当てる
	m = (char *)shmat(shmid, 0, 0);
	// 失敗時は0以外の値を返す
	if (m == (char *)-1) return -1;
	// 共有メモリ部分にコピー
	memcpy(m, s, size);
	// メモリを解放
	shmdt(m);
	// 成功
	return 0;
}

// ハッシュリストの生成
// s:辞書化する単語リスト f:単語一つずつのフラグ値
int iword_makelist(char **s, char *f, int size) {
	int i, j, num, msize, ret; ihash *h;
	unsigned int *ol; unsigned long long *os, *op, mask = 0;
	
	iword_construct();
	// 長い単語の中間単語の数を数える
	for (i = 0, msize = size; i < size; ++i)
	 msize += (strlen(s[i]) - 1) / 16;
	// ハッシュ値保存用領域の生成
	h = (ihash *)malloc(sizeof(ihash) * msize);
	// 領域の取得に失敗していたら
	if (h == NULL) return -1;
	// 全ての文字列に対しハッシュ値を生成
	for (i = num = 0; i < size; ++i) {
		// 長い文字列の処理
		char ss[256]; strncpy(ss, s[i], 255); ss[255] = 0;
		// 16n文字毎にフラグ値15のハッシュ値を登録する
		for (j = strlen(s[i]); j = (j - 1) & ~0xf, 0 < j;)
		 ss[j] = 0, iword_hash(&h[num], ss), h[num++].f = 15;
		// 文字列全体に対するハッシュ値を登録する
		iword_hash(&h[num], s[i]), h[num++].f = f[i];
		mask |= 1 << (f[i] & 15);
	}
	// サイズの計算
	iword_size = msize;
	iword_bcount = (msize + IWORD_BSIZE - 1) / IWORD_BSIZE;
	// ハッシュ値をソートする
	qsort(h, msize, sizeof(ihash), iword_hashcmp);
	// 64ビット区切りでメモリ領域を確保
	os = (unsigned long long *)
	 malloc((iword_bcount / 2 + 1 + iword_size) * 8);
	// メモリ領域の確保に失敗していたら
	if (os == NULL) return free(h), -1;
	// ブロック数を格納
	ol = (unsigned int *)os; *ol = iword_bcount;
	// すべてのハッシュ値を格納していく
	op = os + iword_bcount / 2 + 1;
	for (i = 0, num = -1; i < msize; ++i) {
		// ブロックの変わり目であれば
		if ((int)(h[i].a % iword_bcount) != num) {
			// 新しいブロック番号に変更
			// num = h[i].a % iword_bcount;
			// 次のブロックポインタへ移動
			// *(++ol) = (unsigned int)(op - os) << 8;
			do {
				// 次のブロックポインタへ移動
				num++; *(++ol) = (unsigned int)(op - os) << 8;
			// 目的のブロックポインタまで続ける
			} while ((int)(h[i].a % iword_bcount) != num);
		// 重複キーワードは無視をする
		} else if (h[i - 1].a == h[i].a
		 && h[i - 1].b == h[i].b) continue;
		// デバッグ用(個数が怪しい場合に解除)
		// assert((*ol & 0xff) != 0xff);
		// ハッシュ値とフラグを組み合わせて格納
		*(op++) = (h[i].b & (~0xfULL)) | (h[i].f & 0xfULL);
		// 登録個数を増やす
		(*ol)++;
	}
	// 共有メモリーへ割り当てる
	*(op++) = mask;
	//ret = iword_shmget((char *)os,
	// (iword_bcount / 2 + 1 + iword_size) * 8);
	ret = iword_shmget((char *)os, (op - os) * 8);
	// メモリー解放
	free(h); free(os);
	// 結果を返す
	return ret;
}

// 単語リストのロード
int iword_load(char *filename) {
	FILE *fp; long size; int i, j, wordcnt; char *data, **list, *f;

	// ファイルの読み込み
	if (!(fp = fopen(filename, "r"))) return -1;
	// ファイルサイズを取得する
	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	// サイズが不正でないかの確認
	if (size <= 0) return -1;
	// サイズ分だけ領域を取得
	data = (char *)malloc(size + 1);
	// 領域の取得に失敗したら
	if (data == NULL) return fclose(fp), -1;
	// ファイル全体を一気に読み込む
	fread(data, size, 1, fp);
	// ファイルを閉じる
	fclose(fp);
	// 終端部分には一応終端としてNULLを入れる
	data[size] = 0;
	for (i = wordcnt = 0; i < size; i++)
	 if (data[i] == '\n' || !data[i]) data[i] = 0, wordcnt++;
	if (data[i - 1]) wordcnt++;
	// 単語リストを格納する領域を確保
	list = (char **)malloc(sizeof(char *) * wordcnt);
	f = (char *)malloc(sizeof(char) * wordcnt);
	// 領域の取得に失敗したら
	if (list == NULL || f == NULL) return free(data), free(list), free(f), -1;
	// 単語リストを格納する
	for (i = 0, j = 0; i < size; i++, j++) {
		// 拡張のない単語を設定
		f[j] = 9; list[j] = &data[i];
		// 拡張(タブ)を検索し単語の終端までiをずらす
		for (; i < size && data[i]; i++) if (data[i] == '\t') {
			data[i] = 0;
			if (++i < size) {
				f[j] = data[i] & 0xf;
				for (; i < size && data[i]; i++);
				i--;
			}
		}
		// 0文字は禁止
		if (!list[j][0]) j--;
	}
	// 単語リストから辞書を作って終了
	return iword_makelist(list, f, j);
}

// ハッシュ値の探索
int iword_seekhash(ihash *h) {
	int pmid, pmin, pmax; char *o;
	unsigned int *ol; unsigned long long *os, *op;
	
	// オブジェクトのロード
	o = iword_object;
	// ブロックの計数探索
	ol = (unsigned int *)o; ol += h->a % *ol + 1;
	os = (unsigned long long *)o + (*ol >> 8);
	// ブロック内部の二分探索，範囲は[pmin,pmax)
	for (pmin = 0, pmax = *ol & 0xff; pmin < pmax; ) {
		// 中間地を取る
		op = os + (pmid = (pmin + pmax) / 2);
		// 一致したならば(最後のフラグ部分を残して一致するはず)
		if ((*op ^ h->b) < 16) return *op & 0xf;
		// 新たな範囲を決定
		if (*op < h->b) pmin = pmid + 1; else pmax = pmid;
	}
	return -1;
}

// 文字検索(失敗又は存在しなければ-1を返す)
int iword_seek(char *s) {
	ihash h; int r;
	
	// 初期化
	if (iword_alloc()) return -1;
	// 文字列のハッシュを求める
	iword_hash(&h, s);
	// ハッシュからキーを検索
	r = iword_seekhash(&h);
	// 解放
	iword_free();
	// キーが0〜14であればそれを，そうでなければ-1を返す
	return (r < 15) ? r : -1;
}

// 大文字小文字を無視した文字列比較
char* iword_prei(char *s1, char *s2) {
	char *ps1 = s1;
	
	for (;; s1++, s2++) {
		// 先に比較文字列が終端になった場合
		if (!*s1) return ps1;
		// 一致しない場合
		if ((*s1 ^ *s2) & 0x1f) break;
	}
	return NULL;
}

// 文字が単語としてあり得るか
static inline int iword_ischar(char a) {
	// 英数字であれば
	if (('a' <= a && a <= 'z') || ('A' <= a && a <= 'Z') ||
	 ('0' <= a && a <= '9') || a == '_' || a == '@') return 1;
	// さもなくばfalseを返す
	return 0;
}

// 文字列探索
long long *iword_map(char *s, int size, int mode) {
	ihash *h; void *hp; int i, j, k, r, m, html, fbid, eng;
	long long app = 0, *q = NULL, flg[4] = {0, 0, 0, 0};
	char *w, *v, *t, *keep = NULL;
	
	// モードの解釈
	html = mode & IWORD_MODE_HTML;
	fbid = mode & IWORD_MODE_FORBID;
	eng  = mode & IWORD_MODE_ENGLISH;
	// 初期化
	if (iword_alloc()) return NULL;
	// 文字列長の確認
	if (size < 0) return NULL;
	// フラグ領域を確保
	w = (char *)malloc(sizeof(char) * size);
	v = (char *)malloc(sizeof(char) * size);
	// ハッシュリストの領域を確保
#ifndef IWORD_MMX
	h = (ihash *)(hp = malloc(sizeof(ihash) * 256));
#else
	hp = malloc(sizeof(ihash) * 256 + 16);
	h = (ihash *)((((long long)hp + 15) | 0xf) ^ 0xf);
#endif
	// メモリの確保に失敗したら
	if (!w || !v || !h) goto finalize;
	// 初期化を行う
	memset(w, 0, sizeof(char) * size);
	memset(v, 0, sizeof(char) * size);
	memset(h, 0, sizeof(ihash) * 256);
	// すべての文字に対してハッシュチェックを行う
	for (i = 0; i < size; ++i) {
		// HTML関連処理
		if (html && s[i] == '<') {
			// タグの名称を特定する
			for (j = 0, i++; j < 10 && i + j < size && isalpha(s[i + j]); j++);
			// コメントまたは終了タグであるかを確認
			if (j == 0) {
				// コメント開始
				if (i + 3 < size && s[i] == '!'
				 && s[i + 1] == '-' && s[i + 2] == '-') {
					// コメント終了の探索
					for (i += 5; i < size; i++) if (s[i] == '>' &&
					 s[i - 1] == '-' && s[i - 2] == '-') break;
					continue;
				}
				// キープ中の終了タグの確認
				if (keep && i + (k = strlen(keep)) + 1 < size &&
				 s[i] == '/' && iword_prei(keep, &s[i + 1]) &&
				 s[i + k + 1] == '>') {
					i += k + 2;
					keep = NULL;
					continue;
				}
			// 禁止候補であるかを確認する
			} else if (!keep) {
				// 大体2分探索になるように絞りこむ
				if ((s[i] & 0x1fL) < ('s' & 0x1fL)) {
					// a, button, comment, listing, object, plaintext, pre
					if (j < 7) {
						if (j < 4) {
							if (j == 1) keep = iword_prei("a", &s[i]);
							else if (j == 3) keep = iword_prei("pre", &s[i]);
						} else if (j == 6) {
							keep = iword_prei("object", &s[i]);
							if (!keep) keep = iword_prei("button", &s[i]);
						}
					} else if (j == 7) {
						keep = iword_prei("comment", &s[i]);
						if (!keep) keep = iword_prei("listing", &s[i]);
					} else if (j == 9) keep = iword_prei("plaintext", &s[i]);
				} else if ((s[i] & 0x1fL) == ('s' & 0x1fL)) {
					// script, select, server, style
					if (j == 5) keep = iword_prei("style", &s[i]);
					else if (j == 6) {
						keep = iword_prei("script", &s[i]);
						if (!keep) keep = iword_prei("script", &s[i]);
						if (!keep) keep = iword_prei("select", &s[i]);
						if (!keep) keep = iword_prei("server", &s[i]);
					}
				} else if ((s[i] & 0x1fL) == ('t' & 0x1fL)) {
					// textarea, title
					if (j == 8) keep = iword_prei("textarea", &s[i]);
					else if (j == 5) keep = iword_prei("title", &s[i]);
				} else {
					// xmp
					if (j == 3) keep = iword_prei("xmp", &s[i]);
				}
			}
			// タグの終了を探索する
			for (; i < size && s[i] != '>'; i++)
			// クォーテーションが始まればその終了を探索する
			 if (s[i] == '"') for (i++; i < size && s[i] != '"'; i++);
			 else if (s[i] == '\'') for (i++; i < size && s[i] != '\''; i++);
			// キープされていれば次のタグの手前までポインタを進める
			if (keep) { for (; i < size && s[i] != '<'; i++) {} i--; }
			// 次に進む
			continue;
		}
		// 新規ハッシュを生成(fが処理フラグ)
		ihash *hp = &h[i & 0xffL];
		// フラグの設定
		flg[(i & 0xffL) >> 6] |= 1 << (i & 0x3f);
		// ハッシュの初期化
		hp->a = hp->b = 0; hp->f = 1;
		// 全てのハッシュを処理する
		//for (j = 0; j < 256; j++) if (h[j].f) {
		for (j = 0, t = &(h[0].f); j < 256; )
		 if (!flg[j >> 6]) j += 64, t += sizeof(ihash) * 64; else
		 for (m = j + 64; j < m; j++, t += sizeof(ihash)) if (*t) {
			// 次のハッシュを生成
			iword_nexthash(&h[j], s[i]);
			// ハッシュが存在するかチェックする
			r = iword_seekhash(&h[j]);
			// 無駄な探索を防止する
			// if (((i - j) & 15) == 15 && !~r) h[j].f = 0;
			if (((i - j) & 15) == 15 && !~r)
			 h[j].f = 0, flg[j >> 6] &= ~(1 << (j & 0x3f));
			// 発見した場合，記録をつける
			if (~r & 0xf) {
				k = i - ((i - j) & 0xff);
				// 英語モードの場合は前後に単語要素がないことが条件
				if (!eng || ((k <= 0 || !iword_ischar(s[k - 1])) &&
				 (size <= i + 1 || !iword_ischar(s[i + 1]))))
				  w[k] = i - j + 1, v[k] = r;
			}
		}
	}
	// 返すべき数を計算
	for (i = r = 0; i < size; ++i) if (w[i]) {
		if (fbid || IWORD_FORBID_NUM <= v[i]) r++;
		i += (w[i] - 1) & 0xffL;
	}
	// リミットを超えていた場合は小さくする
	if (iword_limit <= 0) iword_limit = 0xfffffff;
	if (iword_limit < r) r = iword_limit;
	// 返すためのメモリ領域を確保
	q = (long long *)malloc(sizeof(long long) * (r + 2));
	// 領域の確保に失敗したら
	if (q == NULL) goto finalize;
	// すべての値を格納する
	for (i = r = 0; i < size; ++i) if (w[i]) {
		if ((fbid || IWORD_FORBID_NUM <= v[i]) && r < iword_limit)
		 q[r++] = ((long long)i << 16) | (v[i] << 8 & 0xff00L) | (w[i] & 0xffL);
		i += (w[i] - 1) & 0xffL; app |= 1ULL << v[i];
	}
	// 終端にNULLを入れ，最後に固定長の出現リストを入れる
	q[r] = 0; q[r + 1] = app;
	// これ以降後処理
finalize:
	// メモリの解放
	free(hp); free(w); free(v); iword_free();
	// qを返す
	return q;
}
