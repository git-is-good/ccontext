TARGETS := test
HEADERS := context.h
OBJS := context.o test.o

BUILDFLAGS := 
ifeq ($(DEBUG), 1)
	BUILDFLAGS += -g -DDEBUG_CONTEXT
endif

all: $(TARGETS)

%.o: %.c $(HEADERS)
	gcc -c $(BUILDFLAGS) -o $@ $<

test: $(OBJS)
	gcc -o $@ $^

clean:
	rm -rf *~ *.dSYM $(OBJS) $(TARGETS)
