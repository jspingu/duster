#ifndef LIST_H
#define LIST_H

#include <SDL3/SDL.h>

#define LIST_INIT_CAPACITY  0

#define List(type)                              typeof(type)

#define List_Create(type)                       ( (typeof(type) *)List_CreateSized(sizeof(type)) )
#define List_GetAddress(list,index)             ( (typeof(list))((List *)(list))->elements + (index) )
#define List_Get(list,index)                    ( *List_GetAddress(list, index) )
#define List_Set(list,index,value)              ( List_Get(list, index) = value )

#define List_Length(list)                       ( ((List *)(list))->length )
#define List_Capacity(list)                     ( ((List *)(list))->capacity )

#define List_InsertSpace(list,index,count)      ( List_InsertSpaceSized(list, index, count, sizeof(*list)) )
#define List_InsertRange(list,index,src,count)  ( SDL_memcpy(List_InsertSpace(list, index, count), src, sizeof(*list) * count) )
#define List_Insert(list,index,value)           ( *(typeof(list))(List_InsertSpace(list, index, 1)) = value )

#define List_PushSpace(list,count)              ( List_InsertSpace(list, List_Length(list), count) )
#define List_PushRange(list,src,count)          ( SDL_memcpy(List_PushSpace(list, count), src, sizeof(*list) * count) )
#define List_Push(list,value)                   ( *(typeof(list))(List_PushSpace(list, 1)) = value )

#define List_RemoveSpace(list,index,count)      ( List_RemoveSpaceSized(list, index, count, sizeof(*list)) )
#define List_Remove(list,index)                 ( List_RemoveSpace(list, index, 1) )

#define List_DropSpace(list,count)              ( ((List *)(list))->length -= count )
#define List_Drop(list)                         ( ((List *)(list))->length -= 1 )
#define List_Pop(list)                          ( List_Get(list, ((List *)(list))->length -= 1) )
#define List_Clear(list)                        ( ((List *)(list))->length = 0 )

/**
 * Iterate through elements in `list` starting from `left` (inclusive) to `right` (exclusive).
 * In each iteration, execute the statement passed in the variadic argument.
 * The identifier `elem`, corresponding to the current list element, is visible in the statement.
 */
#define List_For(list,left,right,elem,...)         do for (size_t List_iter = left; List_iter < right; ++List_iter) { typeof(*list) elem = List_Get(list, List_iter); do __VA_ARGS__ while(0); } while (0)

/**
 * Iterate through elements in `list` starting from `right` (exclusive) to `left` (inclusive).
 * In each iteration, execute the statement passed in the variadic argument.
 * The identifier `elem`, corresponding to the current list element, is visible in the statement.
 */
#define List_ForReverse(list,right,left,elem,...)  do for (size_t List_iter = right; List_iter > left; --List_iter) { typeof(*list) elem = List_Get(list, List_iter - 1); do __VA_ARGS__ while(0); } while (0)

/**
 * For each element in `list`, execute the statement passed in the variadic argument.
 * The identifier `elem`, corresponding to the current list element, is visible in the statement.
 */
#define List_ForEach(list,elem,...)                List_For(list, 0, List_Length(list), elem, __VA_ARGS__)

/**
 * Iterate through elements in `list` and remove the first element where the expression `expr` evaluates to true.
 * The identifier `elem`, corresponding to the current list element, is visible in the expression.
 */
#define List_RemoveWhere(list,elem,expr)           List_ForEach(list, elem, { if (expr) List_Remove(list, List_iter); })

typedef struct List {
    unsigned char *elements;
    size_t length;
    size_t capacity;
} List;

static inline size_t List_bounding_size(size_t n) {
    size_t size = 1;
    while (size < n)
        size = (size << 1) + 1;

    return size;
}

static inline void *List_InsertSpaceSized(void *list, size_t index, size_t count, size_t type_size) {
    List *l = list;
    size_t old_length = l->length;
    l->length += count;

    if (l->length > l->capacity) {
        l->capacity = List_bounding_size(l->length);
        l->elements = SDL_realloc(l->elements, type_size * l->capacity);
    }

    SDL_memmove(
        l->elements + type_size * (index + count), 
        l->elements + type_size * index, 
        type_size * (old_length - index)
    );

    return l->elements + type_size * index;
}

static inline void List_RemoveSpaceSized(void *list, size_t index, size_t count, size_t type_size) {
    List *l = list;

    SDL_memmove(
        l->elements + type_size * index,
        l->elements + type_size * (index + count),
        type_size * ((l->length -= count) - index)
    );
}

static inline void *List_CreateSized(size_t type_size) {
    List *l = SDL_malloc(sizeof(List));
    l->elements = SDL_malloc(type_size * LIST_INIT_CAPACITY);
    l->length = 0;
    l->capacity = LIST_INIT_CAPACITY;

    return l;
}

static inline void List_Free(void *list) {
    List *l = list;
    SDL_free(l->elements);
    SDL_free(l);
}

#endif /* LIST_H */
