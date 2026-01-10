#ifndef CP_H_
#define CP_H_

#ifndef CP_DA_INIT_CAP
#define CP_DA_INIT_CAP 256
#endif

#ifndef CP_HT_INIT_CAP
#define CP_HT_INIT_CAP 128
#endif

#ifndef CP_ARENA_INIT_CAP
#define CP_ARENA_INIT_CAP (8*1024)
#endif

#ifndef CP_REALLOC
#include <stdlib.h>
#define CP_REALLOC realloc
#endif

#ifndef CP_MALLOC
#include <stdlib.h>
#define CP_MALLOC malloc
#endif

#ifndef CP_FREE
#include <stdlib.h>
#define CP_FREE free
#endif

#ifndef CP_MEMMOVE
#include <string.h>
#define CP_MEMMOVE memmove
#endif

#ifndef CP_STRLEN
#include <string.h>
#define CP_STRLEN strlen
#endif

#ifndef _CP_RUNTIME_CHECKS
#define CP_ASSERT(a) ((void)0)
#else
#ifndef CP_ASSERT
#include <assert.h>
#define CP_ASSERT assert
#endif
#endif

#ifdef __cplusplus
#define CP_DECLTYPE_CAST(T) (decltype(T))
#else
#define CP_DECLTYPE_CAST(T)
#endif // __cplusplus

#ifndef CP_INT_DEFINED
    typedef unsigned int uint;
    #ifdef CP_USE_INT
        typedef unsigned char u8;
        typedef signed char i8;
        typedef unsigned short u16;
        typedef signed short i16;
        typedef unsigned long int u32;
        typedef signed long int i32;
        typedef unsigned long long u64;
        typedef signed long long i64;
    #else
        #include <stdint.h>
        typedef uint8_t u8;
        typedef int8_t i8;
        typedef uint16_t u16;
        typedef int16_t i16;
        typedef uint32_t u32;
        typedef int32_t i32;
        typedef uint64_t u64;
        typedef int64_t i64;
    #endif
    #define CP_INT_DEFINED
#endif

#ifndef ARR_LEN
#define ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Arena allocator (defined early since DA/HT depend on it) */

#define CP_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

typedef struct Arena Arena;
typedef struct ArenaBlock ArenaBlock;

struct ArenaBlock {
    ArenaBlock *next;
    size_t capacity;
    size_t count;
    u8 data[];
};

struct Arena {
    ArenaBlock *first;
    ArenaBlock *last;
    void *last_ptr;
    size_t last_sz;
};

static void *arena_alloc(Arena *a, size_t size) {
    size = CP_ALIGN_UP(size, sizeof(void*));
    if (!a->first) {
        size_t cap = MAX(CP_ARENA_INIT_CAP, size);
        ArenaBlock *b = (ArenaBlock *)CP_MALLOC(sizeof(ArenaBlock) + cap);
        CP_ASSERT(b);
        b->next = NULL;
        b->capacity = cap;
        b->count = 0;
        a->first = a->last = b;
    }
    ArenaBlock *b = a->last;
    if (b->count + size > b->capacity) {
        size_t new_cap = MAX(b->capacity * 2, size);
        ArenaBlock *newb = (ArenaBlock *)CP_MALLOC(sizeof(ArenaBlock) + new_cap);
        CP_ASSERT(newb);
        newb->next = NULL;
        newb->capacity = new_cap;
        newb->count = 0;
        a->last->next = newb;
        a->last = newb;
        b = newb;
    }
    void *p = b->data + b->count;
    b->count += size;
    a->last_ptr = p;
    a->last_sz = size;
    return p;
}

static void *arena_realloc(Arena *a, void *oldptr, size_t oldsz, size_t newsz) {
    if (newsz <= oldsz) return oldptr;
    if (oldptr == a->last_ptr) {
        ArenaBlock *b = a->last;
        size_t extra = newsz - oldsz;
        if (b->count + extra <= b->capacity) {
            b->count += extra;
            a->last_sz = newsz;
            return oldptr;
        } else {
            size_t new_cap = MAX(b->capacity * 2, newsz);
            ArenaBlock *newb = (ArenaBlock *)CP_MALLOC(sizeof(ArenaBlock) + new_cap);
            CP_ASSERT(newb);
            newb->next = NULL;
            newb->capacity = new_cap;
            newb->count = newsz;
            CP_MEMMOVE(newb->data, oldptr, oldsz);
            a->last->next = newb;
            a->last = newb;
            b->count -= oldsz;
            a->last_ptr = newb->data;
            a->last_sz = newsz;
            return a->last_ptr;
        }
    }
    void *newptr = arena_alloc(a, newsz);
    CP_MEMMOVE(newptr, oldptr, oldsz);
    return newptr;
}

