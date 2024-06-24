CC=g++
CXX=g++
RANLIB=ranlib

LIBSRC=uthreads.cpp Thread.cpp 
LIBOBJ=$(LIBSRC:.cpp=.o)
HEADERS=$(LIBSRC:.cpp=.h)

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)

OSMLIB = libuthreads.a
TARGETS = $(OSMLIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex2.tar
TARSRCS=$(LIBSRC) $(HEADERS) Makefile README 

all: $(TARGETS)

$(TARGETS): $(LIBOBJ) $(HEADERS)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(OSMLIB) $(OBJ) $(LIBOBJ) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
