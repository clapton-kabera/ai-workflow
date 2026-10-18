#include "dict.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- malloc failure injection for TC-40 ---- */
static int malloc_fail_countdown = 0;

extern void *__real_malloc(size_t size);

void *__wrap_malloc(size_t size) {
    if (malloc_fail_countdown > 0) {
        malloc_fail_countdown--;
        return NULL;
    }
    return __real_malloc(size);
}

/* ---- test infrastructure ---- */
static int g_total  = 0;
static int g_passed = 0;
static int g_failures_cur = 0;
static char g_fail_reason[512];

#define CHECK(cond) do { \
    if (!(cond)) { \
        if (g_failures_cur == 0) \
            snprintf(g_fail_reason, sizeof(g_fail_reason), "%s", #cond); \
        g_failures_cur++; \
    } \
} while (0)

static void begin_test(void) {
    g_failures_cur = 0;
    g_fail_reason[0] = '\0';
    g_total++;
}

static void end_test(const char *id) {
    if (g_failures_cur == 0) {
        printf("PASS %s\n", id);
        g_passed++;
    } else {
        printf("FAIL %s: %s\n", id, g_fail_reason);
    }
}

/* ---- collision keys ---- */
#define K1 "AaAa"
#define K2 "AaB@"
#define K3 "B@Aa"

/* ---- callbacks ---- */
static int tc23_cb(const char *k, const char *v, void *ud) {
    (void)k; (void)v;
    (*(int *)ud)++;
    return 0;
}

typedef struct { int count; int seen[50]; int value_ok; } TC36State;

static int tc36_cb(const char *k, const char *v, void *ud) {
    TC36State *st = (TC36State *)ud;
    st->count++;
    int idx = atoi(k + 1); /* keys are "k<n>" */
    if (idx >= 0 && idx < 50) {
        st->seen[idx]++;
        char exp[32];
        snprintf(exp, sizeof(exp), "v%d", idx);
        if (strcmp(v, exp) != 0) st->value_ok = 0;
    }
    return 0;
}

static int tc37_cb(const char *k, const char *v, void *ud) {
    (void)k; (void)v;
    (*(int *)ud)++;
    return 1;
}

static int tc39_sentinel;
static int tc39_ok;
static int tc39_cb(const char *k, const char *v, void *ud) {
    (void)k; (void)v;
    if (ud != (void *)&tc39_sentinel) tc39_ok = 0;
    return 0;
}

static int tc41_count;
static int tc41_cb(const char *k, const char *v, void *ud) {
    (void)k; (void)v; (void)ud;
    tc41_count++;
    return 0;
}

/* ---- individual tests ---- */

static void test_TC01(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (d) {
        CHECK(dict_size(d) == (size_t)0);
        CHECK(dict_bucket_count(d) == (size_t)16);
        dict_destroy(d);
    }
    end_test("TC-01");
}

static void test_TC02(void) {
    begin_test();
    Dict *d = dict_create(64);
    CHECK(d != NULL);
    if (d) {
        CHECK(dict_size(d) == (size_t)0);
        CHECK(dict_bucket_count(d) == (size_t)64);
        dict_destroy(d);
    }
    end_test("TC-02");
}

static void test_TC03(void) {
    begin_test();
    dict_destroy(NULL);
    end_test("TC-03");
}

static void test_TC04(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    dict_destroy(d);
    end_test("TC-04");
}

static void test_TC05(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-05"); return; }
    DictStatus s = dict_insert(d, "name", "alice");
    CHECK(s == DICT_OK);
    const char *v = dict_get(d, "name");
    CHECK(v != NULL);
    if (v) CHECK(strcmp(v, "alice") == 0);
    CHECK(dict_contains(d, "name") == true);
    CHECK(dict_size(d) == (size_t)1);
    dict_destroy(d);
    end_test("TC-05");
}

