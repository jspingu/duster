#include <stdio.h>
#include "Duster.h"

#define WHITESPACE         " \n\t"
#define DIGIT              "0123456789"
#define COMPILE_CTRL       "{}[]()"
#define DT_MAX_SYMBOL_LEN  64

#define DT_DEFINE_BINOP(type,name,op)                                                               \
    static void name(DT_System *sys) {                                                              \
        type lhs, rhs;                                                                              \
        if (!(DT_PopCell(sys, sys->param_stack, &rhs) && DT_PopCell(sys, sys->param_stack, &lhs)))  \
            return;                                                                                 \
        type res = lhs op rhs;                                                                      \
        DT_PushCell(sys->param_stack, &res);                                                        \
    }

/* VM */

bool DT_GetCell(DT_System *sys, List(DT_Cell) *stack, uint32_t idx, void *out) {
    if (idx >= List_Length(stack)) {
        sys->halt = sys->err = true;
        fprintf(stderr, "Attempt to read from out-of-bounds stack address\n");
        return false;
    }

    if (out)
        SDL_memcpy(out, List_GetAddress(stack, idx), sizeof(DT_Cell));

    return true;
}

bool DT_SetCell(DT_System *sys, List(DT_Cell) *stack, uint32_t idx, void *in) {
    if (idx >= List_Length(stack)) {
        sys->halt = sys->err = true;
        fprintf(stderr, "Attempt to write to out-of-bounds stack address\n");
        return false;
    }

    SDL_memcpy(List_GetAddress(stack, idx), in, sizeof(DT_Cell));
    return true;
}

void DT_PushCell(List(DT_Cell) *stack, void *in) {
    List_PushRange(stack, in, 1);
}

bool DT_PopCell(DT_System *sys, List(DT_Cell) *stack, void *out) {
    if (!List_Length(stack)) {
        sys->halt = sys->err = true;
        fprintf(stderr, "Stack underflow\n");
        return false;
    }

    if (out)
        SDL_memcpy(out, List_GetAddress(stack, List_Length(stack) - 1), sizeof(DT_Cell));

    List_Drop(stack);
    return true;
}

void DT_PushString(List(DT_Cell) *stack, char *in) {
    uint32_t len = SDL_strlen(in);
    uint32_t cells = len / sizeof(DT_Cell) + 1;
    uint32_t pad = sizeof(DT_Cell) - len % sizeof(DT_Cell);
    char *str = List_PushSpace(stack, cells);

    for (uint32_t i = 0; i < pad; ++i)
        str[i] = '\0';

    for (uint32_t i = 0; i < len; ++i)
        str[pad + i] = in[len - i - 1];
}

bool DT_PopString(DT_System *sys, List(DT_Cell) *stack, char *out, uint32_t maxlen) {
    char cell[sizeof(DT_Cell)];
    uint32_t len = 0;

    while (DT_PopCell(sys, stack, cell)) {
        for (uint32_t i = 0; i < sizeof(DT_Cell); ++i, ++len) {
            char chr = cell[sizeof(DT_Cell) - i - 1];

            if (len < maxlen)
                out[len] = chr;

            if (chr == '\0')
                return true;
        }
    }

    return false;
}

bool DT_Strlen(DT_System *sys, List(DT_Cell) *stack, uint32_t *out) {
    char cell[sizeof(DT_Cell)];
    uint32_t len = 0;

    for (uint32_t i = 0; DT_GetCell(sys, stack, List_Length(stack) - i - 1, cell); ++i) {
        for (uint32_t j = 0; j < sizeof(DT_Cell); ++j, ++len) {
            char chr = cell[sizeof(DT_Cell) - j - 1];

            if (chr == '\0') {
                *out = len;
                return true;
            }
        }
    }

    return false;
}

