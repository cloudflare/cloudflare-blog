CC=gcc
CFLAGS=-Wall -I deps/netmap/sys/ -O2
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=nm-single-rx-queue

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@rm $(OBJECTS) $(EXECUTABLE)

