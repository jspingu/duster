#ifndef STRMAP_H
#define STRMAP_H

#include <SDL3/SDL.h>
#include "fnv1a.h"

#define STRMAP_INIT_CAPACITY  4

#define Strmap(type)                  typeof(type)

#define Strmap_Create(type,key_size)  ( Strmap_CreateSized(key_size, sizeof(type)) )
#define Strmap_Get(map,key)           ( (typeof(map))(Strmap_GetSized(map, key, sizeof(*map))) )
#define Strmap_Set(map,key,value)     ( *(typeof(map))Strmap_InsertSized(map, key, sizeof(*map)) = value )
#define Strmap_Remove(map,key)        ( Strmap_RemoveSized(map, key, sizeof(*map)) )

/**
 * For each value in `map`, execute the statement passed in the variadic argument.
 * The identifier `val`, corresponding to the current mapped value, is visible in the statement.
 */
#define Strmap_ForEach(map,val,...)   do {                                                       \
    Strmap *Strmap_map = (Strmap *)map;                                                          \
    char (*Strmap_keys)[Strmap_map->key_size] = Strmap_map->keys;                                \
    for (size_t Strmap_iter = 0; Strmap_iter < Strmap_map->capacity; ++Strmap_iter)              \
        if (*Strmap_keys[Strmap_iter]) {                                                         \
            typeof(*map) val = *(typeof(map))(Strmap_map->values + sizeof(*map) * Strmap_iter);  \
            do __VA_ARGS__ while (0);                                                            \
        }                                                                                        \
} while (0)

typedef struct Strmap {
    unsigned char *values;
    char (*keys)[];
    size_t length;
    size_t capacity;
    size_t key_size;
} Strmap;

static inline size_t Strmap_Mask(Strmap *m, size_t size) {
    return size & (m->capacity - 1);
}

static inline bool Strmap_Probe(Strmap *m, char *key, size_t *out) {
    char (*keys)[m->key_size] = m->keys;
    *out = Strmap_Mask(m, fnv1a_str(key));

    while (*keys[*out]) {
        if (!SDL_strcmp(key, keys[*out]))
            return true;

        *out = Strmap_Mask(m, *out + 1);
    }

    return false;
}

static inline void *Strmap_GetSized(void *map, char *key, size_t type_size) {
    Strmap *m = map;
    size_t index;

    if (Strmap_Probe(m, key, &index))
        return m->values + type_size * index;
    else
        return nullptr;
}

static inline void *Strmap_InsertSized(void *map, char *key, size_t type_size) {
    Strmap *m = map;
    size_t index;

    if (!Strmap_Probe(m, key, &index)) {
        if (m->length == m->capacity >> 1) {
            size_t old_capacity = m->capacity;
            m->capacity <<= 1;

            char (*old_keys)[m->key_size] = m->keys;
            char (*new_keys)[m->key_size] = SDL_calloc(m->capacity, m->key_size);

            unsigned char *old_values = m->values;
            unsigned char *new_values = SDL_malloc(type_size * m->capacity);

            for (size_t i = 0; i < old_capacity; ++i) {
                if (*old_keys[i]) {
                    size_t j = Strmap_Mask(m, fnv1a_str(old_keys[i]));

                    while (*new_keys[j])
                        j = Strmap_Mask(m, j + 1);

                    SDL_strlcpy(new_keys[j], old_keys[i], m->key_size);
                    SDL_memcpy(new_values + type_size * j, old_values + type_size * i, type_size);
                }
            }

            m->keys = new_keys;
            m->values = new_values;

            SDL_free(old_keys);
            SDL_free(old_values);

            Strmap_Probe(m, key, &index);
        }

        char (*keys)[m->key_size] = m->keys;
        SDL_strlcpy(keys[index], key, m->key_size);
        m->length += 1;
    }

    return m->values + type_size * index;
}

static inline void Strmap_ReplaceEntry(Strmap *map, size_t target, size_t probe_start, size_t probe_end, size_t type_size) {
    char (*keys)[map->key_size] = map->keys;
    unsigned char *values = map->values;

    for (size_t entry = probe_end; entry != target; entry = Strmap_Mask(map, entry - 1)) {
        size_t hashes_to = Strmap_Mask(map, fnv1a_str(keys[entry]));

        if ((target >= probe_start && hashes_to >= probe_start && hashes_to <= target) || 
            (target < probe_start && (hashes_to >= probe_start || hashes_to <= target)))
        {
            SDL_strlcpy(keys[target], keys[entry], map->key_size);
            SDL_memcpy(values + type_size * target, values + type_size * entry, type_size);
            target = entry;
            entry = probe_end;
        }
    }

    /* Nothing to replace with */
    *keys[target] = '\0';
}

static inline bool Strmap_RemoveSized(void *map, char *key, size_t type_size) {
    Strmap *m = map;
    size_t target;

    if (!Strmap_Probe(m, key, &target))
        return false;

    char (*keys)[m->key_size] = m->keys;
    size_t probe_start = Strmap_Mask(m, fnv1a_str(key));
    size_t probe_end = target;

    while (*keys[Strmap_Mask(m, probe_end + 1)])
        probe_end = Strmap_Mask(m, probe_end + 1);

    while (*keys[Strmap_Mask(m, probe_start - 1)])
        probe_start = Strmap_Mask(m, probe_start - 1);

    Strmap_ReplaceEntry(m, target, probe_start, probe_end, type_size);
    m->length -= 1;

    return true;
}

static inline void *Strmap_CreateSized(size_t key_size, size_t type_size) {
    Strmap *m = SDL_malloc(sizeof(Strmap));
    m->values = SDL_malloc(type_size * STRMAP_INIT_CAPACITY);
    m->keys = SDL_calloc(STRMAP_INIT_CAPACITY, key_size);
    m->length = 0;
    m->capacity = STRMAP_INIT_CAPACITY;
    m->key_size = key_size;

    return m;
}

static inline void Strmap_Free(void *map) {
    Strmap *m = map;
    SDL_free(m->values);
    SDL_free(m->keys);
    SDL_free(m);
}

#endif /* STRMAP_H */
