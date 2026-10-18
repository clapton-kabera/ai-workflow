#include "dict.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Collision keys from test_cases.md: same djb2 hash, always one chain. */
static const char *K1 = "AaAa";
static const char *K2 = "AaB@";
static const char *K3 = "B@Aa";

static int g_failed;
static const char *g_fail_reason;
static int g_failures;
static int g_passed;
static int g_total;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            g_failed = 1;                                                       \
            if (g_fail_reason == NULL) {                                        \
                g_fail_reason = #cond;                                          \
            }                                                                   \
        }                                                                       \
    } while (0)

static void finish_test(const char *id)
{
    g_total++;
    if (g_failed) {
        g_failures++;
        printf("FAIL %s: %s\n", id, g_fail_reason ? g_fail_reason : "failed");
    } else {
        g_passed++;
        printf("PASS %s\n", id);
    }
}

static int fail_next_alloc;

void *__real_malloc(size_t n);
void *__real_calloc(size_t nmemb, size_t size);

void *__wrap_malloc(size_t n)
{
    if (fail_next_alloc) {
        fail_next_alloc = 0;
        return NULL;
    }
    return __real_malloc(n);
}

void *__wrap_calloc(size_t nmemb, size_t size)
{
    if (fail_next_alloc) {
        fail_next_alloc = 0;
        return NULL;
    }
    return __real_calloc(nmemb, size);
}

static void arm_alloc_fail(void)
{
    fail_next_alloc = 1;
}

static Dict *make_collision_dict(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, K1, "v1") == DICT_OK);
    CHECK(dict_insert(d, K2, "v2") == DICT_OK);
    CHECK(dict_insert(d, K3, "v3") == DICT_OK);
    return d;
}

static void test_TC_01(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_size(d) == 0);
    CHECK(dict_bucket_count(d) == 16);
    dict_destroy(d);
}

static void test_TC_02(void)
{
    Dict *d = dict_create(64);
    CHECK(d != NULL);
    CHECK(dict_size(d) == 0);
    CHECK(dict_bucket_count(d) == 64);
    dict_destroy(d);
}

static void test_TC_03(void)
{
    dict_destroy(NULL);
}

static void test_TC_04(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    dict_destroy(d);
}

static void test_TC_05(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "name", "alice") == DICT_OK);
    const char *v = dict_get(d, "name");
    CHECK(v != NULL && strcmp(v, "alice") == 0);
    CHECK(dict_contains(d, "name") == true);
    CHECK(dict_size(d) == 1);
    dict_destroy(d);
}

static void test_TC_06(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_insert(d, "b", "2") == DICT_OK);
    CHECK(dict_insert(d, "c", "3") == DICT_OK);
    const char *a = dict_get(d, "a");
    const char *b = dict_get(d, "b");
    const char *c = dict_get(d, "c");
    CHECK(a != NULL && strcmp(a, "1") == 0);
    CHECK(b != NULL && strcmp(b, "2") == 0);
    CHECK(c != NULL && strcmp(c, "3") == 0);
    CHECK(dict_size(d) == 3);
    dict_destroy(d);
}

static void test_TC_07(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "", "") == DICT_OK);
    CHECK(dict_insert(d, "k", "") == DICT_OK);
    const char *empty_key = dict_get(d, "");
    const char *k = dict_get(d, "k");
    CHECK(empty_key != NULL && strcmp(empty_key, "") == 0);
    CHECK(k != NULL && strcmp(k, "") == 0);
    CHECK(dict_size(d) == 2);
    dict_destroy(d);
}

static void test_TC_08(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[] = "key";
    char v[] = "val";
    CHECK(dict_insert(d, k, v) == DICT_OK);
    strcpy(k, "zzz");
    strcpy(v, "xxx");
    const char *got = dict_get(d, "key");
    CHECK(got != NULL && strcmp(got, "val") == 0);
    CHECK(dict_get(d, "zzz") == NULL);
    CHECK(got != v);
    dict_destroy(d);
}

static void test_TC_09(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "Key", "upper") == DICT_OK);
    CHECK(dict_insert(d, "key", "lower") == DICT_OK);
    CHECK(dict_size(d) == 2);
    const char *u = dict_get(d, "Key");
    const char *l = dict_get(d, "key");
    CHECK(u != NULL && strcmp(u, "upper") == 0);
    CHECK(l != NULL && strcmp(l, "lower") == 0);
    dict_destroy(d);
}