static void *arena_memdup(Arena *arena, void *p, size_t size) {
    void *duped_mem = arena_alloc(arena, size);
    CP_MEMMOVE(duped_mem, p, size);
    return duped_mem;
}

static char *arena_strdup(Arena *arena, char *str) {
    return (char *) arena_memdup(arena, str, CP_STRLEN(str) + 1);
}

#define arena_free(ar) \
    do { \
        ArenaBlock *b = (ar)->first; \
        while (b) { \
            ArenaBlock *next = b->next; \
            CP_FREE(b); \
            b = next; \
        } \
        (ar)->first = (ar)->last = NULL; \
        (ar)->last_ptr = NULL; \
        (ar)->last_sz = 0; \
    } while (0)

#define arena_reset(ar) \
    do { \
        for (ArenaBlock *b = (ar)->first; b; b = b->next) { \
            b->count = 0; \
        } \
        (ar)->last = (ar)->first; \
        (ar)->last_ptr = NULL; \
        (ar)->last_sz = 0; \
    } while (0)

/* Dynamic array */

#define DA(type) struct { type *items; size_t count, capacity; Arena *arena; }

#define _da_realloc(da, ptr, old_sz, new_sz) \
    ((da)->arena ? \
        ((ptr) ? arena_realloc((da)->arena, ptr, old_sz, new_sz) : arena_alloc((da)->arena, new_sz)) : \
        CP_REALLOC(ptr, new_sz))

#define _da_free(da, ptr) \
    do { if (!(da)->arena && ptr) CP_FREE(ptr); } while(0)

#define da_foreach(Type, it, da) \
    for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

#define da_reserve(da, expected_capacity) \
    do { \
        if ((expected_capacity) > (da)->capacity) { \
            size_t new_capacity = (da)->capacity ? (da)->capacity : CP_DA_INIT_CAP; \
            while ((expected_capacity) > new_capacity) new_capacity *= 2; \
            size_t old_sz = (da)->capacity * sizeof(*(da)->items); \
            size_t new_sz = new_capacity * sizeof(*(da)->items); \
            void *new_items = _da_realloc(da, (da)->items, old_sz, new_sz); \
            CP_ASSERT(new_items != NULL || (expected_capacity) == 0); \
            if (new_items || (expected_capacity) == 0) { \
                (da)->items = CP_DECLTYPE_CAST((da)->items)new_items; \
                (da)->capacity = new_capacity; \
            } \
        } \
    } while (0)

#define da_shrink(da) \
    do { \
        if ((da)->capacity == 0 || (da)->arena) break; \
        if ((da)->count <= (da)->capacity / 4) { \
            (da)->capacity = (da)->count * 2; \
            (da)->items = CP_REALLOC((da)->items, sizeof(*(da)->items) * (da)->capacity); \
        } \
    } while (0)

#define da_append(da, item) \
    do { \
        da_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item); \
    } while (0)

#define da_insert(da, index, item) \
    do { \
        size_t _idx = (size_t)(index); \
        size_t _old = (da)->count; \
        CP_ASSERT(_idx <= _old); \
        da_reserve((da), _old + 1); \
        if (_idx < _old) { \
            CP_MEMMOVE((da)->items + _idx + 1, \
                       (da)->items + _idx, \
                       sizeof *(da)->items * (_old - _idx)); \
        } \
        (da)->count = _old + 1; \
        da_get(da, _idx) = (item); \
    } while (0)

#define da_get(da, index) \
    (da)->items[CP_ASSERT((index) >= 0 && (index) < (da)->count), (index)]

#define da_last(da) \
    (da)->items[CP_ASSERT((da)->count > 0), ((da)->count - 1)]

#define da_resize(da, cnt) \
    do { \
        (da)->count = (cnt); \
        da_reserve((da), (da)->count); \
        da_shrink(da); \
    } while (0)

#define da_reset(da) \
    do { (da)->count = 0; } while (0)

#define da_remove_unordered(da, index) \
    do { \
        da_get(da, (index)) = da_last(da); \
        (da)->count--; \
    } while (0)

