## FILE: dict.h
/*
 * dict.h - String-to-string dictionary (hash table, separate chaining).
 *
 * OVERVIEW
 *   A Dict maps null-terminated C strings (keys) to null-terminated C
 *   strings (values). Keys are compared byte-wise with strcmp()
 *   (case-sensitive). The empty string "" is a valid key and a valid value.
 *
 * OWNERSHIP
 *   The dictionary stores its own heap copies of every key and value.
 *   Callers keep ownership of the strings they pass in and may modify or
 *   free them immediately after a call returns. All internal copies are
 *   freed by dict_remove() (for that entry), by dict_insert() (the old
 *   value, on replacement), and by dict_destroy() (everything).
 *
 * HASHING AND LAYOUT
 *   - Hash function: djb2 (see dict_hash_djb2), computed in uint64_t with
 *     wrap-around (modulo 2^64) arithmetic.
 *   - Bucket index: dict_hash_djb2(key) % bucket_count.
 *   - Each bucket is a singly linked list. A NEW key is inserted at the
 *     HEAD of its bucket's chain. Replacing an existing key's value does
 *     not move its node.
 *   - Order of entries after a resize, and iteration order, are
 *     unspecified.
 *
 * RESIZING
 *   - Default initial bucket count is DICT_DEFAULT_CAPACITY (16).
 *   - After a NEW key is added, if size / bucket_count > 0.75
 *     (evaluated exactly as: size * 4 > bucket_count * 3), the bucket
 *     count is doubled and all entries are rehashed.
 *   - Replacing a value never triggers a resize. Removal never shrinks
 *     the table.
 *   - If the resize allocation fails, or doubling would overflow size_t,
 *     the resize is skipped silently; the dictionary stays valid and
 *     fully functional (only over the load-factor target), and the
 *     triggering insert still reports success.
 *   - Resizing relinks existing nodes; it does not reallocate key or
 *     value strings.
 *
 * NULL ARGUMENTS AND MISSING KEYS (summary; details per function)
 *   - Functions returning DictStatus return DICT_ERR_NULL_ARG for any
 *     required pointer that is NULL, and leave the dictionary unchanged.
 *   - Query functions return a neutral value for NULL input:
 *       dict_get -> NULL, dict_contains -> false, dict_size -> 0,
 *       dict_bucket_count -> 0.
 *   - dict_destroy(NULL) is a no-op.
 *   - A missing key yields NULL (get), false (contains), or
 *     DICT_ERR_NOT_FOUND (remove). Since stored values are never NULL,
 *     a NULL return from dict_get unambiguously means "not present".
 *
 * THREAD SAFETY
 *   None. Concurrent read-only calls (get, contains, size, bucket_count,
 *   foreach) on the same Dict are safe only if no thread is mutating it.
 *   Distinct Dict objects are independent.
 */

#ifndef DICT_H
#define DICT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bucket count used when dict_create() is called with 0. */
#define DICT_DEFAULT_CAPACITY 16u

/* Opaque dictionary type. Only manipulate through the functions below. */
typedef struct Dict Dict;

/*
 * Status codes. Non-negative values indicate success; negative values
 * indicate errors. On any error the dictionary is left unchanged.
 */
typedef enum DictStatus {
    DICT_OK            =  0, /* Success (new key inserted / entry removed /
                                iteration completed).                      */
    DICT_REPLACED      =  1, /* Success: existing key's value replaced.   */
    DICT_STOPPED       =  2, /* Success: foreach callback stopped early.  */
    DICT_ERR_NULL_ARG  = -1, /* A required pointer argument was NULL.     */
    DICT_ERR_NOMEM     = -2, /* Memory allocation failed.                 */
    DICT_ERR_NOT_FOUND = -3  /* Key not present.                          */
} DictStatus;

/*
 * Callback type for dict_foreach().
 *
 * Parameters:
 *   key       - The entry's key. Borrowed; valid only during the call.
 *   value     - The entry's value. Borrowed; valid only during the call.
 *   user_data - The pointer passed to dict_foreach(), unchanged.
 *
 * Return:
 *   0 to continue iteration; any non-zero value to stop immediately.
 *
 * Restrictions:
 *   The callback must not modify the dictionary being iterated (no
 *   insert, remove, or destroy on it). Doing so is undefined behavior.
 *   It must not free or write through key or value.
 */
