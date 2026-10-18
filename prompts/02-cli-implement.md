You are implementing a C library against a fixed specification.

Step 0: Read spec.md. It contains sections starting with "## FILE: <name>".
Write each section's content, exactly as given, to a file with that name.
Then run `gcc -fsyntax-only dict.h` and stop if it fails.

Produce exactly three files:

1. dict.c
   - Implement every function declared in dict.h, matching the documented
     behavior, ownership rules, and error handling exactly.
   - Hash table with separate chaining (singly linked lists per bucket).
   - djb2 hash; double the bucket count when load factor exceeds 0.75.
   - Copy keys and values on insert (use malloc + memcpy, not strdup,
     for C99 portability); free everything on remove/destroy.
   - Check every malloc result. No global state.

2. test_dict.c
   - One test function per case in test_cases.md, named test_<ID>.
   - No external test framework. Use a small CHECK(cond) macro that
     records failures without aborting.
   - Print one line per test: "PASS <ID>" or "FAIL <ID>: <reason>".
   - End with "SUMMARY: <passed>/<total> passed".
   - Exit with status 0 if all pass, 1 otherwise.

3. Makefile
   - CC ?= gcc, CFLAGS = -std=c99 -Wall -Wextra -Werror -g
   - Targets: all (builds test_dict), test (builds and runs it),
     memcheck (runs it under valgrind --leak-check=full
     --error-exitcode=2), clean.

Do not add features beyond the spec. If the spec is ambiguous, pick the
safest interpretation and note it in a comment starting with "SPEC NOTE:".

After creating the files:
1. Run `make test`. If any test fails, fix dict.c and rerun.
2. Run `make memcheck`. If Valgrind reports leaks or errors, fix dict.c and rerun.
3. Repeat until both pass. Never modify dict.h or weaken test_dict.c
   to make tests pass; only fix the implementation.
4. Finish by printing the final output of `make test` and `make memcheck`.