bool DT_Start(DT_System *sys, uint32_t cfa) {
    /**
     * Data stack layout:
     *
     * +---+--------------------------+
     * | 0 | `nest` code pointer      |
     * +---+--------------------------+
     * | 1 | CFA of the current word  |
     * +---+--------------------------+
     * | 2 | CFA of the `unnest` word |
     * +---+--------------------------+
     */

    uint32_t code_field;
    sys->cfa = 0;
    sys->halt = sys->err = false;
    DT_SetCell(sys, sys->data_stack, 1, &cfa);

    while (true) {
        if (!DT_GetCell(sys, sys->data_stack, sys->cfa, &code_field))
            break;

        if (code_field >= List_Length(sys->native_fns)) {
            sys->halt = sys->err = true;
            fprintf(stderr, "Call to invalid code pointer\n");
            break;
        }
            
        List_Get(sys->native_fns, code_field)(sys);

        if (sys->halt || !DT_GetCell(sys, sys->data_stack, sys->pc, &sys->cfa))
            break;

        sys->pc += 1;
    }

    return !sys->err;
}

void DT_AddNativeFunction(DT_System *sys, char *name, void (*fn)(DT_System *)) {
    uint32_t cfa = List_Length(sys->data_stack);
    uint32_t code_ptr = List_Length(sys->native_fns);
    Strmap_Set(sys->symbols, name, cfa);
    DT_PushCell(sys->data_stack, &code_ptr);

    /* If native function is called with `call` */
    DT_PushCell(sys->data_stack, &cfa);
    DT_PushCell(sys->data_stack, &sys->unnest_cfa);

    List_Push(sys->native_fns, fn);
}

/* Core */

static void nest(DT_System *sys) {
    DT_PushCell(sys->call_stack, &sys->pc);
    sys->pc = sys->cfa + 1;
}

static void unnest(DT_System *sys) {
    DT_PopCell(sys, sys->call_stack, &sys->pc);

    if (!List_Length(sys->call_stack))
        sys->halt = true;
}

static void push(DT_System *sys) {
    DT_Cell cell;

    if (!DT_GetCell(sys, sys->data_stack, sys->pc++, &cell))
        return;

    DT_PushCell(sys->param_stack, &cell);
}

static void pushstr(DT_System *sys) {
    char cell[sizeof(DT_Cell)];
    char *str = (char *)List_GetAddress(sys->data_stack, sys->pc);

    while (DT_GetCell(sys, sys->data_stack, sys->pc++, cell)) {
        for (uint32_t i = 0; i < sizeof(DT_Cell); ++i) {
            if (cell[i] == '\0') {
                DT_PushString(sys->param_stack, str);
                return;
            }
        }
    }
}

static void compile(DT_System *sys) {
    uint32_t defer;
    DT_Cell cell;

    if (!(DT_GetCell(sys, sys->data_stack, sys->pc, &defer) && DT_GetCell(sys, sys->data_stack, sys->pc + 1, &cell)))
        return;

    if (defer) {
        defer -= 1;
        DT_PushCell(sys->data_stack, &sys->compile_cfa);
        DT_PushCell(sys->data_stack, &defer);
    }

    DT_PushCell(sys->data_stack, &cell);
    sys->pc += 2;
}

static void compilestr(DT_System *sys) {
    uint32_t defer;
    char *str = (char *)List_GetAddress(sys->data_stack, sys->pc + 1);

    if (!DT_GetCell(sys, sys->data_stack, sys->pc++, &defer))
        return;

    char cell[sizeof(DT_Cell)];

    while (DT_GetCell(sys, sys->data_stack, sys->pc++, cell)) {
        for (uint32_t i = 0; i < sizeof(DT_Cell); ++i) {
            if (cell[i] == '\0') {
                if (defer) {
                    defer -= 1;
                    DT_PushCell(sys->data_stack, &sys->compilestr_cfa);
                    DT_PushCell(sys->data_stack, &defer);
                }

                uint32_t len = SDL_strlen(str);
                char *compiled = List_PushSpace(sys->data_stack, len / sizeof(DT_Cell) + 1);
                SDL_strlcpy(compiled, str, len + 1);
                return;
            }
        }
    }
}

static void symbol(DT_System *sys) {
    char name[DT_MAX_SYMBOL_LEN];

    if (!DT_PopString(sys, sys->param_stack, name, DT_MAX_SYMBOL_LEN))
        return;

    Strmap_Set(sys->symbols, name, List_Length(sys->data_stack));
}