static void test_TC_10(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "k", "old") == DICT_OK);
    CHECK(dict_insert(d, "k", "new") == DICT_REPLACED);
    const char *v = dict_get(d, "k");
    CHECK(v != NULL && strcmp(v, "new") == 0);
    CHECK(dict_size(d) == 1);
    dict_destroy(d);
}

static void test_TC_11(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char buf[16];
    for (int i = 0; i < 1000; i++) {
        snprintf(buf, sizeof buf, "v%d", i);
        DictStatus st = dict_insert(d, "k", buf);
        if (i == 0) {
            CHECK(st == DICT_OK);
        } else {
            CHECK(st == DICT_REPLACED);
        }
    }
    const char *v = dict_get(d, "k");
    CHECK(v != NULL && strcmp(v, "v999") == 0);
    CHECK(dict_size(d) == 1);
    dict_destroy(d);
}

static void test_TC_12(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "k", "same") == DICT_OK);
    CHECK(dict_insert(d, "k", "same") == DICT_REPLACED);
    const char *v = dict_get(d, "k");
    CHECK(v != NULL && strcmp(v, "same") == 0);
    CHECK(dict_size(d) == 1);
    dict_destroy(d);
}

static void test_TC_13(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_get(d, "b") == NULL);
    CHECK(dict_size(d) == 1);
    dict_destroy(d);
}

static void test_TC_14(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_contains(d, "a") == true);
    CHECK(dict_contains(d, "b") == false);
    dict_destroy(d);
}

static void test_TC_15(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_remove(d, "zzz") == DICT_ERR_NOT_FOUND);
    CHECK(dict_size(d) == 1);
    const char *v = dict_get(d, "a");
    CHECK(v != NULL && strcmp(v, "1") == 0);
    dict_destroy(d);
}

static void test_TC_16(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_get(d, "x") == NULL);
    CHECK(dict_contains(d, "x") == false);
    CHECK(dict_remove(d, "x") == DICT_ERR_NOT_FOUND);
    CHECK(dict_size(d) == 0);
    dict_destroy(d);
}

static void test_TC_17(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_remove(d, "a") == DICT_OK);
    CHECK(dict_remove(d, "a") == DICT_ERR_NOT_FOUND);
    CHECK(dict_size(d) == 0);
    CHECK(dict_get(d, "a") == NULL);
    dict_destroy(d);
}

static void test_TC_18(void)
{
    CHECK(dict_insert(NULL, "k", "v") == DICT_ERR_NULL_ARG);
}

static void test_TC_19(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_insert(d, NULL, "v") == DICT_ERR_NULL_ARG);
    CHECK(dict_insert(d, "k", NULL) == DICT_ERR_NULL_ARG);
    CHECK(dict_insert(d, "a", NULL) == DICT_ERR_NULL_ARG);
    CHECK(dict_size(d) == 1);
    CHECK(dict_contains(d, "k") == false);
    const char *v = dict_get(d, "a");
    CHECK(v != NULL && strcmp(v, "1") == 0);
    dict_destroy(d);
}

static void test_TC_20(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_get(NULL, "a") == NULL);
    CHECK(dict_get(d, NULL) == NULL);
    CHECK(dict_contains(NULL, "a") == false);
    CHECK(dict_contains(d, NULL) == false);
    dict_destroy(d);
}

static void test_TC_21(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_remove(NULL, "a") == DICT_ERR_NULL_ARG);
    CHECK(dict_remove(d, NULL) == DICT_ERR_NULL_ARG);
    CHECK(dict_size(d) == 1);
    dict_destroy(d);
}

static void test_TC_22(void)
{
    CHECK(dict_size(NULL) == 0);
    CHECK(dict_bucket_count(NULL) == 0);
}

static int count_cb(const char *key, const char *value, void *user_data)
{
    (void)key;
    (void)value;
    int *n = user_data;
    (*n)++;
    return 0;
}

static void test_TC_23(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    int n = 0;
    CHECK(dict_foreach(NULL, count_cb, &n) == DICT_ERR_NULL_ARG);
    CHECK(dict_foreach(d, NULL, &n) == DICT_ERR_NULL_ARG);
    CHECK(n == 0);
    dict_destroy(d);
}

