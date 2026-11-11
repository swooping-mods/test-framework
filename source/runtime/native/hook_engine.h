typedef struct {unsigned char *address,original[5];int length,id;} Hook;
static Hook hooks[8];static int hook_count;
static int (*before_hook)(int,Context *);
/* Supported first instructions have no relative operands or flag changes.
 * A one-byte INT3 write is atomic; no thread can see a torn jump instruction.
 * The handler emulates precisely that original instruction and resumes in the
 * original function/unwind region. There is no relocated trampoline. */
static int instruction_length(const unsigned char *p){
    /* MoonDepotBuilder.MachineBuilt saves RSI and MoonMapScreen.Steer saves
     * RDX in pinned stack home slots. Emulate only these exact store forms. */
    if(p[0]==0x48 && p[1]==0x89 && (p[2]==0x4c||p[2]==0x54||p[2]==0x5c||p[2]==0x6c||p[2]==0x74||p[2]==0x7c) && p[3]==0x24 && (p[4]==0x08||p[4]==0x10||p[4]==0x18||p[4]==0x20))return 5;
    if(p[0]==0x40 && p[1]>=0x53 && p[1]<=0x57 && p[1]!=0x54)return 2;
    if(p[0]==0x48 && p[1]==0x8b && p[2]==0xc4)return 3;
    return 0;
}
static U64 saved_register(Context *c,int reg){
    switch(reg){case 1:return c->rcx;case 2:return c->rdx;case 3:return c->rbx;case 5:return c->rbp;case 6:return c->rsi;default:return c->rdi;}
}
static int exception_handler(ExceptionPointers *e){
    if(e->record->code!=0x80000003)return 0;
    for(int i=0;i<hook_count;i++){
        Hook *h=hooks+i;if(e->record->address!=(PTR)h->address)continue;
        Context *c=e->context;
        if(before_hook(h->id,c)){
            c->rip=*(U64 *)c->rsp;c->rsp+=8;return -1;
        }
        unsigned char *p=h->original;
        if(h->length==5)*(U64 *)(c->rsp+p[4])=saved_register(c,(p[2]>>3)&7);
        else if(h->length==2){c->rsp-=8;*(U64 *)c->rsp=saved_register(c,p[1]-0x50);}
        else c->rax=c->rsp;
        c->rip=(U64)h->address+(U64)h->length;return -1;
    }
    return 0;
}
static int install_local_hooks(void){
    PTR handler=AddVectoredExceptionHandler(1,exception_handler);if(!handler)return 0;
    int installed=0,complete=1;
    for(int i=0;i<hook_count;i++){
        DWORD old=0;Hook *h=hooks+i;
        if(!VirtualProtect(h->address,1,0x40,&old)){complete=0;break;}
        *(volatile unsigned char *)h->address=0xcc;installed++;
        DWORD ignored=0;
        if(!FlushInstructionCache(GetCurrentProcess(),h->address,1)||!VirtualProtect(h->address,1,old,&ignored)){complete=0;break;}
    }
    if(complete&&installed==hook_count)return 1;
    int restored=1;
    for(int i=0;i<installed;i++){
        DWORD old=0,ignored=0;Hook *h=hooks+i;
        if(!VirtualProtect(h->address,1,0x40,&old)){restored=0;continue;}
        *(volatile unsigned char *)h->address=h->original[0];
        FlushInstructionCache(GetCurrentProcess(),h->address,1);VirtualProtect(h->address,1,old,&ignored);
    }
    if(restored)RemoveVectoredExceptionHandler(handler);
    /* If restoration fails the handler/DLL remain resident until process exit. */
    return 0;
}