typedef int (*DictVisitFn)(const char *key, const char *value,
                           void *user_data);

/*
 * dict_create - Allocate a new, empty dictionary.
 *
 * Parameters:
 *   initial_capacity - Initial number of buckets. 0 selects
 *                      DICT_DEFAULT_CAPACITY (16). Any other value is
 *                      used exactly as given.
 *
 * Return:
 *   Pointer to a new Dict with size 0, or NULL if memory allocation
 *   fails (nothing is leaked in that case).
 *
 * Ownership:
 *   The caller owns the returned Dict and must release it with
 *   dict_destroy().
 *
 * Errors:
 *   NULL on allocation failure (including if initial_capacity is so large
 *   that the bucket array cannot be allocated).
 */
Dict *dict_create(size_t initial_capacity);

/*
 * dict_destroy - Free a dictionary and everything it owns.
 *
 * Parameters:
 *   d - Dictionary to destroy, or NULL.
 *
 * Return:
 *   None.
 *
 * Ownership:
 *   Frees every stored key copy, value copy, chain node, the bucket
 *   array, and the Dict itself. After this call, d and every pointer
 *   previously returned by dict_get() on d are invalid.
 *
 * Errors:
 *   d == NULL is a no-op. Destroying the same Dict twice is undefined
 *   behavior.
 */
void dict_destroy(Dict *d);

/*
 * dict_insert - Insert a key/value pair, or replace the value of an
 *               existing key.
 *
 * Parameters:
 *   d     - Target dictionary. Must not be NULL.
 *   key   - Null-terminated key. Must not be NULL. May be "".
 *   value - Null-terminated value. Must not be NULL. May be "".
 *
 * Return:
 *   DICT_OK            - key was not present; a new entry was added and
 *                        size increased by 1. A resize may have occurred.
 *   DICT_REPLACED      - key was present; its value was replaced and
 *                        size is unchanged. No resize occurs.
 *   DICT_ERR_NULL_ARG  - d, key, or value was NULL. No change.
 *   DICT_ERR_NOMEM     - allocation of the needed copies failed.
 *                        No change: for a new key, nothing is added;
 *                        for an existing key, the old value is kept.
 *
 * Ownership:
 *   key and value are copied; the caller retains ownership of the
 *   arguments. On replacement, the old value copy is freed only after the
 *   new copy has been allocated successfully; any pointer previously
 *   returned by dict_get() for this key becomes invalid.
 *   dict_get() pointers for OTHER keys remain valid, even across a
 *   resize.
 *
 * Errors:
 *   See return values. A failed resize is not an error (see RESIZING).
 */
DictStatus dict_insert(Dict *d, const char *key, const char *value);

/*
 * dict_get - Look up the value stored for a key.
 *
 * Parameters:
 *   d   - Dictionary to search. May be NULL.
 *   key - Null-terminated key. May be NULL.
 *
 * Return:
 *   Pointer to the stored value string if key is present; NULL if key is
 *   absent, or if d or key is NULL.
 *
 * Ownership:
 *   The returned pointer is borrowed from the dictionary. The caller must
 *   not modify or free it. It remains valid until this key's value is
 *   replaced, this key is removed, or d is destroyed. Insertions of other
 *   keys (including ones that trigger a resize) and removals of other
 *   keys do not invalidate it.
 *
 * Errors:
 *   None reported beyond the NULL return described above.
 */
const char *dict_get(const Dict *d, const char *key);

/*
 * dict_remove - Remove a key and its value.
 *
 * Parameters:
 *   d   - Target dictionary. Must not be NULL.
 *   key - Null-terminated key to remove. Must not be NULL.
 *
 * Return:
 *   DICT_OK            - entry found and removed; size decreased by 1.
 *   DICT_ERR_NOT_FOUND - key not present. No change.
 *   DICT_ERR_NULL_ARG  - d or key was NULL. No change.
 *
 * Ownership:
 *   Frees the stored key copy, value copy, and chain node. Any pointer
 *   previously returned by dict_get() for this key becomes invalid.
 *   Works for an entry at any position in its chain (head, middle,
 *   tail). The bucket array is never shrunk.
 *
 * Errors:
 *   See return values. This function never allocates and cannot fail
 *   with DICT_ERR_NOMEM.
 */
DictStatus dict_remove(Dict *d, const char *key);

