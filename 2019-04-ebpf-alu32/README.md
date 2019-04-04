# Code for "eBPF can't count!?" blog

## Building

Run [`ninja`](https://ninja-build.org/) to build `run-bpf`.

## Running

`run-bpf` loads the socket filter and runs it with supplied command line
arguments. It takes an optional name of the filter variant to use.

```
$ ./run-bpf
Usage: run-bpf [options] uint64 uint64

Options:
  -filter string
        Filter variant to load: "alu64", "alu32", "ir", "stv" (default "alu64")
  -stop-after-load
        Stop the process after loading BPF program
```

For example, calcualte 2^32 - 1 with a filter using ALU64 ops:

```
$ ./run-bpf -filter alu64 $[2**32] 1
arg0           4294967296 0x0000000100000000
arg1                    1 0x0000000000000001
diff 18446744073709551615 0xffffffffffffffff
```

Perform the same calculation using a filter coded in IR:

```
$ ./run-bpf -filter ir $[2**32] 1
arg0           4294967296 0x0000000100000000
arg1                    1 0x0000000000000001
diff           4294967295 0x00000000ffffffff
```

See `run-bpf -help` for a list of available filters.

## Testing

Run `go test` to run tests:

```
$ go test
=== RUN   TestFilterALU64
--- FAIL: TestFilterALU64 (0.00s)
    run_bpf_test.go:59: runBPF(0x1ffffffff, 0x0) = 0x0, want 0x1ffffffff
=== RUN   TestFilterALU32
--- FAIL: TestFilterALU32 (0.00s)
    run_bpf_test.go:59: runBPF(0x1ffffffff, 0x0) = 0x100000000, want 0x1ffffffff
=== RUN   TestFilterIR
--- PASS: TestFilterIR (0.00s)
=== RUN   TestFilterSTV
--- PASS: TestFilterSTV (0.00s)
FAIL
exit status 1
FAIL    _/home/jkbs/src/cloudflare-blog/2019-04-ebpf-alu32      0.007s
```
