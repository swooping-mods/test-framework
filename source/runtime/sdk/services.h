#ifndef MOONLAB_RUNTIME_SERVICES_V1_H
#define MOONLAB_RUNTIME_SERVICES_V1_H
/* Loader-owned, process-lifetime services. Domain API tables remain opaque.
 * Publication names/limits are borrowed for the call and checked against the
 * package contract; API tables and callback code remain resident. */
#include "hooks.h"
#define MOON_RUNTIME_SERVICES_ABI 1u
/* Separate from the frozen API table. READY is published only after all native
 * owners commit and hook installation succeeds; it does not admit domain calls
 * on another thread or make a borrowed game object valid. */
#define MOON_RUNTIME_PENDING 0u
#define MOON_RUNTIME_READY 1u
#define MOON_RUNTIME_FAILED 2u
typedef unsigned int (*MoonRuntimeStateV1)(void);
typedef struct {
    const char *name;
    unsigned int value;
} MoonServiceLimitV1;
typedef struct {
    unsigned int size;
    const char *service_id;
    unsigned int abi,api_size;
    const void *api;
} MoonServicePublicationV1;
typedef struct {
    MoonServicePublicationV1 base;
    unsigned int limit_count, reserved;
    const MoonServiceLimitV1 *limits;
} MoonServicePublicationLimitsV1;
typedef void (*MoonFrameCallback)(void *walker,void *user);
typedef struct {
    unsigned int size,phase;
    int priority;
    MoonFrameCallback callback;
    void *user;
} MoonFrameSubscriptionV1;
typedef struct {
    unsigned int size,abi;
    /* Publish during the provider's ModStart. Visibility begins on success.
     * API storage and all functions must remain resident until process exit. */
    /* Base size 32, or capacity extension size 48 with reserved zero. */
    int (*publish)(const MoonServicePublicationV1 *);
    /* Exact ABI and minimum-size prefix match against a successfully started provider's ID.
     * A manifest declaration alone does not make a service available. */
    const void *(*find)(const char *provider_id,const char *service_id,unsigned int abi,unsigned int api_size);
    /* Frames share the hook observer ordering and rollback rules. The walker
     * is borrowed for this call; validate authority/world before using it. */
    unsigned int (*subscribe_frame)(const MoonFrameSubscriptionV1 *);
    int (*unsubscribe_frame)(unsigned int);
} MoonRuntimeApiV1;
typedef const MoonRuntimeApiV1 *(*MoonGetRuntimeApiV1)(void);
static inline int moon_runtime_api_valid(const MoonRuntimeApiV1 *api){
    return api&&api->size>=sizeof(*api)&&api->abi==MOON_RUNTIME_SERVICES_ABI
        &&api->publish&&api->find&&api->subscribe_frame&&api->unsubscribe_frame;
}
_Static_assert(sizeof(MoonServicePublicationV1)==32,"service publication ABI v1");
_Static_assert(sizeof(MoonServicePublicationLimitsV1)==48,"service capacities ABI v1");
_Static_assert(sizeof(MoonServiceLimitV1)==16,"service limit ABI v1");
_Static_assert(sizeof(MoonFrameSubscriptionV1)==32,"frame subscription ABI v1");
_Static_assert(sizeof(MoonRuntimeApiV1)==40,"runtime service ABI v1");
#endif
