
INC=
LIBS=-lcrypto
DEFS=
LDFLAGS=

CXXFLAGS=-std=c++11 -Wall -O2 -pedantic $(INC) $(DEFS)
CXX=c++
LD=c++


# if your CXX=clang
#LIBS+=-lstdc++

.PHONY: all clean distclean

all: build build/hoschi

build:
	mkdir build || true

clean:
	rm -f build/*.o

distclean:
	rm -rf build

build/hoschi: build/btc-map.o build/main.o build/protocol.o build/filter.o build/log.o build/global.o build/config.o
	$(LD) $(LDFLAGS) build/btc-map.o build/main.o build/protocol.o build/filter.o build/log.o build/global.o build/config.o -o build/hoschi $(LIBS)

build/btc-map.o: btc-map.cc btc-map.h misc.h protocol.h filter.h log.h global.h
	$(CXX) $(CXXFLAGS) -c btc-map.cc -o build/btc-map.o

build/protocol.o: protocol.cc protocol.h misc.h missing.h btc-map.h global.h
	$(CXX) $(CXXFLAGS) -c protocol.cc -o build/protocol.o

build/filter.o: filter.cc filter.h misc.h global.h protocol.h config.h btc-map.h
	$(CXX) $(CXXFLAGS) -c filter.cc -o build/filter.o

build/log.o: log.cc log.h
	$(CXX) $(CXXFLAGS) -c log.cc -o build/log.o

build/global.o: global.cc global.h log.h
	$(CXX) $(CXXFLAGS) -c global.cc -o build/global.o

build/config.o: config.cc
	$(CXX) $(CXXFLAGS) -c config.cc -o build/config.o

build/main.o: main.cc btc-map.h global.h config.h
	$(CXX) $(CXXFLAGS) -c main.cc -o build/main.o

