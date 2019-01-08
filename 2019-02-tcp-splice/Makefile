all: venv/.ok
	clang -g -Wall -Wextra -O2 \
		net.c \
		test-burst.c \
		-o test-burst

	clang -g -Wall -Wextra -O2 \
		net.c \
		echo-naive.c \
		-o echo-naive

	clang -g -Wall -Wextra -O2 \
		net.c \
		echo-iosubmit.c \
		-o echo-iosubmit

	clang -g -Wall -Wextra -O2 \
		net.c \
		echo-splice.c \
		-o echo-splice

	clang -Wall -Wextra -O2 -emit-llvm -c \
		echo-sockmap-kern.c \
		-S -o - \
		| llc -march=bpf -filetype=obj -o - \
		| ./venv/bin/python3 tbpf-decode-elf.py /dev/stdin \
			prog_parser prog_verdict \
		> echo-sockmap-ebpf.c

	clang -g -Wall -Wextra -O2 \
		tbpf.c \
		net.c \
		echo-sockmap-ebpf.c \
		echo-sockmap.c \
		-l elf \
		-o echo-sockmap

venv/.ok:
	virtualenv venv --python=python3
	./venv/bin/pip3 install pyelftools
	touch $@

.PHONY: format
format:
	clang-format -i *.c *.h
	@grep -n "TODO" *.[ch] || true
