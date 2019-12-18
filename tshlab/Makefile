#
# Makefile for the CS:APP Shell Lab
#
# Type "make" to build your shell and driver
#
CC = /usr/bin/gcc
CFLAGS = -Wall -g -O2 -Werror -std=gnu99 -D_FORTIFY_SOURCE=1 -I.
LDLIBS = -lpthread
WRAPCFLAGS = \
	-Wl,--wrap=fork \
	-Wl,--wrap=kill \
	-Wl,--wrap=killpg \
	-Wl,--wrap=waitpid \
	-Wl,--wrap=execve \
	-Wl,--wrap=tcsetpgrp \
	-Wl,--wrap=signal \
	-Wl,--wrap=sigaction \
	-Wl,--wrap=sigsuspend \
	-Wl,--wrap=sigprocmask \
	-Wl,--wrap=printf \
	-Wl,--wrap=fprintf \
	-Wl,--wrap=sprintf \
	-Wl,--wrap=snprintf \
	-Wl,--wrap=init_job_list \
	-Wl,--wrap=job_get_pid \
	-Wl,--wrap=job_set_state

# Auxiliary programs
HELPER_PROGS := \
	myspin1 \
	myspin2 \
	myenv \
	myintp \
	myints \
	mytstpp \
	mytstps \
	mysplit \
	mysplitp \
	mycat \
	mysleepnprint \
	mysigfun \
	mytstpandspin \
	myspinandtstps \
	myusleep

HELPER_PROGS := $(HELPER_PROGS:%=testprogs/%)

FILES = sdriver runtrace tsh $(HELPER_PROGS)
DEPS = config.h csapp.h tsh_helper.h

all: $(FILES)

format:
	-clang-format -style=llvm -i tsh.c

%.o: %.c $(DEPS) format
	$(CC) $(CFLAGS) -c $<

#
# Using link-time interpositioning to introduce non-determinism in the
# order that parent and child execute after invoking fork
#
tsh: tsh.c wrapper.o csapp.o tsh_helper.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(WRAPCFLAGS) $(LDLIBS)

sdriver: sdriver.o
runtrace: runtrace.o csapp.o

# Clean up
clean:
	rm -f *.o *~ $(FILES)

# Create Hand-in
handin:
	#tar cvf handin.tar tsh.c key.txt
	@echo 'Do not submit a handin.tar file to Autolab. Instead, upload your tsh.c file directly.'

.PHONY: all clean handin