static void test_TC_24(void)
{
    CHECK(dict_hash_djb2("") == 5381u);
    CHECK(dict_hash_djb2("a") == 177670u);
    CHECK(dict_hash_djb2("Aa") == 5862151u);
    CHECK(dict_hash_djb2("B@") == 5862151u);
    CHECK(dict_hash_djb2(NULL) == 5381u);
}

static void test_TC_25(void)
{
    uint64_t h1 = dict_hash_djb2(K1);
    uint64_t h2 = dict_hash_djb2(K2);
    uint64_t h3 = dict_hash_djb2(K3);
    CHECK(h1 == h2 && h2 == h3);
}

static void test_TC_26(void)
{
    Dict *d = make_collision_dict();
    CHECK(dict_remove(d, K2) == DICT_OK);
    CHECK(dict_size(d) == 2);
    CHECK(dict_get(d, K2) == NULL);
    const char *v1 = dict_get(d, K1);
    const char *v3 = dict_get(d, K3);
    CHECK(v1 != NULL && strcmp(v1, "v1") == 0);
    CHECK(v3 != NULL && strcmp(v3, "v3") == 0);
    dict_destroy(d);
}

static void test_TC_27(void)
{
    Dict *d = make_collision_dict();
    CHECK(dict_remove(d, K3) == DICT_OK);
    CHECK(dict_size(d) == 2);
    CHECK(dict_get(d, K3) == NULL);
    const char *v1 = dict_get(d, K1);
    const char *v2 = dict_get(d, K2);
    CHECK(v1 != NULL && strcmp(v1, "v1") == 0);
    CHECK(v2 != NULL && strcmp(v2, "v2") == 0);
    dict_destroy(d);
}

static void test_TC_28(void)
{
    Dict *d = make_collision_dict();
    CHECK(dict_remove(d, K1) == DICT_OK);
    CHECK(dict_size(d) == 2);
    CHECK(dict_get(d, K1) == NULL);
    const char *v2 = dict_get(d, K2);
    const char *v3 = dict_get(d, K3);
    CHECK(v2 != NULL && strcmp(v2, "v2") == 0);
    CHECK(v3 != NULL && strcmp(v3, "v3") == 0);
    dict_destroy(d);
}

static void test_TC_29(void)
{
    Dict *d = make_collision_dict();
    CHECK(dict_remove(d, K2) == DICT_OK);
    CHECK(dict_size(d) == 2);
    CHECK(dict_remove(d, K3) == DICT_OK);
    CHECK(dict_size(d) == 1);
    CHECK(dict_remove(d, K1) == DICT_OK);
    CHECK(dict_size(d) == 0);
    CHECK(dict_insert(d, K2, "again") == DICT_OK);
    const char *v = dict_get(d, K2);
    CHECK(v != NULL && strcmp(v, "again") == 0);
    CHECK(dict_size(d) == 1);
    dict_destroy(d);
}

static void test_TC_30(void)
{
    Dict *d = make_collision_dict();
    CHECK(dict_insert(d, K1, "new1") == DICT_REPLACED);
    CHECK(dict_size(d) == 3);
    const char *v1 = dict_get(d, K1);
    const char *v2 = dict_get(d, K2);
    const char *v3 = dict_get(d, K3);
    CHECK(v1 != NULL && strcmp(v1, "new1") == 0);
    CHECK(v2 != NULL && strcmp(v2, "v2") == 0);
    CHECK(v3 != NULL && strcmp(v3, "v3") == 0);
    dict_destroy(d);
}

