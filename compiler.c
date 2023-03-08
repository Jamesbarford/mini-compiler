#include <readline/history.h>
#include <readline/readline.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cstr.h"

#define PROMPT_LEN 15

enum Token {
    TK_EOF,
    TK_NUM,
    TK_OP,
    TK_LEFT,
    TK_RIGHT,
};

enum Op {
    OP_NOOP,
    OP_MUL,
    OP_DIV,
    OP_ADD,
    OP_SUB,
};

enum Precidence {
    PREC_EOF = 0,
    PREC_TERM,
    PREC_MUL,
    PREC_ADD,
    PREC_PAREN,
};

#define debug(...)                                                         \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

#define debug_panic(...)                                                   \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
        exit(EXIT_FAILURE);                                                \
    } while (0)

static char *register_suffix[] = {
    [0] = "AX",
    [1] = "CX",
    [2] = "DX",
    [3] = "BX",
    [4] = "SP",
    [5] = "BP",
    [6] = "SI",
    [7] = "DI",
};

static char *single_byte_instruction[] = {
    [0x2B] = "SUB",
    [0x03] = "ADD",
    [0x33] = "XOR",
    [0x48] = "REX",
    [0x50] = "PUSH",
    [0x58] = "POP",
    [0xB8] = "MOV", // <register>, immediate value
    [0xC3] = "RET",
    [0xF7] = "IDIV",
    [0x0F] = "2_BYTE",
};

static char *two_byte_instruction[] = {
    [0xAF] = "IMUL",
};

void
getRegister(unsigned char *reg, int mod, int reg_idx)
{
    char *suffix = register_suffix[reg_idx];
    /* 32 bit */
    reg[0] = 'E';

    /* 64 bit */
    if (mod == 0x48) {
        reg[0] = 'R';
        /* 16 bit */
    } else if (mod == 0x66) {
        memcpy(reg, suffix, 2);
        reg[2] = '\0';
        return;
    }
    memcpy(reg + 1, suffix, 2);
    reg[3] = '\0';
}

unsigned long
lex(unsigned char **input, unsigned long *parsed_number, enum Op *op)
{
    unsigned char *ptr = *input;
    unsigned long i = 0;

    while (1) {
        switch (*ptr) {
        case 0:
        case ';':
            *input = ptr;
            return TK_EOF;

        case ' ':
        case '\r':
        case '\n':
            ptr++;
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            i = 0;
            do {
                i = i * 10 + *ptr - '0';
                ptr++;
            } while (*ptr <= '9' && *ptr >= '0');
            *parsed_number = i;
            *input = ptr;
            return TK_NUM;

        case '*':
            *op = OP_MUL;
            *input = ptr + 1;
            return TK_OP;

        case '/':
            *op = OP_DIV;
            *input = ptr + 1;
            return TK_OP;

        case '+':
            *op = OP_ADD;
            *input = ptr + 1;
            return TK_OP;

        case '-':
            *op = OP_SUB;
            *input = ptr + 1;
            return TK_OP;

        case '(':
            *input = ptr + 1;
            return TK_LEFT;

        case ')':
            *input = ptr + 1;
            return TK_RIGHT;

        default:
            debug_panic("Unidentified token: '%c'\n", *ptr);
            exit(EXIT_FAILURE);
        }
    }
}

enum Precidence parse(unsigned char **input, unsigned char **dst);

void
parseTerm(unsigned char **input, unsigned char **dst, enum Precidence prec)
{
    enum Precidence next_prec;
    unsigned char *src;
    unsigned char *dst2;

    if (parse(input, dst) == PREC_TERM) {
        src = *input;
        dst2 = *dst;

        while (1) {
            next_prec = parse(&src, &dst2);
            if (PREC_MUL <= next_prec && next_prec < prec) {
                *input = src;
                *dst = dst2;
            } else {
                break;
            }
        }
    } else {
        debug_panic("Should not have reached here\n");
    }
}

void
encodeLong(unsigned char **buf, unsigned long num)
{
    unsigned char tmp[9];
    tmp[0] = (unsigned long)num >> 56UL & 0xff;
    tmp[1] = (unsigned long)num >> 48UL & 0xff;
    tmp[2] = (unsigned long)num >> 40UL & 0xff;
    tmp[3] = (unsigned long)num >> 32UL & 0xff;
    tmp[4] = (unsigned long)num >> 24UL & 0xff;
    tmp[5] = (unsigned long)num >> 16UL & 0xff;
    tmp[6] = (unsigned long)num >> 8UL & 0xff;
    tmp[7] = (unsigned long)num & 0xffUL;
    tmp[8] = '\0';
    memcpy(*buf, tmp, 8);
    *buf += sizeof(unsigned long);
}

unsigned long
decodeLong(unsigned char *tmpulong)
{
    unsigned long n = 0;
    n = ((unsigned long)tmpulong[0] << 56UL) |
            ((unsigned long)tmpulong[1] << 48UL) |
            ((unsigned long)tmpulong[2] << 40UL) |
            ((unsigned long)tmpulong[3] << 32UL) |
            ((unsigned long)tmpulong[4] << 24UL) |
            ((unsigned long)tmpulong[5] << 16UL) |
            ((unsigned long)tmpulong[6] << 8UL) | ((unsigned long)tmpulong[7]);
    return n;
}