/*
 * dict_contains - Test whether a key is present.
 *
 * Parameters:
 *   d   - Dictionary to search. May be NULL.
 *   key - Null-terminated key. May be NULL.
 *
 * Return:
 *   true if key is present; false if absent, or if d or key is NULL.
 *
 * Ownership:
 *   No ownership transfer. Does not modify d.
 *
 * Errors:
 *   None.
 */
bool dict_contains(const Dict *d, const char *key);

/*
 * dict_size - Number of entries (distinct keys) stored.
 *
 * Parameters:
 *   d - Dictionary. May be NULL.
 *
 * Return:
 *   Number of entries; 0 if d is NULL.
 *
 * Ownership:
 *   No ownership transfer. Does not modify d.
 *
 * Errors:
 *   None.
 */
size_t dict_size(const Dict *d);

/*
 * dict_bucket_count - Current number of buckets (diagnostic/testing aid).
 *
 * Parameters:
 *   d - Dictionary. May be NULL.
 *
 * Return:
 *   Current bucket count (always >= 1 for a valid Dict); 0 if d is NULL.
 *
 * Ownership:
 *   No ownership transfer. Does not modify d.
 *
 * Errors:
 *   None.
 */
size_t dict_bucket_count(const Dict *d);

/*
 * dict_foreach - Invoke a callback once for every entry.
 *
 * Parameters:
 *   d         - Dictionary to iterate. Must not be NULL.
 *   fn        - Callback. Must not be NULL.
 *   user_data - Arbitrary pointer passed through to fn. May be NULL.
 *
 * Return:
 *   DICT_OK           - every entry was visited exactly once (including
 *                       the case of an empty dictionary, where fn is
 *                       never called).
 *   DICT_STOPPED      - fn returned non-zero; iteration stopped
 *                       immediately after that call.
 *   DICT_ERR_NULL_ARG - d or fn was NULL; fn is never called.
 *
 * Ownership:
 *   key and value passed to fn are borrowed and valid only for the
 *   duration of that call. Iteration order is unspecified.
 *
 * Errors:
 *   See return values. Modifying d from within fn is undefined behavior.
 */
DictStatus dict_foreach(const Dict *d, DictVisitFn fn, void *user_data);

/*
 * dict_hash_djb2 - The djb2 hash used by the dictionary.
 *
 *   hash = 5381;
 *   for each byte c in s (as unsigned char): hash = hash * 33 + c;
 *   computed in uint64_t with modulo-2^64 wrap-around.
 *
 * Parameters:
 *   s - Null-terminated string. May be NULL.
 *
 * Return:
 *   The 64-bit djb2 hash of s. If s is NULL, returns 5381 (the hash of
 *   the empty string).
 *
 * Ownership:
 *   No ownership transfer; s is only read.
 *
 * Errors:
 *   None.
 */
