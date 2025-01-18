.PHONY: build
build: dist/cloader

dist/cloader: src/main.c
	gcc -Os -s -fno-ident -fno-asynchronous-unwind-tables -o dist/cloader src/main.c