#ifndef MOONLAB_HOOKS_V1_H
#define MOONLAB_HOOKS_V1_H
/* Process-lifetime AMD64 C ABI. No Unity headers or external SDK required. */
#include "context.h"
#include "vehicle_input.h"

enum MoonHook { MOON_HOOK_UPDATE=1, MOON_HOOK_LATE_UPDATE=2, MOON_HOOK_VEHICLE_FIXED=3 };
enum MoonHookRole { MOON_ROLE_OBSERVER=1, MOON_ROLE_OVERRIDE=2, MOON_ROLE_INPUT=3, MOON_ROLE_PHYSICS=4 };
/* These are ordered stages BEFORE the original native method, not postfixes.
 * Camera ownership precedes all LateUpdate observers; vehicle input always
 * precedes physics. Stages/priority order observers within those boundaries. */
enum MoonHookPhase { MOON_PHASE_EARLY=0, MOON_PHASE_NORMAL=1, MOON_PHASE_LATE=2 };
typedef int (*MoonHookCallback)(int,MoonHookContext *);
typedef int (*MoonHookVehicleCallback)(int,MoonHookContext *,MoonVehicleInput *);
typedef struct {
    unsigned int size,hook,role,phase;
    int priority,local_id;
    MoonHookCallback callback;
    MoonHookVehicleCallback vehicle_callback;
} MoonHookSubscriptionV1;
typedef struct {
    unsigned int size,abi;
    /* Zero means rejection. A handle belongs to the mod currently starting.
     * Removal is allowed only during that same ModStart, before publication. */
    unsigned int (*subscribe)(const MoonHookSubscriptionV1 *);
    int (*unsubscribe)(unsigned int);
    int (*camera_override_active)(void);
} MoonHookApiV1;
typedef const MoonHookApiV1 *(*MoonGetHookApiV1)(void);
static inline int moon_hook_api_valid(const MoonHookApiV1 *api){
    return api&&api->size==sizeof(*api)&&api->abi==1&&api->subscribe&&api->unsubscribe&&api->camera_override_active;
}
_Static_assert(sizeof(MoonHookSubscriptionV1)==40,"hook subscription ABI v1");
_Static_assert(sizeof(MoonHookApiV1)==32,"hook API ABI v1");
#endif