uint64_t dict_hash_djb2(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* DICT_H */

## FILE: test_cases.md

#Test Cases: dict (string-to-string hash table) 
 
All tests should run under a leak checker such as `valgrind --leak-check=full --error-exitcode=1` or AddressSanitizer with LeakSanitizer, and every test ends with `dict_destroy(d)` unless stated otherwise.
 
The collision keys `K1 = "AaAa"`, `K2 = "AaB@"`, `K3 = "B@Aa"` all have the same djb2 hash, because `"Aa"` and `"B@"` collide (`65*33+97 == 66*33+64`). That means they always share a chain regardless of bucket count. Since new keys are inserted at the chain head, inserting K1, K2, K3 in that order produces the chain K3 → K2 → K1 (head, middle, tail).
 
1. **TC-01**
   - Description: Create with default capacity.
   - Setup: None.
   - Action: `d = dict_create(0)`.
   - Expected: `d != NULL`, `dict_size(d) == 0`, `dict_bucket_count(d) == 16`.
2. **TC-02**
   - Description: Create with explicit capacity.
   - Setup: None.
   - Action: `d = dict_create(64)`.
   - Expected: `d != NULL`, `dict_size(d) == 0`, `dict_bucket_count(d) == 64`.
3. **TC-03**
   - Description: Destroy NULL is a no-op.
   - Setup: None.
   - Action: `dict_destroy(NULL)`.
   - Expected: No crash and no memory errors reported.
4. **TC-04**
   - Description: Destroy an empty dictionary.
   - Setup: `d = dict_create(0)`.
   - Action: `dict_destroy(d)`.
   - Expected: 0 bytes leaked and no invalid accesses.
5. **TC-05**
   - Description: Insert and get a single pair.
   - Setup: `d = dict_create(0)`.
   - Action: `dict_insert(d, "name", "alice")`.
   - Expected: The call returns `DICT_OK`, `strcmp(dict_get(d, "name"), "alice") == 0`, `dict_contains(d, "name") == true`, and `dict_size(d) == 1`.
6. **TC-06**
   - Description: Insert multiple distinct keys.
   - Setup: `d = dict_create(0)`.
   - Action: Insert `"a"→"1"`, `"b"→"2"`, `"c"→"3"`.
   - Expected: Each insert returns `DICT_OK`, each `dict_get` returns its own value, and `dict_size(d) == 3`.
7. **TC-07**
   - Description: An empty-string key and an empty-string value are valid.
   - Setup: `d = dict_create(0)`.
   - Action: `dict_insert(d, "", "")` and `dict_insert(d, "k", "")`.
   - Expected: Both return `DICT_OK`. `dict_get(d, "")` and `dict_get(d, "k")` are non-NULL and point to `""`. `dict_size(d) == 2`.
8. **TC-08**
   - Description: The dictionary stores its own copies of keys and values.
   - Setup: `d = dict_create(0)`. Create `char k[] = "key"` and `char v[] = "val"`.
   - Action: `dict_insert(d, k, v)`, then overwrite the buffers with `strcpy(k, "zzz")` and `strcpy(v, "xxx")`.
   - Expected: `strcmp(dict_get(d, "key"), "val") == 0`, `dict_get(d, "zzz") == NULL`, and the returned pointer differs from `v`.
9. **TC-09**
   - Description: Keys are case-sensitive.
   - Setup: `d = dict_create(0)`.
   - Action: Insert `"Key"→"upper"` and `"key"→"lower"`.
   - Expected: Both return `DICT_OK`, `dict_size(d) == 2`, and each key returns its own value.
10. **TC-10**
    - Description: Inserting a duplicate key replaces its value.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "k", "old")`.
    - Action: `dict_insert(d, "k", "new")`.
    - Expected: The call returns `DICT_REPLACED`, `strcmp(dict_get(d, "k"), "new") == 0`, and `dict_size(d) == 1`.
11. **TC-11**
    - Description: Repeated replacement does not leak memory.
    - Setup: `d = dict_create(0)`.
    - Action: Call `dict_insert(d, "k", buf)` 1000 times with a different `buf` each time (`"v0"`…`"v999"`), then destroy.
    - Expected: The first call returns `DICT_OK` and the rest return `DICT_REPLACED`. The final value is `"v999"` and `dict_size(d) == 1`. 0 bytes leaked.
12. **TC-12**
    - Description: Replace a value with an identical string.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "k", "same")`.
    - Action: `dict_insert(d, "k", "same")`.
    - Expected: The call returns `DICT_REPLACED`, the value is `"same"`, `dict_size(d) == 1`, and no leak.
13. **TC-13**
    - Description: Get a missing key.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`.
    - Action: `dict_get(d, "b")`.
    - Expected: Returns `NULL`, and `dict_size(d)` is still 1.
14. **TC-14**
    - Description: Contains reports present and missing keys correctly.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`.
    - Action: `dict_contains(d, "a")` and `dict_contains(d, "b")`.
    - Expected: `true` and `false` respectively.
15. **TC-15**
    - Description: Remove a missing key.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`.
    - Action: `dict_remove(d, "zzz")`.
    - Expected: Returns `DICT_ERR_NOT_FOUND`, `dict_size(d) == 1`, and `"a"` is still retrievable.
16. **TC-16**
    - Description: Query operations on an empty dictionary.
    - Setup: `d = dict_create(0)`.
    - Action: `dict_get(d, "x")`, `dict_contains(d, "x")`, `dict_remove(d, "x")`.
    - Expected: `NULL`, `false`, and `DICT_ERR_NOT_FOUND`. `dict_size(d) == 0`.
17. **TC-17**
    - Description: Remove the same key twice.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`.
    - Action: `dict_remove(d, "a")` twice.
    - Expected: The first call returns `DICT_OK` and the second returns `DICT_ERR_NOT_FOUND`. `dict_size(d) == 0`, `dict_get(d, "a") == NULL`.
