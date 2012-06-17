all: libyolog.so demo_static demo_dynamic


CFLAGS=-Winit-self -Wall -O0 -ggdb3 -I$(shell pwd)/src
export CFLAGS

libyolog.so: src/yolog.c
	$(CC) $(CFLAGS) -std=c89 -pedantic -shared -fPIC -o $@ $^


YOCMD=$(shell pwd)/srcutil/genyolog.pl
YOARGS=-c $(shell pwd)/config/sample.cnf -y $(shell pwd)/src

demo_static:
	$(MAKE) -C demo YOCMD=$(YOCMD) \
		YOARGS="$(YOARGS) -S" \
		PREFIX="static"

	mv -f demo/$@ .

demo_dynamic:
	$(MAKE) -C demo \
		YOCMD="$(YOCMD)" \
		YOARGS="$(YOARGS)" \
		PREFIX=dynamic \
		DEMO_LFLAGS="-Wl,-rpath=$(shell pwd) -L$(shell pwd) -lyolog"

	mv -f demo/$@ .

clean:
	rm -rf libyolog.so demo_*
	rm -rf demo/static demo/dynamic