#define da_remove_ordered(da, index) \
    do { \
        da_get(da, index) = da_last(da); \
        CP_MEMMOVE((da)->items+(index), (da)->items+(index)+1, \
                sizeof(*(da)->items)*((da)->count-index)); \
        (da)->count--; \
    } while (0)

#define da_free(da) \
    do { \
        _da_free(da, (da)->items); \
        (da)->items = NULL; \
        (da)->count = 0; \
        (da)->capacity = 0; \
    } while (0)

#define da_remove_last(da) \
    do { \
        CP_ASSERT((da)->count > 0); \
        (da)->count--; \
    } while (0)

#define da_append_many(da, new_items, new_items_count) \
    do { \
        CP_ASSERT(new_items); \
        da_reserve((da), (da)->count + (new_items_count)); \
        CP_MEMMOVE((da)->items + (da)->count, (new_items), (new_items_count)*sizeof(*(da)->items)); \
        (da)->count += (new_items_count); \
    } while (0)

#define da_set_arena(da, ar) do { (da)->arena = (ar); } while(0)

/* Hashtable templates */

#define HT_DECL(ht_type, key_type, value_type) \
    typedef struct ht_type##_node { \
        key_type key; \
        value_type val; \
        struct ht_type##_node *next; \
    } ht_type##_node; \
    typedef struct { \
        ht_type##_node **arr; \
        size_t count; \
        size_t capacity; \
        Arena *arena; \
    } ht_type; \
    u64 ht_type##_hashf(key_type key); \
    int ht_type##_compare(key_type a, key_type b); \
    void ht_type##_add(ht_type *ht, key_type key, value_type val); \
    value_type *ht_type##_get(ht_type *ht, key_type key); \
    void ht_type##_remove(ht_type *ht, key_type key); \
    void ht_type##_free(ht_type *ht); \
    void ht_type##_set_arena(ht_type *ht, Arena *ar);
