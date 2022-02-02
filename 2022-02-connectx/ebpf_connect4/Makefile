CLANG:=~/src/clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-20.04/
LINUX:=/home/marek/src/linux
BPFTOOL:=$(LINUX)/tools/bpf/bpftool/bpftool


attach: connect4_ebpf.o
	sudo $(BPFTOOL) prog load connect4_ebpf.o /sys/fs/bpf/connect4_prog  type cgroup/connect4
	sudo $(BPFTOOL) cgroup attach /sys/fs/cgroup/unified/user.slice connect4 pinned /sys/fs/bpf/connect4_prog

detach:
	sudo $(BPFTOOL) cgroup detach /sys/fs/cgroup/unified/user.slice connect4 pinned /sys/fs/bpf/connect4_prog
	sudo rm /sys/fs/bpf/connect4_prog


connect4_ebpf.o: connect4_ebpf.c Makefile
	$(CLANG)/bin/clang \
		-I$(LINUX)/tools/include/uapi \
	 	-I$(LINUX)/tools/lib \
		connect4_ebpf.c \
		-g -O2 -Wall -Wextra \
	 	-target bpf \
		-c -o $@
