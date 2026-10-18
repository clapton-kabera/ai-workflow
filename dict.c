#include "dict.h"
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    char *key;
    char *value;
    struct Node *next;
} Node;

struct Dict {
    Node **buckets;
    size_t bucket_count;
    size_t size;
};

uint64_t dict_hash_djb2(const char *s) {
    uint64_t hash = 5381;
    if (!s) return hash;
    const unsigned char *p = (const unsigned char *)s;
    while (*p)
        hash = hash * 33u + *p++;
    return hash;
}

Dict *dict_create(size_t initial_capacity) {
    if (initial_capacity == 0)
        initial_capacity = DICT_DEFAULT_CAPACITY;
    Dict *d = malloc(sizeof(Dict));
    if (!d) return NULL;
    d->buckets = malloc(initial_capacity * sizeof(Node *));
    if (!d->buckets) {
        free(d);
        return NULL;
    }
    memset(d->buckets, 0, initial_capacity * sizeof(Node *));
    d->bucket_count = initial_capacity;
    d->size = 0;
    return d;
}

void dict_destroy(Dict *d) {
    if (!d) return;
    for (size_t i = 0; i < d->bucket_count; i++) {
        Node *n = d->buckets[i];
        while (n) {
            Node *next = n->next;
            free(n->key);
            free(n->value);
            free(n);
            n = next;
        }
    }
    free(d->buckets);
    free(d);
}

static Node *find_node(const Dict *d, const char *key) {
    size_t idx = dict_hash_djb2(key) % d->bucket_count;
    for (Node *n = d->buckets[idx]; n; n = n->next)
        if (strcmp(n->key, key) == 0) return n;
    return NULL;
}

static char *dup_string(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

static void try_resize(Dict *d) {
    if (d->size * 4 <= d->bucket_count * 3) return;
    /* SPEC NOTE: skip silently if doubling would overflow size_t */
    if (d->bucket_count > (size_t)-1 / 2) return;
    size_t new_count = d->bucket_count * 2;
    if (new_count > (size_t)-1 / sizeof(Node *)) return;

    Node **nb = malloc(new_count * sizeof(Node *));
    if (!nb) return; /* SPEC NOTE: silent skip on allocation failure */
    memset(nb, 0, new_count * sizeof(Node *));

    for (size_t i = 0; i < d->bucket_count; i++) {
        for (Node *n = d->buckets[i]; n; ) {
            Node *next = n->next;
            size_t idx = dict_hash_djb2(n->key) % new_count;
            n->next = nb[idx];
            nb[idx] = n;
            n = next;
        }
    }
    free(d->buckets);
    d->buckets = nb;
    d->bucket_count = new_count;
}

DictStatus dict_insert(Dict *d, const char *key, const char *value) {
    if (!d || !key || !value) return DICT_ERR_NULL_ARG;

    Node *existing = find_node(d, key);
    if (existing) {
        char *new_val = dup_string(value);
        if (!new_val) return DICT_ERR_NOMEM;
        free(existing->value);
        existing->value = new_val;
        return DICT_REPLACED;
    }

    char *k = dup_string(key);
    if (!k) return DICT_ERR_NOMEM;
    char *v = dup_string(value);
    if (!v) { free(k); return DICT_ERR_NOMEM; }
    Node *node = malloc(sizeof(Node));
    if (!node) { free(k); free(v); return DICT_ERR_NOMEM; }

    node->key = k;
    node->value = v;
    size_t idx = dict_hash_djb2(key) % d->bucket_count;
    node->next = d->buckets[idx];
    d->buckets[idx] = node;
    d->size++;

    try_resize(d);
    return DICT_OK;
}

const char *dict_get(const Dict *d, const char *key) {
    if (!d || !key) return NULL;
    Node *n = find_node(d, key);
    return n ? n->value : NULL;
}

DictStatus dict_remove(Dict *d, const char *key) {
    if (!d || !key) return DICT_ERR_NULL_ARG;
    size_t idx = dict_hash_djb2(key) % d->bucket_count;
    Node *prev = NULL;
    for (Node *n = d->buckets[idx]; n; prev = n, n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (prev) prev->next = n->next;
            else d->buckets[idx] = n->next;
            free(n->key);
            free(n->value);
            free(n);
            d->size--;
            return DICT_OK;
        }
    }
    return DICT_ERR_NOT_FOUND;
}

bool dict_contains(const Dict *d, const char *key) {
    if (!d || !key) return false;
    return find_node(d, key) != NULL;
}

size_t dict_size(const Dict *d) {
    return d ? d->size : 0;
}

size_t dict_bucket_count(const Dict *d) {
    return d ? d->bucket_count : 0;
}

DictStatus dict_foreach(const Dict *d, DictVisitFn fn, void *user_data) {
    if (!d || !fn) return DICT_ERR_NULL_ARG;
    for (size_t i = 0; i < d->bucket_count; i++) {
        for (Node *n = d->buckets[i]; n; n = n->next) {
            if (fn(n->key, n->value, user_data) != 0)
                return DICT_STOPPED;
        }
    }
    return DICT_OK;
}