18. **TC-18**
    - Description: Insert with a NULL dictionary.
    - Setup: None.
    - Action: `dict_insert(NULL, "k", "v")`.
    - Expected: Returns `DICT_ERR_NULL_ARG` with no crash.
19. **TC-19**
    - Description: Insert with a NULL key or NULL value.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`.
    - Action: `dict_insert(d, NULL, "v")`, then `dict_insert(d, "k", NULL)`, then `dict_insert(d, "a", NULL)`.
    - Expected: All three return `DICT_ERR_NULL_ARG`. `dict_size(d) == 1`, `dict_contains(d, "k") == false`, and `dict_get(d, "a")` is still `"1"`.
20. **TC-20**
    - Description: Get and contains with NULL arguments.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`.
    - Action: `dict_get(NULL, "a")`, `dict_get(d, NULL)`, `dict_contains(NULL, "a")`, `dict_contains(d, NULL)`.
    - Expected: `NULL`, `NULL`, `false`, `false`, with no crash.
21. **TC-21**
    - Description: Remove with NULL arguments.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`.
    - Action: `dict_remove(NULL, "a")` and `dict_remove(d, NULL)`.
    - Expected: Both return `DICT_ERR_NULL_ARG`, and `dict_size(d) == 1`.
22. **TC-22**
    - Description: Size and bucket count with a NULL dictionary.
    - Setup: None.
    - Action: `dict_size(NULL)` and `dict_bucket_count(NULL)`.
    - Expected: Both return `0`.
23. **TC-23**
    - Description: Foreach with NULL arguments.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`. Use a callback that increments a counter.
    - Action: `dict_foreach(NULL, cb, &n)` and `dict_foreach(d, NULL, &n)`.
    - Expected: Both return `DICT_ERR_NULL_ARG`, and the callback is never invoked (`n == 0`).
24. **TC-24**
    - Description: The djb2 hash produces known values.
    - Setup: None.
    - Action: Call `dict_hash_djb2` on `""`, `"a"`, `"Aa"`, `"B@"`, and `NULL`.
    - Expected: `5381`, `177670`, `5862151`, `5862151`, and `5381`.
25. **TC-25**
    - Description: Confirm the collision keys share one chain.
    - Setup: None.
    - Action: Compute `dict_hash_djb2` for K1, K2, and K3.
    - Expected: All three hashes are equal.
26. **TC-26**
    - Description: Remove from the middle of a chain.
    - Setup: `d = dict_create(0)`. Insert `K1→"v1"`, `K2→"v2"`, `K3→"v3"` in that order, giving the chain K3 → K2 → K1.
    - Action: `dict_remove(d, K2)`.
    - Expected: Returns `DICT_OK` and `dict_size(d) == 2`. `dict_get(d, K2) == NULL`, K1 still returns `"v1"`, and K3 still returns `"v3"`. No leak.
27. **TC-27**
    - Description: Remove from the head of a chain.
    - Setup: Same as TC-26.
    - Action: `dict_remove(d, K3)`.
    - Expected: Returns `DICT_OK` and `dict_size(d) == 2`. K3 is absent, while K1 and K2 are still retrievable with their values.
28. **TC-28**
    - Description: Remove from the tail of a chain.
    - Setup: Same as TC-26.
    - Action: `dict_remove(d, K1)`.
    - Expected: Returns `DICT_OK` and `dict_size(d) == 2`. K1 is absent, while K2 and K3 are still retrievable with their values.
29. **TC-29**
    - Description: Empty a chain completely, then reuse it.
    - Setup: Same as TC-26.
    - Action: Remove K2, then K3, then K1. Then insert `K2→"again"`.
    - Expected: Each removal returns `DICT_OK`, and the size goes 2, 1, 0. The re-insert returns `DICT_OK`, `dict_get(d, K2)` is `"again"`, and `dict_size(d) == 1`.
30. **TC-30**
    - Description: Replace a value that is not at the head of a chain.
    - Setup: Same as TC-26.
    - Action: `dict_insert(d, K1, "new1")` (K1 is at the tail).
    - Expected: Returns `DICT_REPLACED` and `dict_size(d) == 3`. K1 returns `"new1"`, K2 returns `"v2"`, and K3 returns `"v3"`.
