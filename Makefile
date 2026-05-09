CC = gcc
CFLAGS = -O2

all: _pecl _tool

lib: _lib
_lib:
	-mkdir bin 2>/dev/null
	$(CC) $(CFLAGS) -shared -fPIC -o bin/libiword.so include/iword.c
	@echo "Built bin/libiword.so (for Python/Go ctypes bindings)"

pecl: _pecl
_pecl:
	-rm -r -f -d temp_pecl; mkdir temp_pecl
	-cp -r pecl/* temp_pecl; mkdir temp_pecl/include
	-cp -r include/* temp_pecl/include
	cd temp_pecl; phpize; ./configure; make -j 8
	-mkdir bin 2>/dev/null; rm -r -f -d bin/modules
	-cp -r temp_pecl/modules bin; rm -r -f -d temp_pecl

tool: _tool
_tool: iwordctl iworduse iwordtest

iwordtest:
	-mkdir bin 2>/dev/null
	-cp tool/iword.php tool/iword.sh tool/iword-speed.php bin

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

clean:
	-rm -r -f -d bin

install:
	cp bin/iwordctl /usr/local/bin
	cp bin/modules/iword.so /usr/local/lib
