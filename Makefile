CFLAGS=-g -O0 -ldl -lrt -shared -Wall -fPIC -lpthread -DSUPPORT_X11 -Wl,-soname,swaplogger.so.1
LDFLAGS=
OBJS=

# EGL support
CFLAGS+=-DUSE_EGL
OBJS+=swaplogger_egl.o
LDFLAGS+=-lEGL

# XShmPutImage support
CFLAGS+=-DUSE_XSHM
OBJS+=swaplogger_xshm.o

# X damage extension support
CFLAGS+=-DUSE_XDAMAGE
OBJS+=swaplogger_xdamage.o
LDFLAGS+=$(shell pkg-config --libs xdamage x11)

swaplogger.so.1: swaplogger.c $(OBJS)
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< $(OBJS)
	ln -fs swaplogger.so.1 swaplogger.so

.PHONY: clean
clean:
	rm -rf *.o swaplogger.so swaplogger.so.1
