

CC = gcc  


IFLAGS = -I/comp/40/build/include -I/usr/sup/cii40/include/cii

CFLAGS = -g -std=gnu99 -Wall -Wextra -Werror -Wfatal-errors -pedantic $(IFLAGS)



LDFLAGS = -g -L/comp/40/build/lib -L/usr/sup/cii40/lib64 -lnsl


LDLIBS = -l40locality -lnetpbm -lcii40 -lm -lrt


src = $(wildcard *.c)
obj = $(src:.c=.o)


all: a.out


a.out: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(obj) a.out