31. **TC-31**
    - Description: The resize threshold is exactly at load factor > 0.75.
    - Setup: `d = dict_create(0)`, which starts with 16 buckets.
    - Action: Insert 12 distinct keys and check the bucket count. Then insert a 13th distinct key and check again.
    - Expected: After 12 inserts, `dict_bucket_count(d) == 16` (12/16 = 0.75, not greater). After the 13th, `dict_bucket_count(d) == 32`. All 13 keys are retrievable and `dict_size(d) == 13`.
32. **TC-32**
    - Description: Replacing a value does not trigger a resize.
    - Setup: `d = dict_create(0)`, then insert 12 distinct keys.
    - Action: Replace the value of one of the existing keys.
    - Expected: Returns `DICT_REPLACED`, `dict_bucket_count(d) == 16`, and `dict_size(d) == 12`.
33. **TC-33**
    - Description: Resizing under many inserts.
    - Setup: `d = dict_create(0)`.
    - Action: Insert 100000 pairs `"key%d"→"val%d"`, for `i` from 0 to 99999.
    - Expected: Every insert returns `DICT_OK` and `dict_size(d) == 100000`. Every key returns its matching value. `dict_bucket_count(d) == 262144` (doubled from 16 until `size*4 <= buckets*3`). No leak after destroy.
34. **TC-34**
    - Description: `dict_get` pointers stay valid across a resize.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "anchor", "fixed")`, then `p = dict_get(d, "anchor")`.
    - Action: Insert 1000 other distinct keys, causing several resizes.
    - Expected: `p == dict_get(d, "anchor")` and `strcmp(p, "fixed") == 0`. No invalid reads reported.
35. **TC-35**
    - Description: Removal never shrinks the table.
    - Setup: `d = dict_create(0)`, insert 100 keys, and record `b = dict_bucket_count(d)`.
    - Action: Remove all 100 keys.
    - Expected: `dict_size(d) == 0`, `dict_bucket_count(d) == b`, and no leak.
36. **TC-36**
    - Description: Foreach visits each entry exactly once.
    - Setup: `d = dict_create(0)`, then insert 50 distinct pairs. Use a callback that records each visited key in a seen-set and checks that the value matches.
    - Action: `dict_foreach(d, cb, &state)`.
    - Expected: Returns `DICT_OK`. The callback runs exactly 50 times, every key is seen exactly once, and every value matches.
37. **TC-37**
    - Description: Foreach stops early when the callback returns non-zero.
    - Setup: `d = dict_create(0)` with 10 pairs. Use a callback that increments a counter and returns 1.
    - Action: `dict_foreach(d, cb, &n)`.
    - Expected: Returns `DICT_STOPPED` and `n == 1`.
38. **TC-38**
    - Description: Foreach on an empty dictionary.
    - Setup: `d = dict_create(0)` and a counting callback.
    - Action: `dict_foreach(d, cb, &n)`.
    - Expected: Returns `DICT_OK` and `n == 0`.
39. **TC-39**
    - Description: Foreach passes `user_data` through unchanged.
    - Setup: `d = dict_create(0)` with 3 pairs. Use a callback that asserts `user_data == &sentinel`.
    - Action: `dict_foreach(d, cb, &sentinel)`.
    - Expected: Returns `DICT_OK` and every assertion passes.
40. **TC-40**
    - Description: Allocation failure leaves the dictionary unchanged. This requires an interposed or fault-injecting allocator.
    - Setup: `d = dict_create(0)`, then `dict_insert(d, "a", "1")`. Arm the allocator so the next allocation fails.
    - Action: `dict_insert(d, "b", "2")`. Re-arm the allocator, then call `dict_insert(d, "a", "new")`.
    - Expected: Both calls return `DICT_ERR_NOMEM`. `dict_size(d) == 1`, `dict_contains(d, "b") == false`, and `dict_get(d, "a")` is still `"1"`. No leak after destroy.
41. **TC-41**
    - Description: A full lifecycle with no leaks.
    - Setup: `d = dict_create(0)`.
    - Action: Insert 10000 distinct pairs. Replace the values of all even-numbered keys. Remove every third key. Run a foreach over the remainder. Call `dict_destroy(d)`.
    - Expected: Sizes are consistent throughout (10000 after the inserts, still 10000 after the replacements, 6666 after the removals). Foreach visits exactly `dict_size(d)` entries. The leak checker reports 0 bytes definitely, indirectly, or possibly lost, and no invalid reads, writes, or frees.
