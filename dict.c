#include "dict.h"

#include <stdlib.h>
#include <string.h>

typedef struct DictNode {
    char *key;
    char *value;
    struct DictNode *next;
} DictNode;

struct Dict {
    DictNode **buckets;
    size_t bucket_count;
    size_t size;
};

static char *copy_cstr(const char *s)
{
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, s, n);
    return out;
}

static size_t bucket_index(const Dict *d, const char *key)
{
    return (size_t)(dict_hash_djb2(key) % (uint64_t)d->bucket_count);
}

static void free_node(DictNode *node)
{
    free(node->key);
    free(node->value);
    free(node);
}

static DictNode *find_node(const Dict *d, const char *key)
{
    DictNode *node = d->buckets[bucket_index(d, key)];
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static void maybe_resize(Dict *d)
{
    /* Evaluated exactly as specified: size * 4 > bucket_count * 3. */
    if (d->size * 4 <= d->bucket_count * 3) {
        return;
    }
    if (d->bucket_count > SIZE_MAX / 2u) {
        return;
    }

    size_t new_count = d->bucket_count * 2u;
    /* SPEC NOTE: reject a bucket array whose byte size would overflow size_t. */
    if (new_count > SIZE_MAX / sizeof(DictNode *)) {
        return;
    }

    DictNode **new_buckets = malloc(new_count * sizeof(*new_buckets));
    if (new_buckets == NULL) {
        return;
    }
    memset(new_buckets, 0, new_count * sizeof(*new_buckets));

    for (size_t i = 0; i < d->bucket_count; i++) {
        DictNode *node = d->buckets[i];
        while (node != NULL) {
            DictNode *next = node->next;
            size_t idx = (size_t)(dict_hash_djb2(node->key) % (uint64_t)new_count);
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            node = next;
        }
    }

    free(d->buckets);
    d->buckets = new_buckets;
    d->bucket_count = new_count;
}

Dict *dict_create(size_t initial_capacity)
{
    size_t cap = (initial_capacity == 0) ? (size_t)DICT_DEFAULT_CAPACITY
                                         : initial_capacity;

    if (cap > SIZE_MAX / sizeof(DictNode *)) {
        return NULL;
    }

    Dict *d = malloc(sizeof(*d));
    if (d == NULL) {
        return NULL;
    }

    d->buckets = malloc(cap * sizeof(*d->buckets));
    if (d->buckets == NULL) {
        free(d);
        return NULL;
    }
    memset(d->buckets, 0, cap * sizeof(*d->buckets));
    d->bucket_count = cap;
    d->size = 0;
    return d;
}

void dict_destroy(Dict *d)
{
    if (d == NULL) {
        return;
    }
    for (size_t i = 0; i < d->bucket_count; i++) {
        DictNode *node = d->buckets[i];
        while (node != NULL) {
            DictNode *next = node->next;
            free_node(node);
            node = next;
        }
    }
    free(d->buckets);
    free(d);
}

DictStatus dict_insert(Dict *d, const char *key, const char *value)
{
    if (d == NULL || key == NULL || value == NULL) {
        return DICT_ERR_NULL_ARG;
    }

    DictNode *existing = find_node(d, key);
    if (existing != NULL) {
        char *new_value = copy_cstr(value);
        if (new_value == NULL) {
            return DICT_ERR_NOMEM;
        }
        free(existing->value);
        existing->value = new_value;
        return DICT_REPLACED;
    }

    char *key_copy = copy_cstr(key);
    if (key_copy == NULL) {
        return DICT_ERR_NOMEM;
    }
    char *value_copy = copy_cstr(value);
    if (value_copy == NULL) {
        free(key_copy);
        return DICT_ERR_NOMEM;
    }
    DictNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        free(key_copy);
        free(value_copy);
        return DICT_ERR_NOMEM;
    }
    node->key = key_copy;
    node->value = value_copy;
    size_t idx = bucket_index(d, key);
    node->next = d->buckets[idx];
    d->buckets[idx] = node;
    d->size++;
    maybe_resize(d);
    return DICT_OK;
}

const char *dict_get(const Dict *d, const char *key)
{
    if (d == NULL || key == NULL) {
        return NULL;
    }
    DictNode *node = find_node(d, key);
    return (node == NULL) ? NULL : node->value;
}

DictStatus dict_remove(Dict *d, const char *key)
{
    if (d == NULL || key == NULL) {
        return DICT_ERR_NULL_ARG;
    }

    size_t idx = bucket_index(d, key);
    DictNode **link = &d->buckets[idx];
    while (*link != NULL) {
        if (strcmp((*link)->key, key) == 0) {
            DictNode *gone = *link;
            *link = gone->next;
            free_node(gone);
            d->size--;
            return DICT_OK;
        }
        link = &(*link)->next;
    }
    return DICT_ERR_NOT_FOUND;
}

bool dict_contains(const Dict *d, const char *key)
{
    if (d == NULL || key == NULL) {
        return false;
    }
    return find_node(d, key) != NULL;
}

size_t dict_size(const Dict *d)
{
    return (d == NULL) ? 0 : d->size;
}

size_t dict_bucket_count(const Dict *d)
{
    return (d == NULL) ? 0 : d->bucket_count;
}

DictStatus dict_foreach(const Dict *d, DictVisitFn fn, void *user_data)
{
    if (d == NULL || fn == NULL) {
        return DICT_ERR_NULL_ARG;
    }
    for (size_t i = 0; i < d->bucket_count; i++) {
        for (DictNode *node = d->buckets[i]; node != NULL; node = node->next) {
            if (fn(node->key, node->value, user_data) != 0) {
                return DICT_STOPPED;
            }
        }
    }
    return DICT_OK;
}

uint64_t dict_hash_djb2(const char *s)
{
    uint64_t hash = 5381u;
    if (s == NULL) {
        return hash;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        hash = hash * 33u + *p;
    }
    return hash;
}
