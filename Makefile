TARGET = ww
SRC    = $(TARGET).c
CC     = gcc
CFLAGS = -I. -g -Wall -Wvla -Werror -fsanitize=address,undefined -pthread
OBJ = wordwrap.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf $(TARGET) *.o *.a *.dylib *.dSYM
