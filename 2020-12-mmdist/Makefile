CC:=gcc

all: naive naive-avx2 batch-avx2 short-avx2 short-avx2-threshold short-inv short-inv-avx2 falconn-hyperplane

naive: database.txt test-vector.txt
	$(CC) -Wall -Wextra  -Wpointer-arith \
		-flto \
		mmdist-naive.c \
		utils.c \
		-O3 -o mmdist-naive
	< test-vector.txt $(PERF) ./mmdist-naive > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

naive-avx2: database.txt test-vector.txt
	$(CC) -Wall -Wextra -Wpointer-arith \
		-mavx2 \
		-flto \
		mmdist-naive-avx2.c \
		utils.c \
		-O3 -o mmdist-naive-avx2
	< test-vector.txt $(PERF) ./mmdist-naive-avx2 > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

batch-avx2: database.txt test-vector.txt
	$(CC) -Wall -Wextra -Wpointer-arith \
		-mavx2 \
		-flto \
		mmdist-batch-avx2.c \
		utils.c \
		-O3 -o mmdist-batch-avx2
	< test-vector.txt $(PERF) ./mmdist-batch-avx2 > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

short-avx2: database.txt test-vector.txt
	$(CC) -Wall -Wextra -Wpointer-arith \
		-mavx2 \
		-flto \
		mmdist-short-avx2.c \
		utils.c \
		-O3 -o mmdist-short-avx2
	< test-vector.txt $(PERF) ./mmdist-short-avx2 > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

DEFAULT_PROBES:=100
falconn: database.txt test-vector.txt
	python falconnbench.py --probes $(DEFAULT_PROBES) > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

falconn-npz: database.txt test-vector.txt
	python falconnbench.py --numpy-database database.txt.npz --probes $(DEFAULT_PROBES) > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

CROSS_PROBES=100
falconn-crosspolytope: database.txt test-vector.txt
	python falconnbench.py --params crosspolytope --probes $(CROSS_PROBES) > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

falconn-crosspolytope-npz: database.txt test-vector.txt
	python falconnbench.py --numpy-database database.txt.npz --params crosspolytope --probes $(CROSS_PROBES) > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

HYPER_PROBES=200
falconn-hyperplane: database.txt test-vector.txt
	python falconnbench.py --params hyperplane --probes $(HYPER_PROBES) > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

falconn-hyperplane-npz: database.txt test-vector.txt
	python falconnbench.py --numpy-database database.txt.npz --params hyperplane --probes $(HYPER_PROBES) > test-vector.tmp
	@diff --report-identical-files test-vector.good test-vector.tmp

THRESHOLD:=48400
short-avx2-threshold: database.txt test-vector.txt
	$(CC) -Wall -Wextra -Wpointer-arith \
		-mavx2 \
		-flto \
		mmdist-short-avx2.c \
		utils.c \
		-O3 -o mmdist-short-avx2
	< test-vector.txt $(PERF) ./mmdist-short-avx2 $(THRESHOLD) > test-vector.tmp
	@< test-vector.good python3 filtersolved.py -d $(THRESHOLD) > test-vector.good-cut
	@diff --report-identical-files test-vector.good-cut test-vector.tmp


short-inv: database.txt test-vector.txt
	$(CC) -Wall -Wextra -Wpointer-arith \
		-mavx2\
		-ggdb \
		-g\
		-flto \
		mmdist-short-inv.c \
		utils.c \
		-O3 -o mmdist-short-inv
	< test-vector.txt $(PERF) ./mmdist-short-inv $(THRESHOLD) > test-vector.tmp
	@< test-vector.good python3 filtersolved.py -d $(THRESHOLD) > test-vector.good-cut
	@diff --report-identical-files test-vector.good-cut test-vector.tmp

short-inv-avx2: database.txt test-vector.txt
	$(CC) -Wall -Wextra -Wpointer-arith \
		-mavx2\
		-ggdb \
		-g\
		-flto \
		mmdist-short-inv-avx2.c \
		utils.c \
		-O3 -o mmdist-short-inv-avx2
	< test-vector.txt $(PERF) ./mmdist-short-inv-avx2 $(THRESHOLD) > test-vector.tmp
	@< test-vector.good python3 filtersolved.py -d $(THRESHOLD) > test-vector.good-cut
	@diff --report-identical-files test-vector.good-cut test-vector.tmp



database.txt: generate
	./generate |head -n 1000000 > database.txt

test-vector.txt:
	python3 gentest.py > test-vector.txt 2> test-vector.good

generate: generate.c
	$(CC) -Wall -Wextra generate.c  -O3 -o generate

format:
	clang-format -i *.c *.h
