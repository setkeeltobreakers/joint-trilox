CFLAGS=-O2

all: libraries
	make clean

debug: libraries
	make clean

debug: CFLAGS:=-pg -g



libraries: source/corelib.c joint
	cc $(CFLAGS) -shared -o lib/native/corelib.binlib source/corelib.c *.o -fPIC;

joint: main.o scanner.o compiler.o chunk.o memory.o vm.o value.o object.o table.o logic.o library.o config.o -lm
	cc $(CFLAGS) -o joint main.o scanner.o compiler.o chunk.o memory.o vm.o value.o object.o table.o logic.o library.o config.o -lm

main.o: source/main.c
	cc $(CFLAGS) -o main.o -c source/main.c -fPIC

compiler.o: source/compiler.c
	cc $(CFLAGS) -o compiler.o -c source/compiler.c -fPIC

scanner.o: source/scanner.c
	cc $(CFLAGS) -o scanner.o -c source/scanner.c -fPIC

chunk.o: source/chunk.c
	cc $(CFLAGS) -o chunk.o -c source/chunk.c -fPIC

memory.o: source/memory.c
	cc $(CFLAGS) -o memory.o -c source/memory.c -fPIC

vm.o: source/vm.c
	cc $(CFLAGS) -o vm.o -c source/vm.c -fPIC

value.o: source/value.c
	cc $(CFLAGS) -o value.o -c source/value.c -fPIC

object.o: source/object.c
	cc $(CFLAGS) -o object.o -c source/object.c -fPIC

table.o: source/table.c
	cc $(CFLAGS) -o table.o -c source/table.c -fPIC

logic.o: source/logic.c
	cc $(CFLAGS) -o logic.o -c source/logic.c -fPIC

library.o: source/library.c
	cc $(CFLAGS) -o library.o -c source/library.c -fPIC

config.o: source/config.c
	cc $(CFLAGS) -o config.o -c source/config.c -fPIC

clean:
	rm -f *.o

remove:
	rm -f *.o; rm -f lib/native/*.binlib; rm -f joint
