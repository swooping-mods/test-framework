#ifndef MOONLAB_SERVICES_CLIENT_H
#define MOONLAB_SERVICES_CLIENT_H
/* Include in the owning bootstrap translation unit after its Win32
 * declarations: this header defines the DLL's runtime ABI marker. Do not load
 * a dependency from this connector or recurse into another mod's ModStart. */
#include "services.h"
static inline int moon_runtime_loader_present(void){
    PTR loader=GetModuleHandleA("version.dll");
    return loader&&GetProcAddress(loader,"MoonLab_LoaderAbi")!=0;
}
static inline const MoonRuntimeApiV1 *moon_runtime_connect(void){
    PTR loader=GetModuleHandleA("version.dll");
    if(!loader||!GetProcAddress(loader,"MoonLab_LoaderAbi"))return 0;
    MoonGetRuntimeApiV1 get=(MoonGetRuntimeApiV1)GetProcAddress(loader,"MoonLab_GetRuntimeApi_v1");
    const MoonRuntimeApiV1 *api=get?get():0;
    return moon_runtime_api_valid(api)?api:0;
}
static inline unsigned int moon_runtime_subscribe_frame(MoonFrameCallback callback,void *user){
    const MoonRuntimeApiV1 *api=moon_runtime_connect();if(!api)return 0;
    MoonFrameSubscriptionV1 spec={sizeof(spec),MOON_PHASE_NORMAL,0,callback,user};
    return api->subscribe_frame(&spec);
}
__declspec(dllexport) int MoonLab_RuntimeServicesAbi(void){return MOON_RUNTIME_SERVICES_ABI;}
#endif
