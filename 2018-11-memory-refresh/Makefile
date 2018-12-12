all:
	gcc -msse4.1 -ggdb -O3 -Wall -Wextra measure-dram.c -o measure-dram
	objdump -dx measure-dram | egrep 'clflush' -A 2 -B 2
	./measure-dram | python3 ./analyze-dram.py