static void symboladdr(DT_System *sys) {
    char name[DT_MAX_SYMBOL_LEN];

    if (!DT_PopString(sys, sys->param_stack, name, DT_MAX_SYMBOL_LEN))
        return;

    uint32_t *addr = Strmap_Get(sys->symbols, name);
    DT_PushCell(sys->param_stack, addr ? addr : &(uint32_t){0});
}

/* Stack */

static void dup(DT_System *sys) {
    DT_Cell cell;

    if (!DT_PopCell(sys, sys->param_stack, &cell))
        return;

    DT_PushCell(sys->param_stack, &cell);
    DT_PushCell(sys->param_stack, &cell);
}

static void drop(DT_System *sys) {
    DT_PopCell(sys, sys->param_stack, nullptr);
}

static void swap(DT_System *sys) {
    DT_Cell fst, snd;

    if (!(DT_PopCell(sys, sys->param_stack, &snd) && DT_PopCell(sys, sys->param_stack, &fst)))
        return;

    DT_PushCell(sys->param_stack, &snd);
    DT_PushCell(sys->param_stack, &fst);
}

static void dsp(DT_System *sys) {
    uint32_t top = List_Length(sys->data_stack);
    DT_PushCell(sys->param_stack, &top);
}

static void push_ds(DT_System *sys) {
    DT_Cell cell;

    if (!DT_PopCell(sys, sys->param_stack, &cell))
        return;

    DT_PushCell(sys->data_stack, &cell);
}

static void pop_ds(DT_System *sys) {
    DT_Cell cell;

    if (!DT_PopCell(sys, sys->data_stack, &cell))
        return;

    DT_PushCell(sys->param_stack, &cell);
}

static void store_ds(DT_System *sys) {
    uint32_t addr;
    DT_Cell cell;

    if (!(DT_PopCell(sys, sys->param_stack, &cell) && DT_PopCell(sys, sys->param_stack, &addr)))
        return;

    DT_SetCell(sys, sys->data_stack, addr, &cell);
}

static void fetch_ds(DT_System *sys) {
    uint32_t addr;
    DT_Cell cell;

    if (!(DT_PopCell(sys, sys->param_stack, &addr) && DT_GetCell(sys, sys->data_stack, addr, &cell)))
        return;

    DT_PushCell(sys->param_stack, &cell);
}

/* Control */

static void jmp(DT_System *sys) {
    uint32_t addr;

    if (!DT_GetCell(sys, sys->data_stack, sys->pc, &addr))
        return;

    sys->pc = addr;
}

static void call(DT_System *sys) {
    uint32_t addr;

    if (!DT_PopCell(sys, sys->param_stack, &addr))
        return;

    DT_PushCell(sys->call_stack, &sys->pc);
    sys->pc = addr + 1;
}

static void jz(DT_System *sys) {
    int32_t cond;

    if (!DT_PopCell(sys, sys->param_stack, &cond))
        return;

    if (!cond)
        jmp(sys);
    else
        sys->pc += 1;
}

/* Logical / Comparision */

static void not(DT_System *sys) {
    int32_t i;

    if (!DT_PopCell(sys, sys->param_stack, &i))
        return;

    int32_t res = !i;
    DT_PushCell(sys->param_stack, &res);
}

DT_DEFINE_BINOP(int32_t, and, &&)
DT_DEFINE_BINOP(int32_t, or, ||)
DT_DEFINE_BINOP(int32_t, eq, ==)
DT_DEFINE_BINOP(int32_t, gt, >)
DT_DEFINE_BINOP(int32_t, lt, <)

/* Arithmetic */

DT_DEFINE_BINOP(int32_t, add, +)
DT_DEFINE_BINOP(int32_t, sub, -)
DT_DEFINE_BINOP(int32_t, mul, *)
DT_DEFINE_BINOP(int32_t, div, /)
DT_DEFINE_BINOP(int32_t, rem, %)

DT_DEFINE_BINOP(float, fadd, +)
DT_DEFINE_BINOP(float, fsub, -)
DT_DEFINE_BINOP(float, fmul, *)
DT_DEFINE_BINOP(float, fdiv, /)

static void fsin(DT_System *sys) {
    float f;

    if (!DT_PopCell(sys, sys->param_stack, &f))
        return;

    float res = SDL_sinf(f);
    DT_PushCell(sys->param_stack, &res);
}

