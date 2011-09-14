CC = gcc
FLAGS = -g -o $@
CFLAGS = -c -Wall -Werror -pedantic -std=c99 $(FLAGS)
LDFLAGS = $(FLAGS)

all : blang runtime.a

blang : main.o ast.o scan.o parse.tab.o hash_table.o print.o resolve.o typecheck.o canon.o reduce.o annotate.o inline.o prune.o alloc.o codegen.o
	$(CC) $(LDFLAGS) main.o ast.o scan.o parse.tab.o hash_table.o print.o resolve.o typecheck.o canon.o reduce.o annotate.o inline.o prune.o alloc.o codegen.o

runtime.a : runtime.c
	$(CC) $(CFLAGS) -m32 runtime.c

main.o : main.c ast.h parse.tab.h
	$(CC) $(CFLAGS) main.c

hash_table.o : hash_table.c hash_table.h
	$(CC) $(CFLAGS) -D_GNU_SOURCE hash_table.c

codegen.o : codegen.c ast.h hash_table.h
	$(CC) $(CFLAGS) codegen.c

alloc.o : alloc.c ast.h
	$(CC) $(CFLAGS) alloc.c

prune.o : prune.c ast.h
	$(CC) $(CFLAGS) prune.c

inline.o : inline.c ast.h
	$(CC) $(CFLAGS) inline.c

annotate.o : annotate.c ast.h
	$(CC) $(CFLAGS) annotate.c

reduce.o : reduce.c ast.h
	$(CC) $(CFLAGS) reduce.c

canon.o : canon.c ast.h
	$(CC) $(CFLAGS) canon.c

typecheck.o : typecheck.c ast.h
	$(CC) $(CFLAGS) typecheck.c

resolve.o : resolve.c ast.h hash_table.h
	$(CC) $(CFLAGS) resolve.c

print.o : print.c ast.h
	$(CC) $(CFLAGS) print.c

ast.o : ast.c ast.h hash_table.h
	$(CC) $(CFLAGS) ast.c

scan.o : scan.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE scan.c	

scan.c : blang.l parse.tab.h
	flex -oscan.c blang.l

parse.tab.o : parse.tab.c parse.tab.h
	$(CC) $(CFLAGS) -D_GNU_SOURCE parse.tab.c

parse.tab.c parse.tab.h : blang.y ast.h
	bison -d -bparse -v --warngins=all blang.y

clobber : clean
	rm -f blang runtime.a || true

clean :
	rm -f parse.* scan.* *.o || true

.PHONY : all clobber clean
