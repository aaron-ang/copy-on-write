override CFLAGS := -Wall -Werror -std=gnu99 -pedantic -O0 -g -pthread $(CFLAGS)
override LDLIBS := -pthread $(LDLIBS)

TESTDIR=tests
test_files=test_cow test_simple_create test_simple_write

test_files := $(addprefix $(TESTDIR)/,$(test_files))
objects := $(addsuffix .o,$(test_files))

tls.o: tls.c

all: check

# rules to build each of the tests
$(objects): %.o: %.c

$(test_files): %: %.o tls.o

.PHONY: clean check checkprogs

# Run the test programs
check: checkprogs
    /bin/bash run_tests.sh $(test_files)

# Build all of the test programs
checkprogs: $(test_files)

clean:
	rm -f *.o *~ $(TESTDIR)/*.o $(test_files)
