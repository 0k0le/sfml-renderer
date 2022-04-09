#
# Build file
#

CC = g++
CFLAGS = -DDEBUG -std=gnu++2a -Wall -Wextra -c -O2 -pedantic -DBASE_DIR=\"`pwd`/\" -o
LIBS = `pkg-config --libs sfml-all` `pkg-config --libs x11` `pkg-config --libs xcb` `pkg-config --libs xcb-randr`

SRCDIR = src
OBJDIR = bin-int
BINDIR = bin

SFML-TEST-CPP = ${SRCDIR}/sfml-test.cpp
SFML-TEST-OBJ = ${OBJDIR}/sfml-test.o

OBJ = ${SFML-TEST-OBJ}

build: prereq ${SFML-TEST-OBJ}
	${CC} ${OBJ} ${LIBS} -o ${BINDIR}/sfml-test

prereq:
	mkdir -p ${OBJDIR}
	mkdir -p ${BINDIR}

clean:
	rm -rf ${OBJDIR} ${BINDIR}

${SFML-TEST-OBJ}: ${SFML-TEST-CPP}
	${CC} ${SFML-TEST-CPP} ${CFLAGS} `pkg-config --cflags sfml-all` `pkg-config --cflags x11` `pkg-config --cflags xcb` `pkg-config --cflags xcb-randr` ${SFML-TEST-OBJ}
