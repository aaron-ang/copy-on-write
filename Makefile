override CFLAGS := -Wall -Werror -std=gnu99 -pedantic -O0 -g -pthread $(CFLAGS)
override LDLIBS := -pthread $(LDLIBS)

TESTDIR=tests
test_files=test_create test_read test_write \
 test_write_read test_clone test_clone_read \
 test_one_cow test_many_cow test_edge_cases \

test_files := $(addprefix $(TESTDIR)/,$(test_files))
objects := $(addsuffix .o,$(test_files))

tls.o: tls.c

all: check

.PHONY: clean check checkprogs

# Run the test programs
check: checkprogs
	/bin/bash run_tests.sh $(test_files)

# Build all of the test programs
checkprogs: $(test_files)

$(test_files): %: %.o tls.o

$(objects): %.o: %.c

clean:
	rm -f *.o *~ $(TESTDIR)/*.o $(test_files)
