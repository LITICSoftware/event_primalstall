# definitions
TARGET	= main
LIBS	= -lm -lscip
CC		= gcc
CFLAGS	= -std=c99 -pedantic -Wall -g
LFLAGS	= -Wall -g

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

.PHONY: default all clean

# rules
default: $(TARGET)
all: default

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
