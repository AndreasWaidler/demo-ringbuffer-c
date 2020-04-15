CC=     gcc
LD=     ld
CFLAGS= -g -W -Wall -Werror -std=c99
LDFLAGS= -pthread
TARGET= ringbuffer
SRC=    ringbuffer.c
OBJ=    $(SRC:.c=.o)

ringbuffer: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(TARGET) *.o