static void test_TC_31(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[16], v[16];
    for (int i = 0; i < 12; i++) {
        snprintf(k, sizeof k, "k%d", i);
        snprintf(v, sizeof v, "v%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    CHECK(dict_bucket_count(d) == 16);
    CHECK(dict_insert(d, "k12", "v12") == DICT_OK);
    CHECK(dict_bucket_count(d) == 32);
    CHECK(dict_size(d) == 13);
    for (int i = 0; i < 13; i++) {
        snprintf(k, sizeof k, "k%d", i);
        snprintf(v, sizeof v, "v%d", i);
        const char *got = dict_get(d, k);
        CHECK(got != NULL && strcmp(got, v) == 0);
    }
    dict_destroy(d);
}

static void test_TC_32(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[16], v[16];
    for (int i = 0; i < 12; i++) {
        snprintf(k, sizeof k, "k%d", i);
        snprintf(v, sizeof v, "v%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    CHECK(dict_insert(d, "k0", "replaced") == DICT_REPLACED);
    CHECK(dict_bucket_count(d) == 16);
    CHECK(dict_size(d) == 12);
    dict_destroy(d);
}

static void test_TC_33(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[32], v[32];
    for (int i = 0; i < 100000; i++) {
        snprintf(k, sizeof k, "key%d", i);
        snprintf(v, sizeof v, "val%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    CHECK(dict_size(d) == 100000);
    CHECK(dict_bucket_count(d) == 262144);
    for (int i = 0; i < 100000; i++) {
        snprintf(k, sizeof k, "key%d", i);
        snprintf(v, sizeof v, "val%d", i);
        const char *got = dict_get(d, k);
        CHECK(got != NULL && strcmp(got, v) == 0);
    }
    dict_destroy(d);
}

static void test_TC_34(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "anchor", "fixed") == DICT_OK);
    const char *p = dict_get(d, "anchor");
    CHECK(p != NULL);
    char k[32], v[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(k, sizeof k, "x%d", i);
        snprintf(v, sizeof v, "y%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    CHECK(p == dict_get(d, "anchor"));
    CHECK(strcmp(p, "fixed") == 0);
    dict_destroy(d);
}

static void test_TC_35(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[16], v[16];
    for (int i = 0; i < 100; i++) {
        snprintf(k, sizeof k, "k%d", i);
        snprintf(v, sizeof v, "v%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    size_t b = dict_bucket_count(d);
    for (int i = 0; i < 100; i++) {
        snprintf(k, sizeof k, "k%d", i);
        CHECK(dict_remove(d, k) == DICT_OK);
    }
    CHECK(dict_size(d) == 0);
    CHECK(dict_bucket_count(d) == b);
    dict_destroy(d);
}

typedef struct {
    int count;
    int seen[50];
    int mismatch;
} Visit50;

static int visit50_cb(const char *key, const char *value, void *user_data)
{
    Visit50 *st = user_data;
    st->count++;
    int idx = atoi(key);
    if (idx < 0 || idx >= 50) {
        st->mismatch = 1;
        return 0;
    }
    st->seen[idx]++;
    char expect[16];
    snprintf(expect, sizeof expect, "v%d", idx);
    if (value == NULL || strcmp(value, expect) != 0) {
        st->mismatch = 1;
    }
    return 0;
}

static void test_TC_36(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[16], v[16];
    for (int i = 0; i < 50; i++) {
        snprintf(k, sizeof k, "%d", i);
        snprintf(v, sizeof v, "v%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    Visit50 st;
    memset(&st, 0, sizeof st);
    CHECK(dict_foreach(d, visit50_cb, &st) == DICT_OK);
    CHECK(st.count == 50);
    CHECK(st.mismatch == 0);
    for (int i = 0; i < 50; i++) {
        CHECK(st.seen[i] == 1);
    }
    dict_destroy(d);
}

static int stop_cb(const char *key, const char *value, void *user_data)
{
    (void)key;
    (void)value;
    int *n = user_data;
    (*n)++;
    return 1;
}

static void test_TC_37(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[16], v[16];
    for (int i = 0; i < 10; i++) {
        snprintf(k, sizeof k, "k%d", i);
        snprintf(v, sizeof v, "v%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    int n = 0;
    CHECK(dict_foreach(d, stop_cb, &n) == DICT_STOPPED);
    CHECK(n == 1);
    dict_destroy(d);
}

static void test_TC_38(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    int n = 0;
    CHECK(dict_foreach(d, count_cb, &n) == DICT_OK);
    CHECK(n == 0);
    dict_destroy(d);
}

static int sentinel_cb(const char *key, const char *value, void *user_data)
{
    (void)key;
    (void)value;
    int *sentinel = user_data;
    int *seen = sentinel + 1;
    if (user_data != sentinel) {
        *seen = -1;
        return 1;
    }
    (*seen)++;
    return 0;
}

static void test_TC_39(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_insert(d, "b", "2") == DICT_OK);
    CHECK(dict_insert(d, "c", "3") == DICT_OK);
    int box[2] = {0, 0}; /* box[0] is sentinel identity; box[1] counts visits */
    CHECK(dict_foreach(d, sentinel_cb, &box[0]) == DICT_OK);
    CHECK(box[1] == 3);
    dict_destroy(d);
}

static void test_TC_40(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    arm_alloc_fail();
    CHECK(dict_insert(d, "b", "2") == DICT_ERR_NOMEM);
    arm_alloc_fail();
    CHECK(dict_insert(d, "a", "new") == DICT_ERR_NOMEM);
    CHECK(dict_size(d) == 1);
    CHECK(dict_contains(d, "b") == false);
    const char *v = dict_get(d, "a");
    CHECK(v != NULL && strcmp(v, "1") == 0);
    dict_destroy(d);
}

static int remainder_cb(const char *key, const char *value, void *user_data)
{
    (void)key;
    (void)value;
    size_t *n = user_data;
    (*n)++;
    return 0;
}

static void test_TC_41(void)
{
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    char k[32], v[32], nv[32];
    for (int i = 0; i < 10000; i++) {
        snprintf(k, sizeof k, "k%d", i);
        snprintf(v, sizeof v, "v%d", i);
        CHECK(dict_insert(d, k, v) == DICT_OK);
    }
    CHECK(dict_size(d) == 10000);
    for (int i = 0; i < 10000; i += 2) {
        snprintf(k, sizeof k, "k%d", i);
        snprintf(nv, sizeof nv, "n%d", i);
        CHECK(dict_insert(d, k, nv) == DICT_REPLACED);
    }
    CHECK(dict_size(d) == 10000);
    for (int i = 0; i < 10000; i += 3) {
        snprintf(k, sizeof k, "k%d", i);
        CHECK(dict_remove(d, k) == DICT_OK);
    }
    CHECK(dict_size(d) == 6666);
    size_t visited = 0;
    CHECK(dict_foreach(d, remainder_cb, &visited) == DICT_OK);
    CHECK(visited == dict_size(d));
    dict_destroy(d);
}

int main(void)
{
    /* SPEC NOTE: C identifiers cannot contain '-', so tests are test_TC_NN
     * while printed IDs keep the hyphen from test_cases.md. */
#define RUN(id, fn)                                                             \
    do {                                                                        \
        g_failed = 0;                                                           \
        g_fail_reason = NULL;                                                   \
        fn();                                                                   \
        finish_test(id);                                                        \
    } while (0)

    RUN("TC-01", test_TC_01);
    RUN("TC-02", test_TC_02);
    RUN("TC-03", test_TC_03);
    RUN("TC-04", test_TC_04);
    RUN("TC-05", test_TC_05);
    RUN("TC-06", test_TC_06);
    RUN("TC-07", test_TC_07);
    RUN("TC-08", test_TC_08);
    RUN("TC-09", test_TC_09);
    RUN("TC-10", test_TC_10);
    RUN("TC-11", test_TC_11);
    RUN("TC-12", test_TC_12);
    RUN("TC-13", test_TC_13);
    RUN("TC-14", test_TC_14);
    RUN("TC-15", test_TC_15);
    RUN("TC-16", test_TC_16);
    RUN("TC-17", test_TC_17);
    RUN("TC-18", test_TC_18);
    RUN("TC-19", test_TC_19);
    RUN("TC-20", test_TC_20);
    RUN("TC-21", test_TC_21);
    RUN("TC-22", test_TC_22);
    RUN("TC-23", test_TC_23);
    RUN("TC-24", test_TC_24);
    RUN("TC-25", test_TC_25);
    RUN("TC-26", test_TC_26);
    RUN("TC-27", test_TC_27);
    RUN("TC-28", test_TC_28);
    RUN("TC-29", test_TC_29);
    RUN("TC-30", test_TC_30);
    RUN("TC-31", test_TC_31);
    RUN("TC-32", test_TC_32);
    RUN("TC-33", test_TC_33);
    RUN("TC-34", test_TC_34);
    RUN("TC-35", test_TC_35);
    RUN("TC-36", test_TC_36);
    RUN("TC-37", test_TC_37);
    RUN("TC-38", test_TC_38);
    RUN("TC-39", test_TC_39);
    RUN("TC-40", test_TC_40);
    RUN("TC-41", test_TC_41);

#undef RUN

    printf("SUMMARY: %d/%d passed\n", g_passed, g_total);
    return (g_failures == 0) ? 0 : 1;
}
