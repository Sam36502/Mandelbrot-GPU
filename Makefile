#
# SDL Makefile
#


BIN = mandelbrot.exe
SRC = main.c gl.c demo.c

CC = gcc
CFLAGS = -Wall -g
LIBS = mingw32 SDL2main SDL2 glew32 opengl32

REL_CFLAGS = -Wall -O2

run: build
	@echo -e '### Running... ###\n'
	@./${BIN}

build:
	@echo '### Building... ###\n'
	${CC} ${CFLAGS} -o ${BIN} ${SRC} $(addprefix -l,${LIBS})

