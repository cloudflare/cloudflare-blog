CLANG ?= clang

all: xdp_pass.o

%.o: %.c
	$(CLANG) -ggdb -O2 -Wall -Wextra -target bpf -c $<
