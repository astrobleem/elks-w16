.PHONY: all host samples test clean

all:
	$(MAKE) -C src

host:
	$(MAKE) -C src host

samples:
	sh test/hello16/build.sh
	sh test/dlgtest/build.sh
	sh test/mbtest/build.sh

test: host samples
	bash tests/hosttest.sh
	python3 tests/image_setup_test.py

clean:
	$(MAKE) -C src clean
	rm -f test/*/*.EXE test/*/*.OBJ test/*/*.MAP test/*/*.RES test/dlgtest/dlgtest.ico test/dlgtest/nametab.bin
	rm -rf build