static void fcos(DT_System *sys) {
    float f;

    if (!DT_PopCell(sys, sys->param_stack, &f))
        return;

    float res = SDL_cosf(f);
    DT_PushCell(sys->param_stack, &res);
}

static void itof(DT_System *sys) {
    int32_t i;

    if (!DT_PopCell(sys, sys->param_stack, &i))
        return;

    float cvt = i;
    DT_PushCell(sys->param_stack, &cvt);
}

/* Output */

static void puti(DT_System *sys) {
    int32_t i;

    if (!DT_PopCell(sys, sys->param_stack, &i))
        return;

    printf("%i", i);
}

static void putf(DT_System *sys) {
    float f;

    if (!DT_PopCell(sys, sys->param_stack, &f))
        return;

    printf("%g", f);
}

static void putstr(DT_System *sys) {
    uint32_t len;

    if (!DT_Strlen(sys, sys->param_stack, &len))
        return;

    char *str = SDL_malloc(len + 1);
    DT_PopString(sys, sys->param_stack, str, len);
    str[len] = '\0';

    printf("%s", str);
    SDL_free(str);
}

static void nl(DT_System *sys) {
    (void)sys;
    printf("\n");
}

void DT_InitCore(DT_System *sys) {
    /* The first three cells contain a small subroutine which serves as the program's entry point */
    List_PushSpace(sys->data_stack, 3);

    sys->nest_ptr = List_Length(sys->native_fns);
    List_Push(sys->native_fns, nest);

    sys->unnest_cfa = List_Length(sys->data_stack);
    DT_AddNativeFunction(sys, "unnest", unnest);

    sys->push_cfa = List_Length(sys->data_stack);
    DT_AddNativeFunction(sys, "push", push);

    sys->pushstr_cfa = List_Length(sys->data_stack);
    uint32_t pushstr_ptr = List_Length(sys->native_fns);
    DT_PushCell(sys->data_stack, &pushstr_ptr);
    List_Push(sys->native_fns, pushstr);

    sys->compile_cfa = List_Length(sys->data_stack);
    uint32_t compile_ptr = List_Length(sys->native_fns);
    DT_PushCell(sys->data_stack, &compile_ptr);
    List_Push(sys->native_fns, compile);

    sys->compilestr_cfa = List_Length(sys->data_stack);
    uint32_t compilestr_ptr = List_Length(sys->native_fns);
    DT_PushCell(sys->data_stack, &compilestr_ptr);
    List_Push(sys->native_fns, compilestr);

    DT_SetCell(sys, sys->data_stack, 0, &sys->nest_ptr);
    DT_SetCell(sys, sys->data_stack, 2, &sys->unnest_cfa);

    DT_AddNativeFunction(sys, ":", symbol);
    DT_AddNativeFunction(sys, ":&", symboladdr);

    DT_AddNativeFunction(sys, "dup", dup);
    DT_AddNativeFunction(sys, "drop", drop);
    DT_AddNativeFunction(sys, "swap", swap);
    DT_AddNativeFunction(sys, "dsp", dsp);
    DT_AddNativeFunction(sys, "->ds", push_ds);
    DT_AddNativeFunction(sys, "<-ds", pop_ds);
    DT_AddNativeFunction(sys, "!ds", store_ds);
    DT_AddNativeFunction(sys, "@ds", fetch_ds);

    DT_AddNativeFunction(sys, "jmp", jmp);
    DT_AddNativeFunction(sys, "call", call);
    DT_AddNativeFunction(sys, "jz", jz);

    DT_AddNativeFunction(sys, "not", not);
    DT_AddNativeFunction(sys, "and", and);
    DT_AddNativeFunction(sys, "or", or);
    DT_AddNativeFunction(sys, "=", eq);
    DT_AddNativeFunction(sys, "<", lt);
    DT_AddNativeFunction(sys, ">", gt);

    DT_AddNativeFunction(sys, "+", add);
    DT_AddNativeFunction(sys, "-", sub);
    DT_AddNativeFunction(sys, "*", mul);
    DT_AddNativeFunction(sys, "/", div);
    DT_AddNativeFunction(sys, "%", rem);
    DT_AddNativeFunction(sys, "f+", fadd);
    DT_AddNativeFunction(sys, "f-", fsub);
    DT_AddNativeFunction(sys, "f*", fmul);
    DT_AddNativeFunction(sys, "f/", fdiv);
    DT_AddNativeFunction(sys, "sin", fsin);
    DT_AddNativeFunction(sys, "cos", fcos);
    DT_AddNativeFunction(sys, "itof", itof);

    DT_AddNativeFunction(sys, "puts", putstr);
    DT_AddNativeFunction(sys, "puti", puti);
    DT_AddNativeFunction(sys, "putf", putf);
    DT_AddNativeFunction(sys, "nl", nl);
}

