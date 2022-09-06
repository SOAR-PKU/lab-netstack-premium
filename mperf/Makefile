INSTALL_PATH := /usr/bin

.PHONY: all
all: init clean
	make -C src

.PHONY: init
init:
	-mkdir bin
	-mkdir temp

.PHONY: install
install:
	cp bin/* $(INSTALL_PATH)/
	cp src/scripts/* $(INSTALL_PATH)/ 

.PHONY: uninstall
uninstall:
	-rm $(INSTALL_PATH)/mperf-*

.PHONY: clean
clean:
	-rm bin/* -r
	-rm temp/* -r
	make -C src clean
