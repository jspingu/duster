#ifndef DUSTER_H
#define DUSTER_H

#include <SDL3/SDL.h>
#include "List.h"
#include "Strmap.h"

typedef unsigned char DT_Cell[4];
typedef struct DT_System DT_System;

typedef enum DT_ParseState {
    DT_PARSE_START,
    DT_PARSE_WHITESPACE,
    DT_PARSE_CALL,
    DT_PARSE_NEGATIVE,
    DT_PARSE_INTEGER,
    DT_PARSE_FLOAT,
    DT_PARSE_STRING,
    DT_PARSE_STRING_COMPLETE,
    DT_PARSE_COMPILE_CTRL,
    DT_PARSE_BEGIN_COMMENT,
    DT_PARSE_COMMENT,
    DT_PARSE_STATE_COUNT
} DT_ParseState;

typedef struct DT_ParserTransition {
    DT_ParseState state;
    bool (*callback)(DT_System *);
} DT_ParserTransition;

typedef struct DT_System {
    /* VM */
    uint32_t pc, cfa;
    bool halt, err;
    List(DT_Cell) *param_stack;
    List(DT_Cell) *data_stack;
    List(DT_Cell) *call_stack;
    List(void (*)(DT_System *)) *native_fns;
    Strmap(uint32_t) *symbols;

    /* Core */
    uint32_t nest_ptr;
    uint32_t unnest_cfa;
    uint32_t push_cfa;
    uint32_t pushstr_cfa;
    uint32_t compile_cfa;
    uint32_t compilestr_cfa;

    /* Interpreter */
    DT_ParseState state;
    List(uint8_t) *string;
    uint8_t next;
    uint32_t compilation_depth;
    DT_ParserTransition transition_table[DT_PARSE_STATE_COUNT][256];
    bool (*transition_eof[DT_PARSE_STATE_COUNT])(DT_System *);
} DT_System;

bool DT_GetCell(DT_System *sys, List(DT_Cell) *stack, uint32_t idx, void *out);
bool DT_SetCell(DT_System *sys, List(DT_Cell) *stack, uint32_t idx, void *in);
void DT_PushCell(List(DT_Cell) *stack, void *in);
bool DT_PopCell(DT_System *sys, List(DT_Cell) *stack, void *out);

void DT_PushString(List(DT_Cell) *stack, char *in);
bool DT_PopString(DT_System *sys, List(DT_Cell) *stack, char *out, uint32_t maxlen);
bool DT_Strlen(DT_System *sys, List(DT_Cell) *stack, uint32_t *out);

void DT_AddNativeFunction(DT_System *sys, char *name, void (*fn)(DT_System *));

void DT_Exec(DT_System *sys, SDL_IOStream *fs);

DT_System *DT_CreateSystem(void);
void DT_FreeSystem(DT_System *sys);

#endif /* DUSTER_H */
