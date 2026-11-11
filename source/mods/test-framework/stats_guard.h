/* Test-process isolation at Steamworks' flat user-stat mutation boundary.
 * Reads, authentication, ownership and matchmaking are untouched. No real
 * mutation function is called to test this guard; use the inert PE diagnostic. */
#ifndef MOON_FAST_STATS_GUARD_H
#define MOON_FAST_STATS_GUARD_H
#define MF_STATS_COUNT 11
static const char *mf_stats_names[MF_STATS_COUNT]={
    "SteamAPI_ISteamUserStats_SetAchievement",
    "SteamAPI_ISteamUserStats_ClearAchievement",
    "SteamAPI_ISteamUserStats_SetStatInt32",
    "SteamAPI_ISteamUserStats_SetStatFloat",
    "SteamAPI_ISteamUserStats_UpdateAvgRateStat",
    "SteamAPI_ISteamUserStats_StoreStats",
    "SteamAPI_ISteamUserStats_ResetAllStats",
    "SteamAPI_ISteamUserStats_IndicateAchievementProgress",
    "SteamAPI_ISteamUserStats_UploadLeaderboardScore",
    "SteamAPI_ISteamUserStats_AttachLeaderboardUGC",
    "SteamAPI_ISteamUserStats_FindOrCreateLeaderboard"
};
static struct {unsigned char *address,original;int patched;} mf_stats_sites[MF_STATS_COUNT];
static PTR mf_stats_handler;
static unsigned mf_stats_count,mf_stats_active;
static unsigned long long mf_stats_rejected;
#ifndef MF_STATS_PROTECT
#define MF_STATS_PROTECT VirtualProtect
#endif
static int mf_stats_executable(PTR module,PTR address){
    unsigned char *base=module;if(!base||*(unsigned short *)base!=0x5a4d)return 0;
    DWORD offset=*(DWORD *)(base+0x3c);if(offset<0x40||offset>0x1000)return 0;
    unsigned char *nt=base+offset;
    if(*(DWORD *)nt!=0x4550||*(unsigned short *)(nt+4)!=0x8664||*(unsigned short *)(nt+24)!=0x20b)return 0;
    DWORD size=*(DWORD *)(nt+80);unsigned sections=*(unsigned short *)(nt+6),optional=*(unsigned short *)(nt+20);
    if(size<0x1000||size>0x20000000||sections<1||sections>96||optional<112||optional>512)return 0;
    U64 rva=(U64)address-(U64)base;if(rva>=size)return 0;
    unsigned char *section=nt+24+optional;
    if((U64)(section-base)+40*sections>size)return 0;
    for(unsigned i=0;i<sections;i++,section+=40){
        DWORD start=*(DWORD *)(section+12),span=*(DWORD *)(section+8),raw=*(DWORD *)(section+16);
        if(raw>span)span=raw;
        if(rva>=start&&rva-(U64)start<span&&(*(DWORD *)(section+36)&0x20000000))return 1;
    }
    return 0;
}
static int mf_stats_exception(ExceptionPointers *exception){
    if(exception->record->code!=0x80000003)return 0;
    for(unsigned i=0;i<mf_stats_count;i++)if(exception->record->address==(PTR)mf_stats_sites[i].address){
        Context *context=exception->context;
        __atomic_add_fetch(&mf_stats_rejected,1,__ATOMIC_RELAXED);
        /* Every guarded export returns bool or SteamAPICall_t. Zero is failure.
         * Return to the caller without executing any original API instruction. */
        context->rax=0;context->rip=*(U64 *)context->rsp;context->rsp+=8;return -1;
    }
    return 0;
}
static int mf_stats_restore(void){
    int ok=1;mf_stats_active=0;
    for(unsigned i=0;i<mf_stats_count;i++)if(mf_stats_sites[i].patched){
        DWORD old=0,ignored=0;
        if(!MF_STATS_PROTECT(mf_stats_sites[i].address,1,0x40,&old)){ok=0;continue;}
        *(volatile unsigned char *)mf_stats_sites[i].address=mf_stats_sites[i].original;
        mf_stats_sites[i].patched=0;
        if(!FlushInstructionCache(GetCurrentProcess(),mf_stats_sites[i].address,1)||
           !MF_STATS_PROTECT(mf_stats_sites[i].address,1,old,&ignored))ok=0;
    }
    if(ok&&mf_stats_handler){
        if(!RemoveVectoredExceptionHandler(mf_stats_handler))return 0;
        mf_stats_handler=0;mf_stats_count=0;
    }
    return ok;
}
static int mf_stats_install(PTR module){
    if(mf_stats_active)return 1;
    if(!module||mf_stats_handler)return 0;
    mf_stats_count=0;mf_stats_rejected=0;
    for(unsigned i=0;i<MF_STATS_COUNT;i++){
        unsigned char *address=GetProcAddress(module,mf_stats_names[i]);
        if(!mf_stats_executable(module,address)||*address==0xcc){mf_stats_count=0;return 0;}
        for(unsigned j=0;j<i;j++)if(address==mf_stats_sites[j].address){mf_stats_count=0;return 0;}
        mf_stats_sites[i].address=address;mf_stats_sites[i].original=*address;mf_stats_sites[i].patched=0;
        mf_stats_count++;
    }
    mf_stats_handler=AddVectoredExceptionHandler(1,mf_stats_exception);
    if(!mf_stats_handler){mf_stats_count=0;return 0;}
    for(unsigned i=0;i<mf_stats_count;i++){
        DWORD old=0,ignored=0;
        if(!MF_STATS_PROTECT(mf_stats_sites[i].address,1,0x40,&old))goto failed;
        /* Like the production hook dispatcher, the entry change is an atomic
         * one-byte INT3. No thread can observe a partially written jump. */
        *(volatile unsigned char *)mf_stats_sites[i].address=0xcc;mf_stats_sites[i].patched=1;
        if(!FlushInstructionCache(GetCurrentProcess(),mf_stats_sites[i].address,1)||
           !MF_STATS_PROTECT(mf_stats_sites[i].address,1,old,&ignored))goto failed;
    }
    mf_stats_active=1;return 1;
failed:
    (void)mf_stats_restore();return 0;
}
#endif