enum Precidence
parse(unsigned char **input, unsigned char **dst)
{
    unsigned long num = 0;
    enum Op op = OP_NOOP;
    unsigned char *dst2 = *dst;

    switch (lex(input, &num, &op)) {
    case TK_EOF:
        *dst2++ = 0x58; // POP RAX;
        *dst2++ = 0xC3; // RET;

        *dst = dst2;
        return PREC_EOF;

    case TK_NUM:
        *dst2++ = 0x48; // REX
        *dst2++ = 0xB8; // MOV RAX,immediate num
        encodeLong(&dst2, num);
        *dst2++ = 0x50; // PUSH RAX
        *dst = dst2;
        return PREC_TERM;

    case TK_LEFT:
        parseTerm(input, dst, PREC_PAREN);
        if (parse(input, dst) != PREC_PAREN) {
            debug_panic("Expected parenthesis\n");
        }
        return PREC_TERM;

    case TK_RIGHT:
        return PREC_PAREN;
    case TK_OP:
        switch (op) {
        case OP_NOOP:
            return PREC_EOF;

        case OP_MUL:
            parseTerm(input, &dst2, PREC_MUL);
            *dst2++ = 0x5A; // POP RDX
            *dst2++ = 0x58; // POP RAX

            *dst2++ = 0x48; // REX
            *dst2++ = 0x0F;
            *dst2++ = 0xAF; // IMUL RAX,RDX
            *dst2++ = 0xC2;

            *dst2++ = 0x50; // PUSH RAX
            *dst = dst2;
            return PREC_MUL;

        case OP_DIV:
            parseTerm(input, &dst2, PREC_MUL);
            *dst2++ = 0x5B; // POP RBX
            *dst2++ = 0x58; // POP RAX

            *dst2++ = 0x33; // XOR RDX,RDX
            *dst2++ = 0xD2;

            *dst2++ = 0x48; // REX
            *dst2++ = 0xF7; // IDIV RBX
            *dst2++ = 0xFB;

            *dst2++ = 0x50; // PUSH RAX
            *dst = dst2;
            return PREC_MUL;

        case OP_ADD:
            parseTerm(input, &dst2, PREC_MUL);
            *dst2++ = 0x5A; // POP RDX
            *dst2++ = 0x58; // POP RAX

            *dst2++ = 0x48; // REX
            *dst2++ = 0x03; // ADD RAX,RDX
            *dst2++ = 0xC2;

            *dst2++ = 0x50; // PUSH RAX
            *dst = dst2;
            return PREC_ADD;
        case OP_SUB:
            parseTerm(input, &dst2, PREC_MUL);
            *dst2++ = 0x5A; // POP RDX
            *dst2++ = 0x58; // POP RAX

            *dst2++ = 0x48; // REX
            *dst2++ = 0x2B; // SUB RAX,RDX
            *dst2++ = 0xC2;

            *dst2++ = 0x50; // PUSH RAX
            *dst = dst2;
            return PREC_ADD;
        }
    }
    debug_panic("Should not have got here\n");
}

/* Display connection and port */
static void
cliReloadPrompt(char *prompt)
{
    unsigned int len = snprintf(prompt, PROMPT_LEN, ">>> ");
    prompt[len] = '\0';
}

