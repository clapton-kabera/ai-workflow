You are a C library designer. Produce a specification for a string-to-string
dictionary implemented as a hash table with separate chaining (linked lists).

Requirements:
- Keys and values are null-terminated C strings.
- The dictionary stores its own copies of keys and values and frees them.
- Inserting an existing key replaces its value.
- Resize (double buckets) when load factor exceeds 0.75.
- Use the djb2 hash function.
- Operations: create, destroy, insert, get, remove, contains, size,
  and an iterator (or a foreach with a callback).
- Define behavior for NULL arguments and missing keys.

Output exactly two sections, nothing else:

## FILE: dict.h
A complete, compilable C header with include guards, an opaque Dict type,
all function prototypes, and a doc comment above each function stating
parameters, return value, ownership, and error behavior.

## FILE: test-cases.md
A numbered list of test cases. Each case has: ID, description, setup steps,
action, and expected result. Cover normal use, duplicates, missing keys,
NULL inputs, removal from the middle/head/tail of a chain, resizing with
many inserts, and full cleanup with no leaks.


