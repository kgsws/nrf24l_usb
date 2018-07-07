program=nrf_gtk
OBJ=main.o protocol.o
LIBS=-lpthread -lusb `pkg-config --libs gtk+-3.0`
#CFLAGS=-O2 `pkg-config --cflags gtk+-3.0`
CFLAGS=-g `pkg-config --cflags gtk+-3.0`

build: ${program}

clean:
	rm -f *.o ${program}

${program}: ${OBJ}
	gcc ${OBJ} -o ${program} ${CFLAGS} ${LIBS}

%.o: %.c
	gcc -c -o $@ $< $(CFLAGS)