static void test_TC06(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-06"); return; }
    CHECK(dict_insert(d, "a", "1") == DICT_OK);
    CHECK(dict_insert(d, "b", "2") == DICT_OK);
    CHECK(dict_insert(d, "c", "3") == DICT_OK);
    const char *va = dict_get(d, "a");
    const char *vb = dict_get(d, "b");
    const char *vc = dict_get(d, "c");
    CHECK(va != NULL && strcmp(va, "1") == 0);
    CHECK(vb != NULL && strcmp(vb, "2") == 0);
    CHECK(vc != NULL && strcmp(vc, "3") == 0);
    CHECK(dict_size(d) == (size_t)3);
    dict_destroy(d);
    end_test("TC-06");
}

static void test_TC07(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-07"); return; }
    CHECK(dict_insert(d, "", "") == DICT_OK);
    CHECK(dict_insert(d, "k", "") == DICT_OK);
    const char *v1 = dict_get(d, "");
    const char *v2 = dict_get(d, "k");
    CHECK(v1 != NULL);
    CHECK(v2 != NULL);
    if (v1) CHECK(strcmp(v1, "") == 0);
    if (v2) CHECK(strcmp(v2, "") == 0);
    CHECK(dict_size(d) == (size_t)2);
    dict_destroy(d);
    end_test("TC-07");
}

static void test_TC08(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-08"); return; }
    char k[] = "key";
    char v[] = "val";
    CHECK(dict_insert(d, k, v) == DICT_OK);
    const char *stored = dict_get(d, "key");
    strcpy(k, "zzz");
    strcpy(v, "xxx");
    CHECK(stored != NULL);
    if (stored) CHECK(strcmp(stored, "val") == 0);
    CHECK(dict_get(d, "zzz") == NULL);
    CHECK(stored != (const char *)v);
    dict_destroy(d);
    end_test("TC-08");
}

static void test_TC09(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-09"); return; }
    CHECK(dict_insert(d, "Key", "upper") == DICT_OK);
    CHECK(dict_insert(d, "key", "lower") == DICT_OK);
    CHECK(dict_size(d) == (size_t)2);
    const char *vu = dict_get(d, "Key");
    const char *vl = dict_get(d, "key");
    CHECK(vu != NULL && strcmp(vu, "upper") == 0);
    CHECK(vl != NULL && strcmp(vl, "lower") == 0);
    dict_destroy(d);
    end_test("TC-09");
}

static void test_TC10(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-10"); return; }
    dict_insert(d, "k", "old");
    DictStatus s = dict_insert(d, "k", "new");
    CHECK(s == DICT_REPLACED);
    const char *v = dict_get(d, "k");
    CHECK(v != NULL && strcmp(v, "new") == 0);
    CHECK(dict_size(d) == (size_t)1);
    dict_destroy(d);
    end_test("TC-10");
}

static void test_TC11(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-11"); return; }
    int first_ok = 0, replace_ok = 1;
    for (int i = 0; i < 1000; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "v%d", i);
        DictStatus s = dict_insert(d, "k", buf);
        if (i == 0) { if (s == DICT_OK) first_ok = 1; }
        else { if (s != DICT_REPLACED) replace_ok = 0; }
    }
    CHECK(first_ok);
    CHECK(replace_ok);
    const char *v = dict_get(d, "k");
    CHECK(v != NULL && strcmp(v, "v999") == 0);
    CHECK(dict_size(d) == (size_t)1);
    dict_destroy(d);
    end_test("TC-11");
}

static void test_TC12(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-12"); return; }
    dict_insert(d, "k", "same");
    DictStatus s = dict_insert(d, "k", "same");
    CHECK(s == DICT_REPLACED);
    const char *v = dict_get(d, "k");
    CHECK(v != NULL && strcmp(v, "same") == 0);
    CHECK(dict_size(d) == (size_t)1);
    dict_destroy(d);
    end_test("TC-12");
}

static void test_TC13(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-13"); return; }
    dict_insert(d, "a", "1");
    CHECK(dict_get(d, "b") == NULL);
    CHECK(dict_size(d) == (size_t)1);
    dict_destroy(d);
    end_test("TC-13");
}

static void test_TC14(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-14"); return; }
    dict_insert(d, "a", "1");
    CHECK(dict_contains(d, "a") == true);
    CHECK(dict_contains(d, "b") == false);
    dict_destroy(d);
    end_test("TC-14");
}

