SHELL := /bin/bash
TARGET := ../bin
CC := gcc
CXX := g++
CMACRO := -D "PROGNAME=\"mPerf\"" -D "VERSION=\"1.2.2.0130 Alpha\""
CXXMACRO := $(CMACRO)
PACKAGE_PREFIX := mperf
CFLAGS := $(CMACRO) -O2 -Wall -Werror -I ./include
CXXFLAGS := $(CXXMACRO) -O2 -Wall -Werror -I ./include
PROGS := client server udpreceiver udpsender
PROGNAMES := $(patsubst %,$(PACKAGE_PREFIX)-%,$(PROGS))
OUT := util.o log.o sndrcv.o worker.o
PROBE_PROGS := client server
PROBE_PROGNAMES := $(patsubst %,$(PACKAGE_PREFIX)-probe-%,$(PROBE_PROGS))
PROBE_OUT := util.o log.o probe-sndrcv.o probe-worker.o
CHECK_PROGS := client server
CHECK_PROGNAMES := $(patsubst %,$(PACKAGE_PREFIX)-check-%,$(CHECK_PROGS))
CHECK_OUT := util.o log.o check-sndrcv.o check-worker.o
LIB := -lpthread

$(PACKAGE_PREFIX)-%: %.cpp $(OUT)
	$(CXX) $(CXXFLAGS) -o $(TARGET)/$@ $< $(OUT) $(LIB)

$(PACKAGE_PREFIX)-%: %.cc $(OUT)
	$(CXX) $(CXXFLAGS) -o $(TARGET)/$@ $< $(OUT) $(LIB)

$(PACKAGE_PREFIX)-%: %.c $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/$@ $< $(OUT) $(LIB)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

probe-%.o: %.c
	$(CC) -D PROBE $(CFLAGS) -c -o $@ $<

check-%.o: %.c
	$(CC) -D CHECK $(CFLAGS) -c -o $@ $<

$(PACKAGE_PREFIX)-check-%: %.c $(CHECK_OUT)
	$(CC) -D CHECK $(CFLAGS) -o $(TARGET)/$@ $< $(CHECK_OUT) $(LIB)

$(PACKAGE_PREFIX)-probe-%: %.c $(PROBE_OUT)
	$(CC) -D PROBE $(CFLAGS) -o $(TARGET)/$@ $< $(PROBE_OUT) $(LIB)

.PHONY: all
all: $(PROGNAMES) $(PROBE_PROGNAMES) $(CHECK_PROGNAMES)

.PHONY: clean
clean: 
	-rm $(OUT) $(PROGNAMES) *~ 2> /dev/null
