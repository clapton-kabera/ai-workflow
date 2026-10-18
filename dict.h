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