static void test_TC15(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-15"); return; }
    dict_insert(d, "a", "1");
    CHECK(dict_remove(d, "zzz") == DICT_ERR_NOT_FOUND);
    CHECK(dict_size(d) == (size_t)1);
    const char *v = dict_get(d, "a");
    CHECK(v != NULL && strcmp(v, "1") == 0);
    dict_destroy(d);
    end_test("TC-15");
}

static void test_TC16(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-16"); return; }
    CHECK(dict_get(d, "x") == NULL);
    CHECK(dict_contains(d, "x") == false);
    CHECK(dict_remove(d, "x") == DICT_ERR_NOT_FOUND);
    CHECK(dict_size(d) == (size_t)0);
    dict_destroy(d);
    end_test("TC-16");
}

static void test_TC17(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-17"); return; }
    dict_insert(d, "a", "1");
    CHECK(dict_remove(d, "a") == DICT_OK);
    CHECK(dict_remove(d, "a") == DICT_ERR_NOT_FOUND);
    CHECK(dict_size(d) == (size_t)0);
    CHECK(dict_get(d, "a") == NULL);
    dict_destroy(d);
    end_test("TC-17");
}

static void test_TC18(void) {
    begin_test();
    CHECK(dict_insert(NULL, "k", "v") == DICT_ERR_NULL_ARG);
    end_test("TC-18");
}

static void test_TC19(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-19"); return; }
    dict_insert(d, "a", "1");
    CHECK(dict_insert(d, NULL, "v") == DICT_ERR_NULL_ARG);
    CHECK(dict_insert(d, "k", NULL) == DICT_ERR_NULL_ARG);
    CHECK(dict_insert(d, "a", NULL) == DICT_ERR_NULL_ARG);
    CHECK(dict_size(d) == (size_t)1);
    CHECK(dict_contains(d, "k") == false);
    const char *v = dict_get(d, "a");
    CHECK(v != NULL && strcmp(v, "1") == 0);
    dict_destroy(d);
    end_test("TC-19");
}

static void test_TC20(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-20"); return; }
    dict_insert(d, "a", "1");
    CHECK(dict_get(NULL, "a") == NULL);
    CHECK(dict_get(d, NULL) == NULL);
    CHECK(dict_contains(NULL, "a") == false);
    CHECK(dict_contains(d, NULL) == false);
    dict_destroy(d);
    end_test("TC-20");
}

static void test_TC21(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-21"); return; }
    dict_insert(d, "a", "1");
    CHECK(dict_remove(NULL, "a") == DICT_ERR_NULL_ARG);
    CHECK(dict_remove(d, NULL) == DICT_ERR_NULL_ARG);
    CHECK(dict_size(d) == (size_t)1);
    dict_destroy(d);
    end_test("TC-21");
}

static void test_TC22(void) {
    begin_test();
    CHECK(dict_size(NULL) == (size_t)0);
    CHECK(dict_bucket_count(NULL) == (size_t)0);
    end_test("TC-22");
}

static void test_TC23(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-23"); return; }
    dict_insert(d, "a", "1");
    int n = 0;
    CHECK(dict_foreach(NULL, tc23_cb, &n) == DICT_ERR_NULL_ARG);
    CHECK(dict_foreach(d, NULL, &n) == DICT_ERR_NULL_ARG);
    CHECK(n == 0);
    dict_destroy(d);
    end_test("TC-23");
}

static void test_TC24(void) {
    begin_test();
    CHECK(dict_hash_djb2("") == (uint64_t)5381);
    CHECK(dict_hash_djb2("a") == (uint64_t)177670);
    CHECK(dict_hash_djb2("Aa") == (uint64_t)5862151);
    CHECK(dict_hash_djb2("B@") == (uint64_t)5862151);
    CHECK(dict_hash_djb2(NULL) == (uint64_t)5381);
    end_test("TC-24");
}