#define HT_IMPL(ht_type, key_type, value_type) \
extern u64 ht_type##_hashf(key_type key); \
extern int ht_type##_compare(key_type a, key_type b); \
\
void ht_type##_set_arena(ht_type *ht, Arena *ar) { \
    ht->arena = ar; \
} \
\
void ht_type##_add(ht_type *ht, key_type key, value_type val) { \
    if (ht->capacity == 0) { \
        ht->capacity = CP_HT_INIT_CAP; \
        if (ht->arena) { \
            ht->arr = (ht_type##_node**) arena_alloc(ht->arena, ht->capacity * sizeof(ht_type##_node*)); \
        } else { \
            ht->arr = (ht_type##_node**) CP_MALLOC(ht->capacity * sizeof(ht_type##_node*)); \
        } \
        memset(ht->arr, 0, ht->capacity * sizeof(ht_type##_node*)); \
    } \
    size_t idx = (size_t)(ht_type##_hashf(key) % ht->capacity); \
    ht_type##_node *cur = ht->arr[idx]; \
    while (cur) { \
        if (ht_type##_compare(cur->key, key) == 0) { \
            cur->val = val; \
            return; \
        } \
        cur = cur->next; \
    } \
    ht_type##_node *n = ht->arena \
        ? (ht_type##_node*) arena_alloc(ht->arena, sizeof(ht_type##_node)) \
        : (ht_type##_node*) CP_MALLOC(sizeof(ht_type##_node)); \
    memset(n, 0, sizeof(ht_type##_node)); \
    n->key = key; \
    n->val = val; \
    n->next = ht->arr[idx]; \
    ht->arr[idx] = n; \
    ht->count++; \
    if (ht->count > ht->capacity * 2) { \
        size_t old_cap = ht->capacity; \
        size_t new_cap = old_cap * 3; \
        ht_type##_node **new_arr; \
        if (ht->arena) { \
            new_arr = (ht_type##_node**) arena_alloc(ht->arena, new_cap * sizeof(ht_type##_node*)); \
        } else { \
            new_arr = (ht_type##_node**) CP_MALLOC(new_cap * sizeof(ht_type##_node*)); \
        } \
        memset(new_arr, 0, new_cap * sizeof(ht_type##_node*)); \
        for (size_t i = 0; i < old_cap; ++i) { \
            ht_type##_node *it = ht->arr[i]; \
            while (it) { \
                ht_type##_node *next = it->next; \
                size_t j = (size_t)(ht_type##_hashf(it->key) % new_cap); \
                it->next = new_arr[j]; \
                new_arr[j] = it; \
                it = next; \
            } \
        } \
        if (!ht->arena) CP_FREE(ht->arr); \
        ht->arr = new_arr; \
        ht->capacity = new_cap; \
    } \
} \
\
value_type *ht_type##_get(ht_type *ht, key_type key) { \
    if (ht->capacity == 0) return NULL; \
    size_t idx = (size_t)(ht_type##_hashf(key) % ht->capacity); \
    ht_type##_node *cur = ht->arr[idx]; \
    while (cur) { \
        if (ht_type##_compare(cur->key, key) == 0) return &cur->val; \
        cur = cur->next; \
    } \
    return NULL; \
} \
\
void ht_type##_remove(ht_type *ht, key_type key) { \
    if (ht->capacity == 0) return; \
    size_t idx = (size_t)(ht_type##_hashf(key) % ht->capacity); \
    ht_type##_node *cur = ht->arr[idx]; \
    ht_type##_node *prev = NULL; \
    while (cur) { \
        if (ht_type##_compare(cur->key, key) == 0) { \
            if (prev) prev->next = cur->next; else ht->arr[idx] = cur->next; \
            if (!ht->arena) CP_FREE(cur); \
            ht->count--; \
            return; \
        } \
        prev = cur; \
        cur = cur->next; \
    } \
} \
\
void ht_type##_free(ht_type *ht) { \
    if (ht->capacity == 0 || ht->arena) return; \
    for (size_t i = 0; i < ht->capacity; ++i) { \
        ht_type##_node *cur = ht->arr[i]; \
        while (cur) { \
            ht_type##_node *next = cur->next; \
            CP_FREE(cur); \
            cur = next; \
        } \
    } \
    CP_FREE(ht->arr); \
    ht->arr = NULL; \
    ht->capacity = 0; \
    ht->count = 0; \
}

#define HT(ht_type, key_type, value_type) \
    HT_DECL(ht_type, key_type, value_type) \
    HT_IMPL(ht_type, key_type, value_type)

#define ht_foreach_node(ht_type, ht, nodevar) \
    for (size_t _ht_idx = 0; (ht)->capacity && _ht_idx < (ht)->capacity; ++_ht_idx) \
        for (ht_type##_node *nodevar = (ht)->arr[_ht_idx]; nodevar; nodevar = nodevar->next)

#define ht_set_arena(ht, ar) do { (ht)->arena = (ar); } while (0)

static inline u64 hash_str(char *str) {
    u64 h = 14695981039346656037ULL;
    u8 *p = (u8*)(str);
    while (*p) {
        h ^= (u64)(*p++);
        h *= 1099511628211ULL;
    }
    return h;
}

#define hash_num(num) \
    ((u64)((u64)6364136223846793005ULL * (u64)(num) + 1442695040888963407ULL))

#define hash_combine(h1, h2) \
    h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2))

#define HT_DECL_STR(ht_type, value_type) \
    HT_DECL(ht_type, char*, value_type)

#define HT_IMPL_STR(ht_type, value_type) \
HT_IMPL(ht_type, char*, value_type) \
\
u64 ht_type##_hashf(char* s) { \
    return hash_str(s); \
} \
\
int ht_type##_compare(char* a, char* b) { \
    return strcmp(a, b); \
}

#define HT_STR(ht_type, value_type) \
    HT_DECL_STR(ht_type, value_type) \
    HT_IMPL_STR(ht_type, value_type)

#define HT_IMPL_NUM(ht_type, key_type, value_type) \
HT_IMPL(ht_type, key_type, value_type) \
\
u64 ht_type##_hashf(key_type num) { \
    return hash_num(num); \
} \
\
int ht_type##_compare(key_type a, key_type b) { \
    return !(a == b); \
}

/* String builder */

#include <stdarg.h>
#include <stdio.h>

typedef DA(char) StringBuilder;

static inline int sb_appendf(StringBuilder *sb, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    da_reserve(sb, sb->count + n + 1);
    char *dest = sb->items + sb->count;
    va_start(args, fmt);
    vsnprintf(dest, n+1, fmt, args);
    va_end(args);

    sb->count += n;
    return n;
}

#define sb_append(sb, c) da_append(sb, c)
#define sb_reset(sb) da_reset(sb)
#define sb_free(sb) da_free(sb)
#define sb_set_arena(sb, ar) da_set_arena(sb, ar)

#endif // CP_H_
