all: libyolog.so autotest


CFLAGS=-Winit-self -Wall -O0 -ggdb3 -Isrc

libyolog.so: src/yolog.c
	$(CC) $(CFLAGS) -std=c89 -pedantic -shared -fPIC -o $@ $^


yolog_out/myproj_yolog.c: src/yolog.c src/yolog.h srcutil/genyolog.pl
	perl srcutil/genyolog.pl -c config/sample.cnf -S -y src

autotest: auto.c yolog_out/myproj_yolog.c libyolog.so
	$(CC) -pthread -std=c99 $(CFLAGS) -o $@ \
	   auto.c \
	   yolog_out/myproj_yolog.c

clean:
	rm -rf libyolog.so autotest yolog_out
