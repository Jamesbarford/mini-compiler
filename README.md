# Mini Compiler Masquerading as a Calculator (MCMC)

I found `MiniCompiler.HC` in a TempleOS demo folder and decided that it needed a fresh lick of paint.

The outcome of which is a program that takes simple integer arithmetic and spits out highly unoptimised Assembly, Machine and shellcode. Then subsequently fails to perform the arithmetic you wanted it to *perform.

## Example
Run `make` to compile the code. Then run `./mcmc`

```sh
>>> 4 + 4
```

This will print the following to stdout:

```bash
Assembly:
MOV   RAX,4
PUSH  RAX
MOV   RAX,4
PUSH  RAX
POP   RDX
POP   RAX
ADD   RAX,RDX
PUSH  RAX
POP   RAX
RET

--
Machine:
0x48B800000004
0x50
0x48B800000004
0x50
0x5A
0x58
0x4803C2
0x50
0x58
0xC3

--
Shellcode:
\x48\xb8\x00\x00\x00\x00\x00\x00\x00\x04\x50\x48\xb8\x00\x00\x00\x00\x00\x00\x00\x04\x50\x5a\x58\x48\x03\xc2\x50\x58\xc3
```

## References
- Original MiniCompiler.HC [MiniCompiler](https://github.com/cia-foundation/TempleOS/blob/archive/Demo/Lectures/MiniCompiler.HC)
- sds inspiration for the `cstr.c` implementation [sds](https://github.com/antirez/sds)

## *Shellcode
I cannot get the shellcode to execute but feel it should? I have the following `c` code, help appreciated:
```c
static unsigned char *example_shell_code = "\x48\xb8\x00\x00\x00\x00\x00\x00\x00\x04\x50\x48\xb8\x00\x00\x00\x00\x00\x00\x00\x04\x50\x5a\x58\x48\x03\xc2\x50\x58\xc3";

void
example(void) {
    int value = 0;
    int (*func)() = (int(*)())shellcode;
    value = func();
    printf("%d\n", value);
}
```
