package main

import (
	"testing"
)

func TestFilterALU64(t *testing.T) { testFilter(t, "alu64") }
func TestFilterALU32(t *testing.T) { testFilter(t, "alu32") }
func TestFilterIR(t *testing.T)    { testFilter(t, "ir") }
func TestFilterSTV(t *testing.T)   { testFilter(t, "stv") }

func testFilter(t *testing.T, filterName string) {
	ctx, err := loadBPF(filterName)
	if err != nil {
		t.Fatalf("loadBPF: %v", err)
	}
	defer ctx.Close()

	testSub64(t, ctx)
}

func testSub64(t *testing.T, ctx *Context) {
	tests := []struct {
		A, B uint64
	}{
		{0, 0},
		{1, 0},
		{5, 2},
		{7, 13},

		{(2 << 32) - 1, 0},
		{(2 << 32), 0},
		{(2 << 32) + 1, 0},

		{(2 << 32) - 1, 1},
		{(2 << 32), 1},
		{(2 << 32) + 1, 1},

		{(2 << 32) + 1, (2 << 32) - 1},
		{(2 << 32) + 1, (2 << 32)},
		{(2 << 32) + 1, (2 << 32) + 1},

		{(2 << 32) + 2, (2 << 32) + 1},
	}

	for _, test := range tests {
		testOneSub64(t, ctx, test.A, test.B)
		testOneSub64(t, ctx, test.B, test.A)
	}
}

func testOneSub64(t *testing.T, ctx *Context, arg0, arg1 uint64) {
	diff, err := runBPF(ctx, arg0, arg1)
	if err != nil {
		t.Fatalf("runBPF(%#x, %#x) failed: %v", arg0, arg1, err)
	}
	want := arg0 - arg1
	if diff != want {
		t.Fatalf("runBPF(%#x, %#x) = %#x, want %#x", arg0, arg1, diff, want)
	}

}
