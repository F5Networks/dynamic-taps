CXX = gcc
CC = $(CXX)

DEBUG_LEVEL = -g
CXXFLAGS    = $(DEBUG_LEVEL)
CCFLAGS     = $(CXXFLAGS)

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:src/%.c=bin/%.o)

all: lib/libtaps.so lib/libtaps_tcp.so

lib/libtaps.so: $(OBJECTS)
	$(CC) $(CCFLAGS) -shared -o lib/libtaps.so $(OBJECTS)

lib/libtaps_tcp.so: src/tcp/tcp.c
	$(CC) $(CCFLAGS) -o lib/tcp.o -c src/tcp/tcp.c -fPIC -levent
	$(CC) $(CCFLAGS) -shared -o lib/libtaps_tcp.so  lib/tcp.o -levent
	rm -f lib/tcp.o

bin/%.o: src/%.c
	$(CC) $(CCFLAGS) -c $< -o $@ -lyaml -levent -ldl -fPIC -I .

install: lib/libtaps.so lib/libtaps_tcp.so
	cp lib/libtaps.so /usr/lib/x86_64-linux-gnu/
	cp lib/libtaps_tcp.so /usr/lib/x86_64-linux-gnu/
	if [ ! -d "/etc/taps" ]; then mkdir /etc/taps; fi
	cp ./kernel.yaml /etc/taps

clean:
	rm -f *.o *.a test/t test/*.o bin/*.o lib/*.so examples/echoapp

# Builds for unit tests
TEST_SOURCES := $(filter-out test/t.c, $(wildcard test/*.c))
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)

test/%.o: test/%.c
	$(CC) $(CCFLAGS) -c $< -o $@ -I test/

test: $(TEST_OBJECTS) $(OBJECTS)
	$(CC) $(CCFLAGS) -o test/t $(OBJECTS) $(TEST_OBJECTS) test/t.c -levent -lyaml -ldl -I test/
	./test/t

# Builds for examples
EXAMPLE_SOURCES := $(wildcard examples/*.c)
EXAMPLE_OBJS := $(EXAMPLE_SOURCES:examples/%.c=examples/%)

examples/%: examples/%.c
	$(CC) $(CCFLAGS) $< -o $@ -L ./lib -ltaps -ltaps_tcp -levent -lyaml -ldl

examples: $(EXAMPLE_OBJS) lib/libtaps.so lib/libtaps_tcp.so
	chmod 755 examples/echoapp
