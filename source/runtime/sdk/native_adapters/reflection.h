/* Shared first-party reflection subset, compiled separately into each mod.
 * Persistent references use pointer-sized roots; scene calls use the Unity thread. */
#ifndef MOON_NATIVE_REFLECTION_H
#define MOON_NATIVE_REFLECTION_H
typedef U64 DRoot;
typedef struct {float x,y,z;} DVec;
typedef struct {float x,y,w,h;} DRect;
typedef struct {float r,g,b,a;} DColor;
static struct {
    PTR (*methods)(PTR,PTR *),(*param)(PTR,DWORD),(*new_object)(PTR),(*string)(const char *);
    const char *(*name)(PTR);DWORD (*arity)(PTR);char *(*type_name)(PTR);void (*free)(PTR);
    PTR (*class_type)(PTR),(*type_object)(PTR),(*target)(DRoot);
    DRoot (*hold)(PTR,BOOL);void (*drop)(DRoot);const char *failed;
    U64 (*array_length)(PTR);
} da;
static inline int deq(const char *a,const char *b){if(!a||!b)return a==b;while(*a&&*a==*b){a++;b++;}return *a==*b;}
static inline void dzero(void *to,U64 n){unsigned char *p=to;while(n--)*p++=0;}
static inline void dcopy(void *to,const void *from,U64 n){unsigned char *d=to;const unsigned char *s=from;while(n--)*d++=*s++;}
static inline PTR dmethod(const char *ns,const char *type,const char *name,const char *signature){
    PTR k=find_class(ns,type),iterator=0,m,found=0;if(!k)return 0;
    while((m=da.methods(k,&iterator))){
        if(!deq(da.name(m),name))continue;const char *s=signature;DWORD count=da.arity(m);int match=1;
        for(DWORD i=0;i<count;i++){
            char *type_name=da.type_name(da.param(m,i));const char *t=type_name;
            if(!t){match=0;break;}
            while(*t&&*s&&*s!=';'&&*t==*s){t++;s++;}
            if(*t||(*s&&*s!=';'))match=0;da.free(type_name);
            if(*s==';')s++;else if(i+1<count)match=0;if(!match)break;
        }
        if(*s)match=0;if(match){if(found)return 0;found=m;}
    }return found;
}
static inline PTR dtype(const char *ns,const char *name){PTR k=find_class(ns,name);return k?da.type_object(da.class_type(k)):0;}
static inline DRoot dhold(PTR p){return p?da.hold(p,0):0;}
static inline PTR dget(DRoot r){return r?da.target(r):0;}
static inline void ddrop(DRoot *r){if(*r)da.drop(*r);*r=0;}
static inline float dnumber(PTR m,PTR self,int *ok){float f=0;PTR v=call(m,self,0,ok);if(v)dcopy(&f,unbox(v),sizeof(f));else *ok=0;return f;}
static inline DVec dvector(PTR m,PTR self,int *ok){DVec v={0};PTR p=call(m,self,0,ok);if(p)dcopy(&v,unbox(p),sizeof(v));else *ok=0;return v;}
static inline int dapi(PTR game){
#define DA(F,N,T) da.F=(T)GetProcAddress(game,"il2cpp_" N);if(!da.F){da.failed="il2cpp_" N;return 0;}
    DA(methods,"class_get_methods",PTR(*)(PTR,PTR *));DA(param,"method_get_param",PTR(*)(PTR,DWORD));
    DA(name,"method_get_name",const char *(*)(PTR));DA(arity,"method_get_param_count",DWORD(*)(PTR));
    DA(type_name,"type_get_name",char *(*)(PTR));DA(free,"free",void(*)(PTR));
    DA(new_object,"object_new",PTR(*)(PTR));DA(string,"string_new",PTR(*)(const char *));
    DA(class_type,"class_get_type",PTR(*)(PTR));DA(type_object,"type_get_object",PTR(*)(PTR));
    DA(hold,"gchandle_new",DRoot(*)(PTR,BOOL));DA(target,"gchandle_get_target",PTR(*)(DRoot));DA(drop,"gchandle_free",void(*)(DRoot));
    DA(array_length,"array_length",U64(*)(PTR));
#undef DA
    return 1;
}
static inline PTR darray(PTR array,U64 index){return array&&index<da.array_length(array)?(unsigned char *)array+32+8*index:0;}
#endif
