CC = gcc
CLANG ?= clang
CFLAGS ?= -g -O2 -Wall -Wextra -Werror
CROSS_COMPILE ?= aarch64-linux-gnu-

BPF_FS ?= /sys/fs/bpf
BPF_OBJS := $(addprefix $(BPF_FS)/,entry_prog target_prog jmp_table)

.PHONY: all
all: fib_okay fib_cool fib_okay.arm64 fib_cool.arm64

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

fib_okay: fib_main.o fib_okay.o
	$(CC) $(CFLAGS) -o $@ $^

fib_cool: fib_main.o fib_cool.o
	$(CC) $(CFLAGS) -o $@ $^

%.arm64.o: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c -o $@ $<

fib_okay.arm64: fib_main.arm64.o fib_okay.arm64.o
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $^

fib_cool.arm64: fib_main.arm64.o fib_cool.arm64.o
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $^

%.o: %.bpf.c
	$(CLANG) $(CFLAGS) -target bpf -c $< -o $@

.PHONY: $(BPF_FS)
$(BPF_FS):
	mountpoint -q $@ || mount -t bpf none $@

.PHONY: unload
unload:
	for obj in $(BPF_OBJS); do [ -f $$obj ] && rm $$obj; done

.PHONY: load-ex1
load-ex1: tail_call_ex1.o | $(BPF_FS)
	bpftool prog loadall $< $(BPF_FS) pinmaps $(BPF_FS)
	bpftool map update pinned $(BPF_FS)/jmp_table key 0 0 0 0 value pinned $(BPF_FS)/target_prog

.PHONY: load-ex2
load-ex2: tail_call_ex2.o | $(BPF_FS)
	bpftool prog loadall $< $(BPF_FS) pinmaps $(BPF_FS)
	bpftool map update pinned $(BPF_FS)/jmp_table key 0 0 0 0 value pinned $(BPF_FS)/target_prog
