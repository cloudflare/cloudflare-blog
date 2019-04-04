#pragma once

/* stdint.h pulls in architecture specific headers on some distros */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;

/* Emits a "load volatile" in LLVM IR */
#define LD_V(var)      (*(volatile typeof(var) *) &(var))

/* Emits a "store volatile" in LLVM IR */
#define ST_V(rhs, lhs) (*(volatile typeof(rhs) *) &(rhs) = (lhs))
