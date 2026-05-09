{
  "targets": [{
    "target_name": "iword_napi",
    "sources": [ "iword_napi.cc" ],
    "include_dirs": [
      "../../include"
    ],
    "conditions": [
      ["OS=='mac'", {
        "libraries": [ "<(module_root_dir)/../../bin/libiword.dylib" ],
        "xcode_settings": {
          "OTHER_LDFLAGS": [ "-Wl,-rpath,@loader_path/../../../bin" ]
        }
      }],
      ["OS=='linux'", {
        "libraries": [ "<(module_root_dir)/../../bin/libiword.so" ],
        "ldflags": [ "-Wl,-rpath,$$ORIGIN/../../../bin" ]
      }]
    ],
    "cflags": [ "-O2", "-Wall" ],
    "cflags_cc": [ "-O2", "-Wall" ]
  }]
}
