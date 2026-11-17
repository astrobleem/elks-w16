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
	python3 tests/manysegtest.py
	python3 tests/image_setup_test.py
	python3 tests/dibtest.py
	python3 tests/registeredtest.py
	python3 tests/lineddatest.py
	python3 tests/enumobjtest.py
	python3 tests/recttest.py
	python3 tests/globaltest.py
	python3 tests/childtest.py
	python3 tests/textlayouttest.py
	python3 tests/bitmaptest.py
	python3 tests/devicecapstest.py
	python3 tests/brushorgtest.py
	python3 tests/setpixeltest.py
	python3 tests/dcorgtest.py
	python3 tests/getpixeltest.py
	python3 tests/readpixelbackendtest.py
	python3 tests/capturepixelstest.py
	python3 tests/missingtraptest.py

clean:
	$(MAKE) -C src clean
	rm -f test/*/*.EXE test/*/*.OBJ test/*/*.MAP test/*/*.RES test/dlgtest/dlgtest.ico test/dlgtest/nametab.bin
	rm -rf build
