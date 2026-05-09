CC = gcc
CFLAGS = -O2

# Default: build CLI tools + shared library (needed by Python/Go/Node bindings)
all: _tool _lib

# Full build including PHP PECL extension (requires phpize)
full: _tool _lib _pecl

lib: _lib
_lib:
	-mkdir bin 2>/dev/null
	$(CC) $(CFLAGS) -shared -fPIC -o bin/libiword.so include/iword.c
	@if [ "$$(uname)" = "Darwin" ]; then \
		$(CC) $(CFLAGS) -shared -fPIC -dynamiclib \
			-install_name @rpath/libiword.dylib \
			-o bin/libiword.dylib include/iword.c; \
		echo "Built bin/libiword.dylib (macOS)"; \
	fi
	@echo "Built bin/libiword.so (for Python/Go/Node bindings)"

pecl: _pecl
_pecl:
	-rm -r -f -d temp_pecl; mkdir temp_pecl
	-cp -r pecl/* temp_pecl; mkdir temp_pecl/include
	-cp -r include/* temp_pecl/include
	cd temp_pecl; phpize; ./configure; make -j 8
	-mkdir bin 2>/dev/null; rm -r -f -d bin/modules
	-cp -r temp_pecl/modules bin; rm -r -f -d temp_pecl

tool: _tool
_tool: iwordctl iworduse iwordserver iwordtest

iwordtest:
	-mkdir bin 2>/dev/null
	-cp tool/iword.php tool/iword.sh tool/iword-speed.php tool/iwordd bin
	-chmod +x bin/iwordd

iwordctl:
	-rm -r -f -d temp_ctl
	-mkdir temp_ctl
	-cp -r tool/* temp_ctl
	cp -r include/* temp_ctl
	cd temp_ctl; make iwordctl
	-mkdir bin 2>/dev/null
	-cp temp_ctl/iwordctl temp_ctl/iworduse bin
	-rm -r -f -d temp_ctl

iworduse:
	-rm -r -f -d temp_use
	-mkdir temp_use
	-cp -r tool/* temp_use
	cp -r include/* temp_use
	cd temp_use; make iworduse
	-mkdir bin 2>/dev/null
	-cp temp_use/iwordctl temp_use/iworduse bin
	-rm -r -f -d temp_use

iwordserver:
	-rm -r -f -d temp_server
	-mkdir temp_server
	-cp -r tool/* temp_server
	cp -r include/* temp_server
	cd temp_server; make iwordserver
	-mkdir bin 2>/dev/null
	-cp temp_server/iwordserver bin
	-rm -r -f -d temp_server

# Python binding: no build step needed (ctypes loads libiword.so directly)
python: _lib
	@echo "Python binding ready. Run: python3 bindings/python/test_basic.py"

# Go binding
go: _lib
	cd bindings/go && go build . && go vet .
	@echo "Go binding built."

# Node.js binding (N-API addon)
node: _lib
	cd bindings/node && npm run build
	@echo "Node.js binding built."

clean:
	-rm -r -f -d bin
	-rm -r -f -d bindings/node/build

install:
	cp bin/iwordctl /usr/local/bin
	cp bin/modules/iword.so /usr/local/lib
