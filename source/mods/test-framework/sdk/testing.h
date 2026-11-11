#ifndef MOON_FAST_TESTING_V1_H
#define MOON_FAST_TESTING_V1_H
/* Test-only service. Pointers and callbacks remain resident; contexts borrow
 * Unity objects for one main-thread callback and must not be retained raw. */
#define MF_ABI 1u
#define MF_LIMIT 96u
enum { MF_PENDING = 0, MF_PASS = 1, MF_FAIL = 2, MF_BLOCKED = 3 };
enum { MF_READY = 1u, MF_HOST = 2u, MF_FOCUSED = 4u };
typedef struct {
    unsigned long long now_ms, frame;
    void *world, *walker;
    unsigned flags, reserved;
} MoonTestContext;
typedef int (*MoonTestStep)(const MoonTestContext *, void *, const char **reason);
typedef int (*MoonTestCleanup)(const MoonTestContext *, void *);
typedef struct {
    unsigned abi, size;
    const char *id, *owner;
    unsigned timeout_ms, requirements;
    MoonTestStep setup, step;
    MoonTestCleanup cleanup;
    void *user;
} MoonTestCase;
typedef struct {
    unsigned abi, size;
    int (*register_cases)(const MoonTestCase *, unsigned count);
    int (*check)(const char *label, int passed, long long actual, long long expected);
} MoonTesting;
typedef const MoonTesting *(*MoonTestingGet)(unsigned abi, unsigned size);
enum { MF_ISOLATE_STEAM_STATS=1u };
typedef struct {
    unsigned abi,size;
    unsigned (*status)(void);
    unsigned long long (*blocked_calls)(void);
} MoonTestIsolation;
#endif