/* Interpreter */

static void DT_CompileCell(DT_System *sys, void *cell) {
    if (sys->compilation_depth > 1) {
        uint32_t defer = sys->compilation_depth - 2;
        DT_PushCell(sys->data_stack, &sys->compile_cfa);
        DT_PushCell(sys->data_stack, &defer);
    }

    DT_PushCell(sys->data_stack, cell);
}

static void DT_CompileStr(DT_System *sys, char *str) {
    if (sys->compilation_depth > 1) {
        uint32_t defer = sys->compilation_depth - 2;
        DT_PushCell(sys->data_stack, &sys->compilestr_cfa);
        DT_PushCell(sys->data_stack, &defer);
    }

    uint32_t len = SDL_strlen(str);
    char *compiled = List_PushSpace(sys->data_stack, len / sizeof(DT_Cell) + 1);
    SDL_strlcpy(compiled, str, len + 1);
}

static void DT_SetTransition(DT_System *sys, DT_ParseState from, char *inputs, DT_ParseState to) {
    if (!inputs)
        for (int i = 0; i < 256; ++i)
            sys->transition_table[from][i] = (DT_ParserTransition) { .state=to };
    else
        for (uint8_t *chr = (uint8_t *)inputs; *chr; ++chr)
            sys->transition_table[from][*chr] = (DT_ParserTransition) { .state=to };
}

static void DT_SetCompletion(DT_System *sys, DT_ParseState from, char *inputs, bool (*callback)(DT_System *)) {
    if (!inputs)
        for (int i = 0; i < 256; ++i)
            sys->transition_table[from][i].callback = callback;
    else
        for (uint8_t *chr = (uint8_t *)inputs; *chr; ++chr)
            sys->transition_table[from][*chr].callback = callback;

    sys->transition_eof[from] = callback;
}

static bool DT_CompleteDiscard(DT_System *sys) {
    (void)sys;
    return true;
}

static bool DT_CompleteCall(DT_System *sys) {
    List_Push(sys->string, '\0');
    char *symbol = (char *)List_GetAddress(sys->string, 0);
    uint32_t *cfa = Strmap_Get(sys->symbols, symbol);
    bool res = true;

    if (!cfa) {
        fprintf(stderr, "Unknown symbol \"%s\"\n", symbol);
        return false;
    }

    if (sys->compilation_depth)
        DT_CompileCell(sys, cfa);
    else
        res = DT_Start(sys, *cfa);

    return res;
}

static bool DT_CompleteInteger(DT_System *sys) {
    List_Push(sys->string, '\0');
    int32_t i = SDL_strtoll((char *)List_GetAddress(sys->string, 0), nullptr, 10);

    if (sys->compilation_depth) {
        DT_CompileCell(sys, &sys->push_cfa);
        DT_CompileCell(sys, &i);
    } else DT_PushCell(sys->param_stack, &i);

    return true;
}

static bool DT_CompleteFloat(DT_System *sys) {
    List_Push(sys->string, '\0');
    float f = SDL_strtod((char *)List_GetAddress(sys->string, 0), nullptr);

    if (sys->compilation_depth) {
        DT_CompileCell(sys, &sys->push_cfa);
        DT_CompileCell(sys, &f);
    } else DT_PushCell(sys->param_stack, &f);

    return true;
}

static bool DT_CompleteString(DT_System *sys) {
    List_Set(sys->string, List_Length(sys->string) - 1, '\0');
    char *str = (char *)List_GetAddress(sys->string, 1);

    if (sys->compilation_depth) {
        DT_CompileCell(sys, &sys->pushstr_cfa);
        DT_CompileStr(sys, str);
    } else DT_PushString(sys->param_stack, str);

    return true;
}

