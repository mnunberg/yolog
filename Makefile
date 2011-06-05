all: logtest

logtest: logtest.c yolog.c mlog_test.c
	$(CC) -o $@ $^ -lcurses
