/**
 * iWord Node.js N-API native addon.
 *
 * Build: npm run build  (runs node-gyp rebuild)
 * Use:   const iword = require('./build/Release/iword_napi');
 */

#include <node_api.h>
#include <string.h>
#include <stdlib.h>

// iWord core header (relative to build context; binding.gyp sets include_dirs)
#include "iword.h"

// ---- helpers ----------------------------------------------------------------

#define NAPI_CALL(env, call)                                        \
  do {                                                              \
    napi_status _s = (call);                                        \
    if (_s != napi_ok) {                                            \
      const napi_extended_error_info *info;                         \
      napi_get_last_error_info((env), &info);                       \
      napi_throw_error((env), NULL,                                 \
        info->error_message ? info->error_message : "N-API error"); \
      return NULL;                                                  \
    }                                                               \
  } while (0)

static napi_value throw_type_error(napi_env env, const char *msg) {
  napi_throw_type_error(env, NULL, msg);
  return NULL;
}

// ---- iword_load(filename) -> 0 on success -----------------------------------

static napi_value Load(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  if (argc < 1) return throw_type_error(env, "load(filename) requires 1 argument");

  size_t len;
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], NULL, 0, &len));
  char *filename = (char *)malloc(len + 1);
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], filename, len + 1, &len));

  int ret = iword_load(filename);
  free(filename);

  napi_value result;
  NAPI_CALL(env, napi_create_int32(env, ret, &result));
  return result;
}

// ---- iword_unload() -> 0 on success -----------------------------------------

static napi_value Unload(napi_env env, napi_callback_info info) {
  int ret = iword_unload();
  napi_value result;
  NAPI_CALL(env, napi_create_int32(env, ret, &result));
  return result;
}

// ---- iword_seek(word) -> key (int), or -1 -----------------------------------

static napi_value Seek(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  if (argc < 1) return throw_type_error(env, "seek(word) requires 1 argument");

  size_t len;
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], NULL, 0, &len));
  char *word = (char *)malloc(len + 1);
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], word, len + 1, &len));

  int key = iword_seek(word);
  free(word);

  napi_value result;
  NAPI_CALL(env, napi_create_int32(env, key, &result));
  return result;
}

// ---- iword_map(text, textLen, mode) -> [{position, length, key}, ...] -------

static napi_value Map(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  if (argc < 1) return throw_type_error(env, "map(text[, mode]) requires at least 1 argument");

  // text
  size_t len;
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], NULL, 0, &len));
  char *text = (char *)malloc(len + 1);
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], text, len + 1, &len));

  // mode (optional, default MODE_HTML|MODE_FORBID)
  int mode = IWORD_MODE_HTML | IWORD_MODE_FORBID;
  if (argc >= 2) {
    int32_t m;
    if (napi_get_value_int32(env, args[1], &m) == napi_ok) mode = m;
  }

  long long *raw = iword_map(text, (int)len, mode);
  free(text);

  napi_value arr;
  NAPI_CALL(env, napi_create_array(env, &arr));

  if (!raw) return arr;

  uint32_t idx = 0;
  for (int i = 0; raw[i]; i++) {
    long long entry = raw[i];
    int length   =  entry        & 0xFF;
    int key      = (entry >> 8)  & 0xFF;
    int position = (entry >> 16) & 0xFFFFFFFF;

    napi_value obj;
    NAPI_CALL(env, napi_create_object(env, &obj));

    napi_value vpos, vlen, vkey;
    NAPI_CALL(env, napi_create_int32(env, position, &vpos));
    NAPI_CALL(env, napi_create_int32(env, length,   &vlen));
    NAPI_CALL(env, napi_create_int32(env, key,      &vkey));
    NAPI_CALL(env, napi_set_named_property(env, obj, "position", vpos));
    NAPI_CALL(env, napi_set_named_property(env, obj, "length",   vlen));
    NAPI_CALL(env, napi_set_named_property(env, obj, "key",      vkey));

    NAPI_CALL(env, napi_set_element(env, arr, idx++, obj));
  }
  free(raw);

  return arr;
}

// ---- iword_mask() -> bitmask ------------------------------------------------

static napi_value Mask(napi_env env, napi_callback_info info) {
  int m = iword_mask();
  napi_value result;
  NAPI_CALL(env, napi_create_int32(env, m, &result));
  return result;
}

// ---- iword_set_limit(n) -----------------------------------------------------

static napi_value SetLimit(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  int32_t n = 0;
  if (argc >= 1) napi_get_value_int32(env, args[0], &n);
  iword_set_limit(n);
  return NULL;
}

// ---- iword_set_strkey(key) --------------------------------------------------

static napi_value SetDictKey(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  if (argc < 1) return throw_type_error(env, "setDictKey(key) requires 1 argument");

  size_t len;
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], NULL, 0, &len));
  char *key = (char *)malloc(len + 1);
  NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], key, len + 1, &len));
  iword_set_strkey(key, len);
  free(key);
  return NULL;
}

// ---- module init ------------------------------------------------------------

static napi_value Init(napi_env env, napi_value exports) {
  struct { const char *name; napi_callback fn; } funcs[] = {
    { "load",       Load       },
    { "unload",     Unload     },
    { "seek",       Seek       },
    { "map",        Map        },
    { "mask",       Mask       },
    { "setLimit",   SetLimit   },
    { "setDictKey", SetDictKey },
  };
  for (size_t i = 0; i < sizeof(funcs)/sizeof(funcs[0]); i++) {
    napi_value fn;
    napi_create_function(env, funcs[i].name, NAPI_AUTO_LENGTH, funcs[i].fn, NULL, &fn);
    napi_set_named_property(env, exports, funcs[i].name, fn);
  }

  // Constants
  struct { const char *name; int val; } consts[] = {
    { "MODE_HTML",    IWORD_MODE_HTML    },
    { "MODE_FORBID",  IWORD_MODE_FORBID  },
    { "MODE_ENGLISH", IWORD_MODE_ENGLISH },
    { "KEY_HIDDEN",   IWORD_KEY_HIDDEN   },
    { "KEY_ADULT",    IWORD_KEY_ADULT    },
    { "KEY_SPAM",     IWORD_KEY_SPAM     },
    { "KEY_DEFAULT",  9                  },
  };
  for (size_t i = 0; i < sizeof(consts)/sizeof(consts[0]); i++) {
    napi_value v;
    napi_create_int32(env, consts[i].val, &v);
    napi_set_named_property(env, exports, consts[i].name, v);
  }

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
