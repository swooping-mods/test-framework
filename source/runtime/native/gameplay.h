/* Small, source-pinned Windows/IL2CPP boundary. No external SDK is required. */
typedef void *PTR;
typedef unsigned int DWORD;
typedef unsigned long long U64;
typedef int BOOL;
__declspec(dllimport) PTR __stdcall GetProcAddress(PTR,const char *);
__declspec(dllimport) PTR __stdcall LoadLibraryA(const char *);
__declspec(dllimport) DWORD __stdcall GetEnvironmentVariableA(const char *,char *,DWORD);
__declspec(dllimport) DWORD __stdcall GetCurrentProcessId(void);
__declspec(dllimport) DWORD __stdcall GetCurrentThreadId(void);
__declspec(dllimport) PTR __stdcall GetCurrentProcess(void);
__declspec(dllimport) BOOL __stdcall VirtualProtect(PTR,U64,DWORD,DWORD *);
__declspec(dllimport) BOOL __stdcall FlushInstructionCache(PTR,const void *,U64);
__declspec(dllimport) PTR __stdcall CreateFileA(const char *,DWORD,DWORD,PTR,DWORD,DWORD,PTR);
__declspec(dllimport) BOOL __stdcall WriteFile(PTR,const void *,DWORD,DWORD *,PTR);
__declspec(dllimport) BOOL __stdcall FlushFileBuffers(PTR);
#include "hook_context.h"
__declspec(dllimport) PTR __stdcall AddVectoredExceptionHandler(DWORD,int (*)(ExceptionPointers *));
__declspec(dllimport) DWORD __stdcall RemoveVectoredExceptionHandler(PTR);