static bool DT_UnterminatedString(DT_System *sys) {
    (void)sys;
    fprintf(stderr, "Unterminated string literal\n");
    return false;
}

static bool DT_CompleteCompileCtrl(DT_System *sys) {
    char ctrl = *(char *)List_GetAddress(sys->string, 0);

    if (ctrl == '{' || ctrl == '(' || ctrl == ']') {
        sys->compilation_depth += 1;

        if (ctrl == '{')
            DT_CompileCell(sys, &sys->nest_ptr);
    }
    else if (sys->compilation_depth) {
        if (ctrl == '}')
            DT_CompileCell(sys, &sys->unnest_cfa);

        sys->compilation_depth -= 1;
    }
    else {
        fprintf(stderr, "Attempt to exit compilation while not in compilation state\n");
        return false;
    }

    return true;
}

void DT_Exec(DT_System *sys, SDL_IOStream *stream) {
    bool exit = false;
    bool next = true;
    sys->state = DT_PARSE_START;

    while (!exit) {
        if (next) {
            exit = !SDL_ReadU8(stream, &sys->next);
            next = false;
        }

        bool (*callback)(DT_System *) = exit ? sys->transition_eof[sys->state]
                                             : sys->transition_table[sys->state][sys->next].callback;

        if (callback) {
            bool status = callback(sys);
            sys->state = DT_PARSE_START;
            List_Clear(sys->string);

            if (!status) {
                sys->compilation_depth = 0;
                List_Clear(sys->param_stack);
                List_Clear(sys->call_stack);
                return;
            }
        }
        else {
            sys->state = sys->transition_table[sys->state][sys->next].state;
            List_Push(sys->string, sys->next);
            next = true;
        }
    }
}

