#
# Build file
#

CC = g++
CFLAGS = -DDEBUG -std=gnu++2a -Wall -Wextra -c -O2 -pedantic -o
LIBS = `pkg-config --libs sfml-all` -lX11

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
	${CC} ${SFML-TEST-CPP} ${CFLAGS} `pkg-config --cflags sfml-all` ${SFML-TEST-OBJ}
