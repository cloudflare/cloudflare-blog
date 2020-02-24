CC := clang

all: mmuniq-bloom mmuniq-hash mmuniq

mmuniq-bloom: *c Makefile
	$(CC) \
		-g -ggdb -O3 \
		-Wall -Wextra -Wpointer-arith \
		-D_FORTIFY_SOURCE=2 -fPIE \
		mmuniq-bloom.c \
		-lm \
		-Wl,-z,now -Wl,-z,relro \
		-o $@

mmuniq-hash: *c Makefile
	$(CC) \
		-g -ggdb -O3 \
		-Wall -Wextra -Wpointer-arith \
		-D_FORTIFY_SOURCE=2 -fPIE \
		mmuniq-hash.c \
		-Wl,-z,now -Wl,-z,relro \
		-o $@

mmuniq: *c Makefile
	$(CC) \
		-g -ggdb -O3 \
		-Wall -Wextra -Wpointer-arith \
		-D_FORTIFY_SOURCE=2 -fPIE \
		mmuniq.c \
		-maes \
		-Wl,-z,now -Wl,-z,relro \
		-o $@

.PHONY: format
format:
	clang-format -i *.c
