CFLAGS = $(shell pkg-config --cflags libdaemon)
LDFLAGS = $(shell pkg-config --libs libdaemon)

CFLAGS += -Wall

OBJECTS = main.o udprx.o

udprx: $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	$(RM) $(OBJECTS) udprx
