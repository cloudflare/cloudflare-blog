print('''
#ifdef __x86_64__
void __attribute__((noinline)) scramble_btb() {
\tint c = 3;''');
print('''
\tasm volatile (
\t\t".align 16, 0x90\\n"
\t\t"\tnop;nop;nop;nop;\\n"
'''[1:].rstrip())

for i in range(1024*8):
    print('''
\t\t"\tmov %%0, %%%%ecx;\\n"
\t\t"\tjmp label_%d;\\n"
\t\t"\tnop;nop;nop;nop;nop;nop;nop;nop;\\n"
\t\t"label_%d: \\n"
\t\t"\tdec %%%%ecx\\n"
\t\t"\tjnz label_%d;\\n"
'''[1:].rstrip() % (i,i, i))

print('''
		: /*no outputs*/
		: "r"(c)
		: "%ecx" );
}
''');
print("""}
#else
void __attribute__((noinline)) scramble_btb(){
\tint c = 3;\n
\tasm volatile (
\t\t".align 4\\n"
"""[1:].rstrip())

for pad in range(3):
    for i in range(pad):
        print("\"nop;\\n\"");
    for i in range(16*1024):
        print('''
\t\t"\tmov w0, %%w0;\\n"
\t\t".align 1\\n"
\t\t"label_%d_%d: \\n"
\t\t"\tsubs w0,w0, #1;\\n"
\t\t"\tbne label_%d_%d;\\n"
'''[1:].rstrip() % (pad,i,pad,i))

print("""
\t\t: /* no out */
\t\t: "r" (c)
\t\t: "w0" );
}
#endif
""")
