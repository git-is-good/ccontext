TARGETS := test
HEADERS := context.h
OBJS := context.o 

BUILDFLAGS := 
ifeq ($(DEBUG), 1)
	BUILDFLAGS += -g
endif

all: $(TARGETS)

%.o: %.c $(HEADERS)
	gcc -c $(BUILDFLAGS) -o $@ $<

test: $(OBJS)
	gcc -o $@ $^

clean:
	rm -rf *~ *.dSYM $(OBJS) $(TARGETS)
