program=nrfprog
OBJ=main.o
LIBS=-lpthread -lusb
CFLAGS=-O2

.PHONY: build
.PHONY: clean

build: ${program}

clean:
	rm -f *.o ${program}

${program}: ${OBJ}
	gcc ${OBJ} -o ${program} ${CFLAGS} ${LIBS}

%.o: %.c
	gcc -c -o $@ $< $(CFLAGS)
