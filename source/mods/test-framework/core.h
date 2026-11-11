#ifndef MOON_FAST_TEST_CORE_H
#define MOON_FAST_TEST_CORE_H
#include "sdk/testing.h"

typedef void (*MFReport)(void *user, const char *event, const MoonTestCase *test,
                         int status, const char *reason, unsigned assertions,
                         int cleanup_ok);
typedef struct {
    const MoonTestCase *cases[MF_LIMIT], *selected[MF_LIMIT];
    unsigned count, selected_count, index, assertions, failures, blocked, finished;
    unsigned long long started_ms, last_ms;
    void *world;
    int sealed, active, failed_assertion, in_callback, complete, tainted;
    MFReport report;
    void *report_user;
} MFRunner;

static inline int mf_equal(const char *a, const char *b) {
    if (!a || !b) return a == b;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static inline int mf_name(const char *name, int dot) {
    if (!name || !*name) return 0;
    unsigned length = 0;
    for (; *name; name++) {
        if (++length > 127 || !((*name >= 'a' && *name <= 'z') ||
              (*name >= '0' && *name <= '9') || *name == '-' || (dot && *name == '.'))) return 0;
    }
    return 1;
}
static inline int mf_owned_id(const char *owner, const char *id) {
    if (!mf_name(owner, 0) || !mf_name(id, 1)) return 0;
    while (*owner) if (*owner++ != *id++) return 0;
    return *id++ == '.' && *id;
}
static inline int mf_register(MFRunner *runner, const MoonTestCase *cases, unsigned count) {
    if (!runner || runner->sealed || !cases || !count || count > MF_LIMIT - runner->count) return 0;
    /* Validate the entire contribution before publishing any of it. */
    for (unsigned i = 0; i < count; i++) {
        const MoonTestCase *test = cases + i;
        if (test->abi != MF_ABI || test->size != sizeof(*test) || !test->step ||
            !test->cleanup || !mf_owned_id(test->owner, test->id) ||
            test->timeout_ms < 1 || test->timeout_ms > 120000 ||
            (test->requirements & ~(MF_READY | MF_HOST | MF_FOCUSED))) return 0;
        for (unsigned j = 0; j < runner->count; j++)
            if (mf_equal(test->id, runner->cases[j]->id)) return 0;
        for (unsigned j = 0; j < i; j++) if (mf_equal(test->id, cases[j].id)) return 0;
    }
    for (unsigned i = 0; i < count; i++) runner->cases[runner->count++] = cases + i;
    return 1;
}
static inline int mf_select(MFRunner *runner, const char *const *ids, unsigned count) {
    if (!runner || runner->sealed || !ids || !count || count > MF_LIMIT) return 0;
    const MoonTestCase *selected[MF_LIMIT];
    for (unsigned i = 0; i < count; i++) {
        selected[i] = 0;
        for (unsigned j = 0; j < i; j++) if (mf_equal(ids[i], ids[j])) return 0;
        for (unsigned j = 0; j < runner->count; j++)
            if (mf_equal(ids[i], runner->cases[j]->id)) selected[i] = runner->cases[j];
        if (!selected[i]) return 0;
    }
    for (unsigned i = 0; i < count; i++) runner->selected[i] = selected[i];
    runner->selected_count = count;
    runner->sealed = 1;
    return 1;
}
static inline int mf_check(MFRunner *runner, int passed) {
    if (!runner || !runner->active || !runner->in_callback) return 0;
    runner->assertions++;
    if (!passed) runner->failed_assertion = 1;
    return passed != 0;
}
static inline void mf_finish(MFRunner *runner, const MoonTestContext *context,
                             int status, const char *reason) {
    const MoonTestCase *test = runner->selected[runner->index];
    int cleanup_ok = 1;
    if (runner->active) {
        runner->in_callback = 1;
        cleanup_ok = test->cleanup(context, test->user) != 0;
        runner->in_callback = 0;
    }
    if (!cleanup_ok) { status = MF_FAIL; reason = "cleanup-failed"; runner->tainted = 1; }
    if (runner->failed_assertion) { status = MF_FAIL; reason = "assertion-failed"; }
    if (status == MF_PASS && !runner->assertions) { status = MF_FAIL; reason = "missing-observation"; }
    if (status == MF_FAIL) runner->failures++;
    if (status == MF_BLOCKED) runner->blocked++;
    if (runner->report) runner->report(runner->report_user, "case-finished", test,
                                     status, reason, runner->assertions, cleanup_ok);
    runner->finished++;
    runner->index++;
    runner->active = 0;
    runner->assertions = 0;
    runner->failed_assertion = 0;
    if (runner->index == runner->selected_count) {
        runner->complete = 1;
        if (runner->report) runner->report(runner->report_user, "batch-finished", 0,
                                         runner->failures ? MF_FAIL : runner->blocked ? MF_BLOCKED : MF_PASS, 0, 0, 1);
    }
}
static inline void mf_tick(MFRunner *runner, const MoonTestContext *context) {
    if (!runner || !context || !runner->sealed || runner->complete || runner->in_callback) return;
    const MoonTestCase *test = runner->selected[runner->index];
    if (runner->tainted) { mf_finish(runner, context, MF_BLOCKED, "previous-cleanup-failed"); return; }
    if (runner->active && (context->now_ms < runner->last_ms || context->world != runner->world)) {
        mf_finish(runner, context, MF_FAIL, "clock-or-world-changed"); return;
    }
    runner->last_ms = context->now_ms;
    if ((context->flags & test->requirements) != test->requirements) {
        if (runner->active) mf_finish(runner, context, MF_FAIL, "eligibility-lost");
        /* First-world readiness is the launcher's responsibility. A case that
         * needs a special fixture is blocked, not a success or a busy loop. */
        else mf_finish(runner, context, MF_BLOCKED, "missing-prerequisite");
        return;
    }
    const char *reason = 0;
    if (!runner->active) {
        runner->active = 1;
        runner->world = context->world;
        runner->started_ms = context->now_ms;
        if (runner->report) runner->report(runner->report_user, "case-started", test, MF_PENDING, 0, 0, 1);
        if (test->setup) {
            runner->in_callback = 1;
            int state = test->setup(context, test->user, &reason);
            runner->in_callback = 0;
            if (state != MF_PENDING) mf_finish(runner, context, state >= MF_PASS && state <= MF_BLOCKED ? state : MF_FAIL, reason);
            return;
        }
    }
    if (context->now_ms - runner->started_ms >= test->timeout_ms) {
        mf_finish(runner, context, MF_FAIL, "deadline"); return;
    }
    runner->in_callback = 1;
    int status = test->step(context, test->user, &reason);
    runner->in_callback = 0;
    if (status != MF_PENDING) mf_finish(runner, context, status >= MF_PASS && status <= MF_BLOCKED ? status : MF_FAIL, reason);
}
#endif
