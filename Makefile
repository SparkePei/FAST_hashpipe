# bin directory
BINDIR = ./bin

CC          = g++
CCFLAGS     = -g -O3 -fPIC -shared -lstdc++ -mavx -msse4 \
                     -I. -I$(CUDA_DIR)/include -I/usr/local/include \
                     -L. -L/usr/local/lib \
                     -lhashpipe -lrt -lm

FILTERBANK_OBJECT   = filterbank.o
FILTERBANK_SOURCES  = filterbank.cpp
FILTERBANK_LIB_INCLUDES = filterbank.h

HPFAST_LIB_OBJECT   = $(patsubst %.c,%.o,$(HPFAST_LIB_SOURCES))
HPFAST_LIB_TARGET   = FAST_hashpipe.so
HPFAST_LIB_SOURCES  = FAST_net_thread.c \
                      FAST_gpu_thread.c \
		      FAST_output_thread.c\
                      FAST_databuf.c
HPFAST_LIB_INCLUDES = FAST_databuf.h \

all: $(FILTERBANK_OBJECT)  $(HPFAST_LIB_OBJECT) $(HPFAST_LIB_TARGET)

$(FILTERBANK_OBJECT): $(FILTERBANK_SOURCES) $(FILTERBANK_LIB_INCLUDES)
	$(CC) -c $< $(CCFLAGS) 

$(HPFAST_LIB_OBJECT): $(HPFAST_LIB_SOURCES) $(HPFAST_LIB_INCLUDES)
	$(CC) -c $^ $(CCFLAGS)

$(HPFAST_LIB_TARGET): $(HPFAST_LIB_OBJECT) $(FILTERBANK_OBJECT) $(HPFAST_LIB_INCLUDES)
	$(CC) *.o -o $@ $(CCFLAGS)

tags:
	ctags -R .
clean:
	rm -f $(HPFAST_LIB_TARGET) *.o tags

prefix=/usr/local
LIBDIR=$(prefix)/lib
BINDIR=$(prefix)/bin
install-lib: $(HPFAST_LIB_TARGET)
	mkdir -p "$(DESTDIR)$(LIBDIR)"
	install -p $^ "$(DESTDIR)$(LIBDIR)"
install: install-lib

.PHONY: all tags clean install install-lib
# vi: set ts=8 noet :