DT_System *DT_CreateSystem(void) {
    DT_System *sys = SDL_malloc(sizeof(DT_System));
    sys->param_stack = List_Create(DT_Cell);
    sys->data_stack = List_Create(DT_Cell);
    sys->call_stack = List_Create(DT_Cell);
    sys->native_fns = List_Create(void (*)(DT_System *));
    sys->symbols = Strmap_Create(uint32_t, DT_MAX_SYMBOL_LEN);
    sys->string = List_Create(uint8_t);
    sys->compilation_depth = 0;

    DT_SetTransition(sys, DT_PARSE_START, nullptr, DT_PARSE_CALL);
    DT_SetTransition(sys, DT_PARSE_START, WHITESPACE, DT_PARSE_WHITESPACE);
    DT_SetTransition(sys, DT_PARSE_START, "-", DT_PARSE_NEGATIVE);
    DT_SetTransition(sys, DT_PARSE_START, DIGIT, DT_PARSE_INTEGER);
    DT_SetTransition(sys, DT_PARSE_START, ".", DT_PARSE_FLOAT);
    DT_SetTransition(sys, DT_PARSE_START, "\"", DT_PARSE_STRING);
    DT_SetTransition(sys, DT_PARSE_START, COMPILE_CTRL, DT_PARSE_COMPILE_CTRL);
    DT_SetTransition(sys, DT_PARSE_START, "/", DT_PARSE_BEGIN_COMMENT);
    DT_SetCompletion(sys, DT_PARSE_START, "", DT_CompleteDiscard);

    DT_SetCompletion(sys, DT_PARSE_WHITESPACE, nullptr, DT_CompleteDiscard);
    DT_SetTransition(sys, DT_PARSE_WHITESPACE, WHITESPACE, DT_PARSE_WHITESPACE);

    DT_SetTransition(sys, DT_PARSE_CALL, nullptr, DT_PARSE_CALL);
    DT_SetCompletion(sys, DT_PARSE_CALL, WHITESPACE COMPILE_CTRL "\"", DT_CompleteCall);

    DT_SetTransition(sys, DT_PARSE_NEGATIVE, nullptr, DT_PARSE_CALL);
    DT_SetTransition(sys, DT_PARSE_NEGATIVE, DIGIT, DT_PARSE_INTEGER);
    DT_SetTransition(sys, DT_PARSE_NEGATIVE, ".", DT_PARSE_FLOAT);
    DT_SetCompletion(sys, DT_PARSE_NEGATIVE, WHITESPACE COMPILE_CTRL "\"", DT_CompleteCall);

    DT_SetTransition(sys, DT_PARSE_INTEGER, nullptr, DT_PARSE_CALL);
    DT_SetTransition(sys, DT_PARSE_INTEGER, DIGIT, DT_PARSE_INTEGER);
    DT_SetTransition(sys, DT_PARSE_INTEGER, ".", DT_PARSE_FLOAT);
    DT_SetCompletion(sys, DT_PARSE_INTEGER, WHITESPACE COMPILE_CTRL "\"", DT_CompleteInteger);

    DT_SetTransition(sys, DT_PARSE_FLOAT, nullptr, DT_PARSE_CALL);
    DT_SetTransition(sys, DT_PARSE_FLOAT, DIGIT, DT_PARSE_FLOAT);
    DT_SetCompletion(sys, DT_PARSE_FLOAT, WHITESPACE COMPILE_CTRL "\"", DT_CompleteFloat);

    DT_SetTransition(sys, DT_PARSE_STRING, nullptr, DT_PARSE_STRING);
    DT_SetTransition(sys, DT_PARSE_STRING, "\"", DT_PARSE_STRING_COMPLETE);
    DT_SetCompletion(sys, DT_PARSE_STRING, "\n", DT_UnterminatedString);

    DT_SetCompletion(sys, DT_PARSE_STRING_COMPLETE, nullptr, DT_CompleteString);
    DT_SetCompletion(sys, DT_PARSE_COMPILE_CTRL, nullptr, DT_CompleteCompileCtrl);

    DT_SetTransition(sys, DT_PARSE_BEGIN_COMMENT, nullptr, DT_PARSE_CALL);
    DT_SetTransition(sys, DT_PARSE_BEGIN_COMMENT, "/", DT_PARSE_COMMENT);
    DT_SetCompletion(sys, DT_PARSE_BEGIN_COMMENT, WHITESPACE COMPILE_CTRL "\"", DT_CompleteCall);

    DT_SetTransition(sys, DT_PARSE_COMMENT, nullptr, DT_PARSE_COMMENT);
    DT_SetCompletion(sys, DT_PARSE_COMMENT, "\n", DT_CompleteDiscard);

    DT_InitCore(sys);

    const char *stdlib = "duster.dt";
    const char *base = SDL_GetBasePath();

    size_t len = SDL_strlen(base) + SDL_strlen(stdlib) + 1;
    char *path = SDL_realloc(SDL_strdup(base), len);
    SDL_strlcat(path, stdlib, len);
    SDL_IOStream *fs = SDL_IOFromFile(path, "r");
    DT_Exec(sys, fs);

    SDL_free(path);
    SDL_CloseIO(fs);
    return sys;
}

void DT_FreeSystem(DT_System *sys) {
    List_Free(sys->param_stack);
    List_Free(sys->data_stack);
    List_Free(sys->call_stack);
    List_Free(sys->native_fns);
    Strmap_Free(sys->symbols);
    List_Free(sys->string);
    SDL_free(sys);
}

int main(int argc, char **argv) {
    DT_System *sys = DT_CreateSystem();

    if (argc > 1) {
        SDL_IOStream *fs = SDL_IOFromFile(argv[1], "r");
        DT_Exec(sys, fs);
        SDL_CloseIO(fs);
    }
    else {
        List(uint8_t) *buf = List_Create(uint8_t);
        bool exit = false;
        int chr;

        while (true) {
            printf("> ");

            do {
                chr = getc(stdin);

                if (chr == EOF) {
                    printf("\n");
                    exit = true;
                    break;
                }

                List_Push(buf, chr);
            } while (chr != '\n');

            if (exit)
                break;

            SDL_IOStream *stream = SDL_IOFromConstMem(List_GetAddress(buf, 0), List_Length(buf));
            DT_Exec(sys, stream);
            SDL_CloseIO(stream);
            List_Clear(buf);
        }

        List_Free(buf);
    }

    DT_FreeSystem(sys);
    return 0;
}
