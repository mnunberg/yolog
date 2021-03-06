all: libyolog.so demo_static demo_dynamic

CFLAGS=-Winit-self -Wall -Wextra -ggdb3 -DYOLOG_APESQ_STATIC -I$(shell pwd)/src
export CFLAGS
export YDEBUG_LEVEL
export YOCMD
export YOARGS

libyolog.so: src/yolog.c src/yoconf.c src/format.c
	$(CC) $(CFLAGS) -std=c89 -pedantic -shared -fPIC -o $@ $^


YOCMD=$(shell pwd)/srcutil/genyolog.pl
YOARGS=-c $(shell pwd)/config/sample.cnf -y $(shell pwd)/src -K

demo_static:
	$(MAKE) -C demo \
		YOARGS="$(YOARGS) -S" PREFIX="static"
	mv -f demo/$@ .

demo_dynamic:
	$(MAKE) -C demo PREFIX=dynamic \
		DEMO_LFLAGS="-Wl,-rpath=$(shell pwd) -L$(shell pwd) -lyolog"
	mv -f demo/$@ .

clean:
	rm -rf libyolog.so demo_*
	rm -rf demo/static demo/dynamic
