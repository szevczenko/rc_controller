CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -g \
            -Istubs \
            -Iunity/src \
            -I../components/rc_protocol/include \
            -I../components/rc_core/include

UNITY_SRC := unity/src/unity.c
TESTS     := test_rc_protocol test_rc_core

.PHONY: all test clean

all: $(TESTS)

test: $(TESTS)
	@for t in $(TESTS); do echo "--- $$t ---"; ./$$t; done

test_rc_protocol: test_rc_protocol.c \
                  ../components/rc_protocol/src/rc_protocol.c \
                  $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_rc_core: test_rc_core.c \
              ../components/rc_core/src/rc_core.c \
              $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TESTS)
