all: demo_$(PREFIX)

# Directory
DIR=$(PREFIX)

# Yolog output directory
OUTDIR=$(DIR)/yolog_out

# Sourc code..
SRC=$(DIR)/demo.c

# The actual generated source file
YOSRC=$(DIR)/yolog_out/myproj_yolog.c

CFLAGS += -pthread

ifdef YDEBUG_LEVEL
	CFLAGS += -DMYPROJ_YOLOG_DEBUG_LEVEL=$(YDEBUG_LEVEL)
endif

$(SRC): demo.c
	test -d $(DIR) || mkdir $(DIR)
	cp $^ $(DIR)/

$(YOSRC): $(YODEPS) $(YOCMD)
	$(YOCMD) $(YOARGS) -o $(OUTDIR)

demo_$(PREFIX): $(SRC) $(YOSRC)
	$(CC) $(CFLAGS) -o $@ $^ \
		$(DEMO_LFLAGS)

