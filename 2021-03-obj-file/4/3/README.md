# Example on how to import code from an object file without linking on ARM

1. Compile, but not link `obj.c`:

```bash
$ gcc -c obj.c
```

2. Compile and link `loader.c`:

```bash
$ gcc -o loader loader.c
```

3. Execute `loader` (`obj.o` has to be present in the same directory):

```bash
$ ./loader 
Executing add5...
add5(42) = 47
Executing add10...
add10(42) = 52
Executing get_hello...
get_hello() = Hello, world!
Executing get_var...
get_var() = 5
Executing set_var(42)...
Executing get_var again...
get_var() = 42
Executing say_hello...
my_puts executed
Hello, world!
```