static char *cat(char *p,const char *s){while(*s)*p++=*s++;*p=0;return p;}
static char *num(char *p,U64 n){char t[24];int i=0;do{t[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)*p++=t[--i];*p=0;return p;}

typedef PTR (*Invoke)(PTR,PTR,void **,PTR *);
static Invoke invoke;
static PTR (*unbox)(PTR);
static PTR (*get_method)(PTR,const char *,int);
static PTR (*get_class)(PTR,const char *,const char *);
static PTR (*get_field)(PTR,const char *);
static void (*field_value)(PTR,PTR,void *);
static DWORD (*method_token)(PTR);
static PTR (*object_class)(PTR);
static PTR images[128];static U64 image_count;
static PTR find_class(const char *ns,const char *name){
    for(U64 i=0;i<image_count;i++){PTR k=get_class(images[i],ns,name);if(k)return k;}return 0;
}
static PTR method(const char *ns,const char *type,const char *name,int args){
    PTR k=find_class(ns,type);return k?get_method(k,name,args):0;
}
static PTR call(PTR m,PTR self,void **args,int *ok){
    if(!m){*ok=0;return 0;}PTR exception=0;PTR value=invoke(m,self,args,&exception);
    if(exception)*ok=0;return value;
}
static int integer(PTR m,PTR self,int *ok){PTR v=call(m,self,0,ok);if(!v){*ok=0;return 0;}return *(int *)unbox(v);}
static int boolean(PTR m,PTR self,int *ok){PTR v=call(m,self,0,ok);if(!v){*ok=0;return 0;}return *(unsigned char *)unbox(v)!=0;}

#define BIND(NAME,TYPE) NAME=(TYPE)GetProcAddress(game,"il2cpp_" #NAME);if(!NAME)return 0
static int bind_runtime(PTR domain,PTR game){
    invoke=(Invoke)GetProcAddress(game,"il2cpp_runtime_invoke");
    unbox=(PTR(*)(PTR))GetProcAddress(game,"il2cpp_object_unbox");
    get_method=(PTR(*)(PTR,const char *,int))GetProcAddress(game,"il2cpp_class_get_method_from_name");
    get_class=(PTR(*)(PTR,const char *,const char *))GetProcAddress(game,"il2cpp_class_from_name");
    get_field=(PTR(*)(PTR,const char *))GetProcAddress(game,"il2cpp_class_get_field_from_name");
    field_value=(void(*)(PTR,PTR,void *))GetProcAddress(game,"il2cpp_field_get_value");
    method_token=(DWORD(*)(PTR))GetProcAddress(game,"il2cpp_method_get_token");
    object_class=(PTR(*)(PTR))GetProcAddress(game,"il2cpp_object_get_class");
    const PTR *(*assemblies)(PTR,U64 *)=(const PTR *(*)(PTR,U64 *))GetProcAddress(game,"il2cpp_domain_get_assemblies");
    PTR (*image)(PTR)=(PTR(*)(PTR))GetProcAddress(game,"il2cpp_assembly_get_image");
    if(!invoke||!unbox||!get_method||!get_class||!get_field||!field_value||!method_token||!object_class||!assemblies||!image)return 0;
    U64 count=0;const PTR *a=assemblies(domain,&count);if(!a||count>128)return 0;
    image_count=count;for(U64 i=0;i<count;i++)images[i]=image(a[i]);return 1;
}

#include "hook_engine.h"
static int prepare_hook(PTR m,DWORD token,int id,PTR game){
    if(!m||method_token(m)!=token||hook_count>=8)return 0;
    /* The pinned runtime-invoke producer loads native methodPointer at +0.
     * This is a checked current-build fact, not a metadata-version assumption. */
    unsigned char *p=*(unsigned char **)m;
    if(p<(unsigned char *)game+0x1000||p>=(unsigned char *)game+31917844)return 0;
    int n=instruction_length(p);if(!n)return 0;
    for(int i=0;i<hook_count;i++)if(hooks[i].address==p)return 0;
    Hook *h=hooks+hook_count++;h->address=p;h->length=n;h->id=id;
    for(int i=0;i<5;i++)h->original[i]=p[i];return 1;
}

#include "../sdk/hooks.h"
/* New builds use the versioned framework. Old binary exports remain loader
 * compatibility shims; a standalone single-mod probe keeps local hooks. */
__declspec(dllexport) int MoonLab_SharedUpdateAbi(void){return 1;}
__declspec(dllexport) int MoonLab_HookFrameworkAbi(void){return 1;}
__declspec(dllimport) PTR __stdcall GetModuleHandleA(const char *);
static const MoonHookApiV1 *loader_hook_api(PTR loader){
    MoonGetHookApiV1 get_api=(MoonGetHookApiV1)GetProcAddress(loader,"MoonLab_GetHookApi_v1");
    const MoonHookApiV1 *api=get_api?get_api():0;
    return moon_hook_api_valid(api)?api:0;
}
#ifdef MOONLAB_LATE_UPDATE_ROLE
__declspec(dllexport) int MoonLab_LateUpdateAbi(void){return 1;}
#endif
static int install_hooks(void){
    PTR loader=GetModuleHandleA("version.dll");
    if(loader&&GetProcAddress(loader,"MoonLab_LoaderAbi")){
        const MoonHookApiV1 *api=loader_hook_api(loader);if(!api)return 0;
        PTR update=method("MoonCleaners","MoonWalker","Update",0);
        PTR address=update?*(PTR *)update:0;
        for(int i=0;i<hook_count;i++)if(hooks[i].address==address){
            MoonHookSubscriptionV1 spec={sizeof(spec),MOON_HOOK_UPDATE,MOON_ROLE_OBSERVER,MOON_PHASE_NORMAL,0,hooks[i].id,before_hook,0};
            if(!api->subscribe(&spec))return 0;
            for(int j=i+1;j<hook_count;j++)hooks[j-1]=hooks[j];
            hook_count--;break;
        }
#ifdef MOONLAB_LATE_UPDATE_ROLE
        PTR late=method("MoonCleaners","MoonWalker","LateUpdate",0);
        PTR late_address=late?*(PTR *)late:0;
        for(int i=0;i<hook_count;i++)if(hooks[i].address==late_address){
            MoonHookSubscriptionV1 spec={sizeof(spec),MOON_HOOK_LATE_UPDATE,MOONLAB_LATE_UPDATE_ROLE,MOON_PHASE_NORMAL,0,hooks[i].id,before_hook,0};
            if(!api->subscribe(&spec))return 0;
            for(int j=i+1;j<hook_count;j++)hooks[j-1]=hooks[j];
            hook_count--;break;
        }
#endif
    }
    return !hook_count||install_local_hooks();
}
