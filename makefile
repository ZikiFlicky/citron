DEBUG_VERSION := 112
DEBUG_BUILD_VERSION := "\"$(DEBUG_VERSION)\""
LEXTRACF := ${LEXTRACF} -flto -lstdc++
fv := $(strip $(shell ldconfig -p | grep libgc.so | cut -d ">" -f2 | head -n1))
location = $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
WHERE_ART_THOU := $(location)
new_makefile_l1 := $(shell perl -ne '/((DEBUG_VERSION := )(\d+))/ && print (sprintf("%s%s", "$$2", "$$3"+1));' $(WHERE_ART_THOU))

ifeq ($(strip ${WITH_ICU}),)
	CFLAGS = -Wall -Wextra -Wno-unused-parameter -mtune=native\
              -march=native -D withTermios -D forLinux\
              -D CTR_STD_EXTENSION_PATH=\"`pwd`\"
else
	CFLAGS = -Wall -Wextra -Wno-unused-parameter -mtune=native\
			 -march=native -D withTermios -D forLinux\
			 -D CTR_STD_EXTENSION_PATH=\"`pwd`\" -D withICU
	LEXTRACF := ${LEXTRACF} -L/usr/lib -licui18n -licuuc -licudata
endif

ifeq ($(strip ${WITHOUT_BOEHM_GC}),)
	CFLAGS := ${CFLAGS} "-D withBoehmGC"
	LEXTRACF := ${LEXTRACF} ${fv}
else
endif

ifeq ($(strip ${WITH_TYPED_GC}),)
else
	CFLAGS := ${CFLAGS} "-D withBoehmGC_P"
endif


.PHONY: gc_check
gc_check:
	@if [ "${fv}x" == "x" ]; then echo "Could not find libgc.so."; echo Failing; exit 1; else echo "Found libgc.so at ${fv}"; fi
	$(eval LEXTRACF := ${LEXTRACF} ${fv})

-include gc_check

OBJS = siphash.o utf8.o memory.o util.o base.o collections.o file.o system.o\
		   world.o lexer.o lexer_plug.o parser.o walker.o marshal.o reflect.o fiber.o\
			 importlib.o coroutine.o symbol.o generator.o base_extensions.o citron.o\
			 promise.o symbol_cxx.o

COBJS = ${OBJS} compiler.o

# .SUFFIXES:	.o .c

all: CFALGS := $(CFLAGS) -O2
all: cxx
all: ctr

debug: CFLAGS := ${CFLAGS} -DDEBUG_BUILD -DDEBUG_BUILD_VERSION=${DEBUG_BUILD_VERSION} -Og -g3 -ggdb3 -Wno-unused-function
debug: cxx
debug: ctr
debug:
	sed -i -e "1s/.*/${new_makefile_l1}/" makefile

install:
	echo -e "install directly from source not allowed.\nUse citron_autohell instead for installs"
	exit 1;
ctr:	$(OBJS)
	$(CC) -fopenmp $(OBJS) -rdynamic -lm -ldl -lbsd -lpcre -lpthread ${LEXTRACF} -o ctr

libctr: CFLAGS := $(CFLAGS) -fPIC -DCITRON_LIBRARY
libctr: symbol_cxx
libctr: $(OBJS)
	$(CC) $(OBJS) -shared -export-dynamic -ldl -lbsd -lpcre -lpthread -o libctr.so

compiler: CFLAGS := $(CFLAGS) -D comp=1
compiler: cxx
compiler: $(COBJS)
	$(CC) $(COBJS) -rdynamic -lm -ldl -lbsd -lpcre -lprofiler -lpthread ${LEXTRACF} -o ctrc

cxx:
	echo "blah"

%.o: %.c
	$(CC) -fopenmp $(CFLAGS) -c $< >/dev/null 2>&1

define SHVAL =
for f in *.c; do\
	echo "Formatting $f";\
	indent -l150 -hnl -bbo $f;\
done
endef

format:
	mkdir -p .bac
	rm -rf .bac/*.c
	cp *.c .bac/
	$(value SHVAL)
	rm -rf *.c~

.ONESHELL:

unback:
	cp .bac/*.c .

clean:
	rm -rf ${OBJS} ctr

cclean:
	rm -rf ${COBJS} ctrc

love:
	echo "Not war?"

war:
	echo "Not love?"
