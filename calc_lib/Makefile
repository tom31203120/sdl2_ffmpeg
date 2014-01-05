#
# http://www.gnu.org/software/make/manual/make.html
#
CC:=gcc
INCLUDES:=$(shell pkg-config --cflags)

CFLAGS:=-Wall -ggdb 
LDFLAGS:=libportaudio.a  -lm -lpthread -lrt -lasound -ljson
EXE:=demo.out

#
# This is here to prevent Make from deleting secondary files.
#
.SECONDARY:
	

#
# $< is the first dependency in the dependency list
# $@ is the target name
#
all: dirs $(addprefix bin/, $(EXE)) tags

dirs:
	mkdir -p obj
	mkdir -p bin

tags: *.c
	ctags *.c

bin/demo.out: obj/demo.o obj/calc_score.o
	$(CC) $(CFLAGS) -o $@ $< obj/calc_score.o $(LDFLAGS)

#gcc -o t obj/demo.o obj/calc_score.o libportaudio.a  -lm -lpthread -lrt -lasound

obj/%.o : %.c
	$(CC) $(CFLAGS) $< $(INCLUDES) -c -o $@

clean:
	rm -f obj/*
	rm -f bin/*
	rm -f tags

