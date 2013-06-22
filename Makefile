
CFLAGS ?= -O2 -ggdb3

all: unwind_safeness_helper.so test

unwind_safeness_helper.o: CFLAGS += -fpic -pthread

unwind_safeness_helper.so: unwind_safeness_helper.o
	$(CC) -shared -o $@ $^ -lunwind -ldl -lpthread

unwind_safeness_test_so.so: unwind_safeness_test.o
	$(CC) -shared -o $@ $^ -lunwind -ldl -lpthread

unwind_safeness_test: unwind_safeness_test_main.o unwind_safeness_test_so.so
	$(CC) -o $@ $^

%.s: %.c
	$(CC) -S -fverbose-asm $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	rm -rf unwind_safeness_helper.{o,so} unwind_safeness_test{,.o}
	rm -rf unwind_safeness_test_main.o unwind_safeness_test_so.so

unwind_safeness_test.o unwind_safeness_test.s: CFLAGS += -fpic

test: test_without_helper test_with_helper
test: test_fails_without_4 test_fails_without_2

test_deps: unwind_safeness_test unwind_safeness_helper.so

test_with_helper: test_deps
	LD_PRELOAD=./unwind_safeness_helper.so LD_LIBRARY_PATH="$(PWD)" ./unwind_safeness_test

test_without_helper: test_deps
	EXPECT_NO_HELPER=1 LD_LIBRARY_PATH="$(PWD)" ./unwind_safeness_test

# the following tests that we actually fail tests without working
# helper. Note testmask of 1 passes because it merely tests if unwind
# works without asserting that helper is "helpful"

test_fails_without_2: test_deps
	@echo "expecting failure for second test case without helper"
	ulimit -c 0 && if UNWIND_PRETEND_SAFE=1 LD_LIBRARY_PATH="$(PWD)" ./unwind_safeness_test 2 ; then false; else echo "got expected crash!"; fi

test_fails_without_4: test_deps
	@echo "expecting failure for third test case without helper"
	ulimit -c 0 && if UNWIND_PRETEND_SAFE=1 LD_LIBRARY_PATH="$(PWD)" ./unwind_safeness_test 4 ; then false; else echo "got expected crash!"; fi