cstr *
unassembleMachineCode(unsigned char *m)
{
    cstr *assembly = cstrnew();
    unsigned char *ptr = m;
    int bit_mode = 0x48;
    unsigned char reg_1[4] = { '\0' };
    unsigned char reg_2[4] = { '\0' };
    unsigned char tmp[10] = { '\0' };
    cstr *machine_instruction = cstrnew();

    while (1) {
        int machine_code = *ptr;
        char *instruction = single_byte_instruction[machine_code];
        machine_instruction = cstrCat(machine_instruction, "0x");

        if (machine_code == 0x48) {
            ptr++;
            bit_mode = 0x48;
            machine_instruction = cstrCatPrintf(machine_instruction, "%X%X",
                    (machine_code >> 4) & 0xF, machine_code & 0xF);
            machine_code = *ptr;
            instruction = single_byte_instruction[machine_code];
        }

        if (machine_code >= 0x50 && machine_code <= 0x57) {
            getRegister(reg_1, bit_mode, machine_code & 0xF - 8);
            machine_instruction = cstrCatPrintf(machine_instruction, "%X%X",
                    (machine_code >> 4) & 0xF, machine_code & 0xF);
            assembly = cstrCatPrintf(assembly, "%-*s %s", 5, "PUSH", reg_1);

        } else if (machine_code >= 0x58 && machine_code <= 0x5F) {
            getRegister(reg_1, bit_mode, machine_code & 0xF - 8);
            machine_instruction = cstrCatPrintf(machine_instruction, "%X%X",
                    (machine_code >> 4) & 0xF, machine_code & 0xF);
            assembly = cstrCatPrintf(assembly, "%-*s %s", 5, "POP", reg_1);

        } else if (machine_code >= 0xB8 && machine_code <= 0xBF) {
            getRegister(reg_1, bit_mode, machine_code & 0xF - 8);
            machine_instruction = cstrCatPrintf(machine_instruction, "%X%X",
                    (machine_code >> 4) & 0xF, machine_code & 0xF);

            ptr++;
            memcpy(tmp, ptr, 8);
            tmp[9] = '\0';

            unsigned long num = decodeLong(tmp);
            machine_instruction = cstrCatPrintf(machine_instruction, "%08X",
                    num);
            assembly = cstrCatPrintf(assembly, "%-*s %s,%ld", 5, "MOV", reg_1,
                    num);
            /* move past the number, the loop will handle the next +1 */
            ptr += 7;
        } else if (instruction &&
                machine_code != 0x0F) { /* Not equal 2 byte instruction */
            switch (machine_code) {
            case 0x48: /* REX */
                bit_mode = 0x48;
                break;
            case 0xC3: /* RET */
                assembly = cstrCatPrintf(assembly, "%s\n", instruction);
                machine_instruction = cstrCatPrintf(machine_instruction, "%X\n",
                        machine_code);
                goto out;
            case 0xF7: { /* IDIV */
                ptr++;
                unsigned int modrm = *ptr;
                unsigned int rm = modrm & 0x07;
                getRegister(reg_1, bit_mode, rm);
                machine_instruction = cstrCatPrintf(machine_instruction,
                        "%02X%02X", machine_code, modrm);
                assembly = cstrCatPrintf(assembly, "%*-s %s", 5, instruction,
                        reg_1);
                break;
            }
            case 0x33: /* XOR */
            case 0x2B: /* SUB */
            case 0x03: /* ADD */
                ptr++;
                unsigned int modrm = *ptr;
                unsigned int rm = modrm & 0x07;
                unsigned int reg = (modrm >> 3) & 0x07;
                machine_instruction = cstrCatPrintf(machine_instruction,
                        "%02X%02X", machine_code, modrm);
                getRegister(reg_1, bit_mode, reg);
                getRegister(reg_2, bit_mode, rm);
                assembly = cstrCatPrintf(assembly, "%-*s %s,%s", 5, instruction,
                        reg_1, reg_2);
                break;
            }
        } else if (machine_code == 0x0F) { /* Only catering for IMUL */
            ptr++;
            machine_instruction = cstrCatPrintf(machine_instruction, "%X",
                    machine_code);
            machine_code = *ptr;
            instruction = two_byte_instruction[machine_code];

            switch (machine_code) {
            case 0xAF: /* IMUL */
                ptr++;
                unsigned int modrm = *ptr;
                unsigned int rm = modrm & 0x07;
                unsigned int reg = (modrm >> 3) & 0x07;
                machine_instruction = cstrCatPrintf(machine_instruction,
                        "%02X%02X", machine_code, modrm);
                getRegister(reg_1, bit_mode, reg);
                getRegister(reg_2, bit_mode, rm);
                assembly = cstrCatPrintf(assembly, "%-*s %s,%s", 5, instruction,
                        reg_1, reg_2);
                break;
            }
        }
        assembly = cstrCatPrintf(assembly, "\n");
        machine_instruction = cstrCatPrintf(machine_instruction, "\n");
        ptr++;
    }

out:
    printf("Assembly: \n%s\n--\n", assembly);
    printf("Machine: \n%s\n--\n", machine_instruction);

    return assembly;
}

void
shellcodePrint(unsigned char *shellcode, unsigned char term)
{
    printf("Shellcode: \n");
    unsigned char *p;
    for (p = shellcode; *p != term; ++p) {
        printf("\\x%02x", *p);
    }
    printf("\\x%02x\n", *p);
}

int
main(void)
{
    printf("Mini Compiler Masquerading as a Calculator\nv0.0.1 2023\n");
    unsigned char *src;
    unsigned char *dst;
    unsigned char *shellcode = malloc(sizeof(char) * 2048);
    char prompt[PROMPT_LEN] = { '\0' };

    rl_bind_key('\t', rl_complete);

    cliReloadPrompt(prompt);
    while (1) {
        unsigned char *input = (unsigned char *)readline(prompt);

        if (!input) {
            break;
        }

        if (*input != '\0') {
            add_history((char *)input);
        }

        int len = strlen((char *)input);
        input[len] = '\0';

        if (input && len) {
            src = input;
            dst = shellcode;
            parseTerm(&src, &dst, PREC_PAREN);
            if (parse(&src, &dst) != PREC_EOF) {
                debug("maybe an error!\n");
            }

            unassembleMachineCode(shellcode);
            shellcodePrint(shellcode, 0xC3);
        } else {
            free(input);
            break;
        }

        /*
        int value = 0;
        int (*func)() = (int(*)())shellcode;
        value = func();
        printf("%d\n", value);

        memset(shellcode, '\0', 2048);
        */
        free(input);
        cliReloadPrompt(prompt);
    }

    free(shellcode);
}
