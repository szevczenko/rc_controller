CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -g \
            -Istubs \
            -Iunity/src \
            -I../components/rc_protocol/include \
            -I../components/rc_core/include

UNITY_SRC := unity/src/unity.c

.PHONY: all clean

all: test_rc_protocol
	./test_rc_protocol

test_rc_protocol: test_rc_protocol.c \
                  ../components/rc_protocol/src/rc_protocol.c \
                  $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f test_rc_protocol
