SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:src/%.c=bin/%.o)

all: lib/libtaps.so lib/libtaps_tcp.so

lib/libtaps.so: $(OBJECTS)
	gcc -shared -o lib/libtaps.so $(OBJECTS)

lib/libtaps_tcp.so: src/tcp/tcp.c
	gcc -o lib/tcp.o -c src/tcp/tcp.c -levent
	gcc -shared -o lib/libtaps_tcp.so lib/tcp.o -levent
	rm -f lib/tcp.o

bin/%.o: src/%.c
	gcc -c $< -o $@ -lyaml -ldl -fPIC -I .

TEST_SOURCES := $(filter-out test/t.c, $(wildcard test/*.c))
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)

test/%.o: test/%.c
	gcc -c $< -o $@ -lyaml -I test/

examples: lib/libtaps.so lib/libtaps_tcp.so
	gcc -o echoapp examples/echoapp.c -L ./lib -levent -ltaps -ltaps_tcp -lyaml -ldl
	chmod 755 echoapp

test: $(TEST_OBJECTS) $(OBJECTS)
	gcc -o test/t $(OBJECTS) $(TEST_OBJECTS) test/t.c -lyaml -ldl -I test/
	./test/t

install: lib/libtaps.so lib/libtaps_tcp.so
	cp lib/libtaps.so /usr/lib
	cp lib/libtaps_tcp.so /usr/lib

clean:
	rm -f *.o *.a test/t test/*.o bin/*.o lib/*.so echoapp
