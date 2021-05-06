all: run

MODEL:=$(shell lscpu | grep "Model "|cut -d ":" -f 2 | tr -s " ")

.PHONY: run
run: branch
	@echo "[*] $(MODEL)"
	./branch
	gdb ./branch \
		-ex "break blob_trap" \
		-ex "r -a4 --count=2 --type=jmp" \
		-ex "disassemble /r blob->ptr_fn,+0x50" \
		-ex "stop" \
		-ex "quit"

branch: scramble.c perf.c gadget.c main.c branch.h
	clang-9 -g -flto -O3 -Wall -Wextra scramble.c perf.c gadget.c main.c -o branch


scramble.c: scramble.py
	python3 scramble.py > scramble.c

.PHONY: format fmt
format: fmt
fmt:
	clang-format -i scramble.c perf.c gadget.c main.c branch.h