static void test_TC25(void) {
    begin_test();
    uint64_t h1 = dict_hash_djb2(K1);
    uint64_t h2 = dict_hash_djb2(K2);
    uint64_t h3 = dict_hash_djb2(K3);
    CHECK(h1 == h2);
    CHECK(h2 == h3);
    end_test("TC-25");
}

static void test_TC26(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-26"); return; }
    dict_insert(d, K1, "v1");
    dict_insert(d, K2, "v2");
    dict_insert(d, K3, "v3");
    CHECK(dict_remove(d, K2) == DICT_OK);
    CHECK(dict_size(d) == (size_t)2);
    CHECK(dict_get(d, K2) == NULL);
    const char *v1 = dict_get(d, K1);
    const char *v3 = dict_get(d, K3);
    CHECK(v1 != NULL && strcmp(v1, "v1") == 0);
    CHECK(v3 != NULL && strcmp(v3, "v3") == 0);
    dict_destroy(d);
    end_test("TC-26");
}

static void test_TC27(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-27"); return; }
    dict_insert(d, K1, "v1");
    dict_insert(d, K2, "v2");
    dict_insert(d, K3, "v3");
    CHECK(dict_remove(d, K3) == DICT_OK);
    CHECK(dict_size(d) == (size_t)2);
    CHECK(dict_get(d, K3) == NULL);
    const char *v1 = dict_get(d, K1);
    const char *v2 = dict_get(d, K2);
    CHECK(v1 != NULL && strcmp(v1, "v1") == 0);
    CHECK(v2 != NULL && strcmp(v2, "v2") == 0);
    dict_destroy(d);
    end_test("TC-27");
}

static void test_TC28(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-28"); return; }
    dict_insert(d, K1, "v1");
    dict_insert(d, K2, "v2");
    dict_insert(d, K3, "v3");
    CHECK(dict_remove(d, K1) == DICT_OK);
    CHECK(dict_size(d) == (size_t)2);
    CHECK(dict_get(d, K1) == NULL);
    const char *v2 = dict_get(d, K2);
    const char *v3 = dict_get(d, K3);
    CHECK(v2 != NULL && strcmp(v2, "v2") == 0);
    CHECK(v3 != NULL && strcmp(v3, "v3") == 0);
    dict_destroy(d);
    end_test("TC-28");
}

static void test_TC29(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-29"); return; }
    dict_insert(d, K1, "v1");
    dict_insert(d, K2, "v2");
    dict_insert(d, K3, "v3");
    CHECK(dict_remove(d, K2) == DICT_OK);
    CHECK(dict_size(d) == (size_t)2);
    CHECK(dict_remove(d, K3) == DICT_OK);
    CHECK(dict_size(d) == (size_t)1);
    CHECK(dict_remove(d, K1) == DICT_OK);
    CHECK(dict_size(d) == (size_t)0);
    CHECK(dict_insert(d, K2, "again") == DICT_OK);
    const char *v = dict_get(d, K2);
    CHECK(v != NULL && strcmp(v, "again") == 0);
    CHECK(dict_size(d) == (size_t)1);
    dict_destroy(d);
    end_test("TC-29");
}

static void test_TC30(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-30"); return; }
    dict_insert(d, K1, "v1");
    dict_insert(d, K2, "v2");
    dict_insert(d, K3, "v3");
    CHECK(dict_insert(d, K1, "new1") == DICT_REPLACED);
    CHECK(dict_size(d) == (size_t)3);
    const char *v1 = dict_get(d, K1);
    const char *v2 = dict_get(d, K2);
    const char *v3 = dict_get(d, K3);
    CHECK(v1 != NULL && strcmp(v1, "new1") == 0);
    CHECK(v2 != NULL && strcmp(v2, "v2") == 0);
    CHECK(v3 != NULL && strcmp(v3, "v3") == 0);
    dict_destroy(d);
    end_test("TC-30");
}

static void test_TC31(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-31"); return; }
    for (int i = 0; i < 12; i++) {
        char k[16];
        snprintf(k, sizeof(k), "key%d", i);
        dict_insert(d, k, "v");
    }
    CHECK(dict_bucket_count(d) == (size_t)16);
    dict_insert(d, "key12", "v");
    CHECK(dict_bucket_count(d) == (size_t)32);
    CHECK(dict_size(d) == (size_t)13);
    int all_found = 1;
    for (int i = 0; i < 13; i++) {
        char k[16];
        snprintf(k, sizeof(k), "key%d", i);
        if (!dict_get(d, k)) { all_found = 0; break; }
    }
    CHECK(all_found);
    dict_destroy(d);
    end_test("TC-31");
}

static void test_TC32(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-32"); return; }
    for (int i = 0; i < 12; i++) {
        char k[16];
        snprintf(k, sizeof(k), "key%d", i);
        dict_insert(d, k, "v");
    }
    CHECK(dict_bucket_count(d) == (size_t)16);
    CHECK(dict_insert(d, "key0", "newval") == DICT_REPLACED);
    CHECK(dict_bucket_count(d) == (size_t)16);
    CHECK(dict_size(d) == (size_t)12);
    dict_destroy(d);
    end_test("TC-32");
}

static void test_TC33(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-33"); return; }
    int all_ok = 1;
    for (int i = 0; i < 100000; i++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(val, sizeof(val), "val%d", i);
        if (dict_insert(d, key, val) != DICT_OK) { all_ok = 0; break; }
    }
    CHECK(all_ok);
    CHECK(dict_size(d) == (size_t)100000);
    int all_found = 1;
    for (int i = 0; i < 100000; i++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(val, sizeof(val), "val%d", i);
        const char *v = dict_get(d, key);
        if (!v || strcmp(v, val) != 0) { all_found = 0; break; }
    }
    CHECK(all_found);
    CHECK(dict_bucket_count(d) == (size_t)262144);
    dict_destroy(d);
    end_test("TC-33");
}

static void test_TC34(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-34"); return; }
    dict_insert(d, "anchor", "fixed");
    const char *p = dict_get(d, "anchor");
    CHECK(p != NULL);
    for (int i = 0; i < 1000; i++) {
        char k[32];
        snprintf(k, sizeof(k), "extra%d", i);
        dict_insert(d, k, "v");
    }
    CHECK(p == dict_get(d, "anchor"));
    CHECK(p != NULL && strcmp(p, "fixed") == 0);
    dict_destroy(d);
    end_test("TC-34");
}

static void test_TC35(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-35"); return; }
    for (int i = 0; i < 100; i++) {
        char k[16];
        snprintf(k, sizeof(k), "key%d", i);
        dict_insert(d, k, "v");
    }
    size_t b = dict_bucket_count(d);
    for (int i = 0; i < 100; i++) {
        char k[16];
        snprintf(k, sizeof(k), "key%d", i);
        dict_remove(d, k);
    }
    CHECK(dict_size(d) == (size_t)0);
    CHECK(dict_bucket_count(d) == b);
    dict_destroy(d);
    end_test("TC-35");
}

static void test_TC36(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-36"); return; }
    for (int i = 0; i < 50; i++) {
        char k[16], v[16];
        snprintf(k, sizeof(k), "k%d", i);
        snprintf(v, sizeof(v), "v%d", i);
        dict_insert(d, k, v);
    }
    TC36State st;
    st.count = 0;
    st.value_ok = 1;
    int i;
    for (i = 0; i < 50; i++) st.seen[i] = 0;
    DictStatus s = dict_foreach(d, tc36_cb, &st);
    CHECK(s == DICT_OK);
    CHECK(st.count == 50);
    CHECK(st.value_ok);
    int all_once = 1;
    for (i = 0; i < 50; i++) {
        if (st.seen[i] != 1) { all_once = 0; break; }
    }
    CHECK(all_once);
    dict_destroy(d);
    end_test("TC-36");
}

static void test_TC37(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-37"); return; }
    for (int i = 0; i < 10; i++) {
        char k[16];
        snprintf(k, sizeof(k), "k%d", i);
        dict_insert(d, k, "v");
    }
    int n = 0;
    DictStatus s = dict_foreach(d, tc37_cb, &n);
    CHECK(s == DICT_STOPPED);
    CHECK(n == 1);
    dict_destroy(d);
    end_test("TC-37");
}

static void test_TC38(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-38"); return; }
    int n = 0;
    DictStatus s = dict_foreach(d, tc23_cb, &n);
    CHECK(s == DICT_OK);
    CHECK(n == 0);
    dict_destroy(d);
    end_test("TC-38");
}

static void test_TC39(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-39"); return; }
    dict_insert(d, "a", "1");
    dict_insert(d, "b", "2");
    dict_insert(d, "c", "3");
    tc39_ok = 1;
    DictStatus s = dict_foreach(d, tc39_cb, &tc39_sentinel);
    CHECK(s == DICT_OK);
    CHECK(tc39_ok);
    dict_destroy(d);
    end_test("TC-39");
}

static void test_TC40(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-40"); return; }
    CHECK(dict_insert(d, "a", "1") == DICT_OK);

    malloc_fail_countdown = 1;
    DictStatus s1 = dict_insert(d, "b", "2");
    CHECK(s1 == DICT_ERR_NOMEM);

    malloc_fail_countdown = 1;
    DictStatus s2 = dict_insert(d, "a", "new");
    CHECK(s2 == DICT_ERR_NOMEM);

    CHECK(dict_size(d) == (size_t)1);
    CHECK(dict_contains(d, "b") == false);
    const char *v = dict_get(d, "a");
    CHECK(v != NULL && strcmp(v, "1") == 0);
    dict_destroy(d);
    end_test("TC-40");
}

static void test_TC41(void) {
    begin_test();
    Dict *d = dict_create(0);
    CHECK(d != NULL);
    if (!d) { end_test("TC-41"); return; }

    int all_ok = 1;
    for (int i = 0; i < 10000; i++) {
        char k[32], v[32];
        snprintf(k, sizeof(k), "key%d", i);
        snprintf(v, sizeof(v), "val%d", i);
        if (dict_insert(d, k, v) != DICT_OK) { all_ok = 0; break; }
    }
    CHECK(all_ok);
    CHECK(dict_size(d) == (size_t)10000);

    int rep_ok = 1;
    for (int i = 0; i < 10000; i += 2) {
        char k[32], v[32];
        snprintf(k, sizeof(k), "key%d", i);
        snprintf(v, sizeof(v), "newval%d", i);
        if (dict_insert(d, k, v) != DICT_REPLACED) { rep_ok = 0; break; }
    }
    CHECK(rep_ok);
    CHECK(dict_size(d) == (size_t)10000);

    int rem_ok = 1;
    for (int i = 0; i < 10000; i += 3) {
        char k[32];
        snprintf(k, sizeof(k), "key%d", i);
        if (dict_remove(d, k) != DICT_OK) { rem_ok = 0; break; }
    }
    CHECK(rem_ok);
    CHECK(dict_size(d) == (size_t)6666);

    tc41_count = 0;
    DictStatus fs = dict_foreach(d, tc41_cb, NULL);
    CHECK(fs == DICT_OK);
    CHECK(tc41_count == (int)dict_size(d));

    dict_destroy(d);
    end_test("TC-41");
}

/* ---- main ---- */
int main(void) {
    test_TC01(); test_TC02(); test_TC03(); test_TC04(); test_TC05();
    test_TC06(); test_TC07(); test_TC08(); test_TC09(); test_TC10();
    test_TC11(); test_TC12(); test_TC13(); test_TC14(); test_TC15();
    test_TC16(); test_TC17(); test_TC18(); test_TC19(); test_TC20();
    test_TC21(); test_TC22(); test_TC23(); test_TC24(); test_TC25();
    test_TC26(); test_TC27(); test_TC28(); test_TC29(); test_TC30();
    test_TC31(); test_TC32(); test_TC33(); test_TC34(); test_TC35();
    test_TC36(); test_TC37(); test_TC38(); test_TC39(); test_TC40();
    test_TC41();
    printf("SUMMARY: %d/%d passed\n", g_passed, g_total);
    return (g_passed == g_total) ? 0 : 1;
}
