/* Explicit private-run automation. Normal loader launches leave this disabled. */
#include "../../runtime/native/gameplay.h"
#include "../../runtime/sdk/native_adapters/reflection.h"
#include "../../runtime/native/services.h"
#include "core.h"
static void mf_stats_note(unsigned,U64);
#define MF_STATS_NOTE mf_stats_note
#include "stats_guard.h"
int _fltused = 0;
static MFRunner mf;
static PTR mf_log, mf_alive_method, mf_restoring, mf_focused, mf_world_field;
static PTR mf_stats_log;
static unsigned mf_stats_noted;
static PTR mf_joined, mf_host_id, mf_local_id, mf_path, mf_quit;
static PTR mf_menu_blocked;
static PTR mf_steam_initialized;
static PTR mf_game;
static PTR mf_menu_start, mf_menu_slot, mf_menu_privacy, mf_menu_play, mf_slot_used;
static PTR mf_menu_chosen, mf_menu_private, mf_menu_kit;
static DWORD mf_thread;
static U64 mf_frame, mf_sequence, mf_quit_frame;
static U64 mf_wait_logged;
static int mf_boot, mf_boot_stage, mf_boot_frames, mf_ready_frames, mf_invite;
static U64 (*mf_clock)(void);
static int (*mf_string_length)(PTR);
static const unsigned short *(*mf_string_chars)(PTR);
static char mf_run[96], mf_expected_profile[1024], mf_selection[12288];
static void mf_stats_note(unsigned index,U64 count){
    unsigned bit=1u<<index,seen=__atomic_fetch_or(&mf_stats_noted,bit,__ATOMIC_RELAXED);
    if(!mf_stats_log||((seen&bit)&&count%65536))return;
    char buffer[512],*end=cat(buffer,"{\"api\":\"");end=cat(end,mf_stats_names[index]);
    end=cat(end,"\",\"count\":");end=num(end,count);end=cat(end,",\"thread\":");end=num(end,GetCurrentThreadId());end=cat(end,"}\n");
    DWORD written=0;WriteFile(mf_stats_log,buffer,(DWORD)(end-buffer),&written,0);FlushFileBuffers(mf_stats_log);
}

static char *mf_quote(char *out, const char *value, unsigned limit) {
    *out++ = '"';
    for (unsigned i = 0; value && value[i] && i < limit; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '"' || c == '\\') *out++ = '\\';
        if (c < 32 || c > 126) c = '?';
        *out++ = (char)c;
    }
    *out++ = '"'; *out = 0; return out;
}
static char *mf_signed(char *out, long long value) {
    if (value < 0) { *out++ = '-'; return num(out, (U64)(-(value + 1)) + 1); }
    return num(out, (U64)value);
}
static char *mf_prefix(char *out, const char *event) {
    out = cat(out, "{\"run_id\":"); out = mf_quote(out, mf_run, 95);
    out = cat(out, ",\"sequence\":"); out = num(out, ++mf_sequence);
    out = cat(out, ",\"event\":"); return mf_quote(out, event, 80);
}
static void mf_write(char *buffer, char *end) {
    end = cat(end, "}\n"); DWORD written = 0;
    if (!WriteFile(mf_log, buffer, (DWORD)(end - buffer), &written, 0) || written != (DWORD)(end - buffer)) mf.tainted = 1;
    FlushFileBuffers(mf_log);
}
static void mf_event(const char *event) {
    char buffer[512], *end = mf_prefix(buffer, event); mf_write(buffer, end);
}
static int mf_isolate_stats(void){
    if(mf_stats_active)return 1;
    /* Wait for the real initialized context. Interface thunks can be aliased
     * across Steam interfaces, so rejection is scoped to this exact receiver. */
    int ok=1;
    if(!mf_steam_initialized||!boolean(mf_steam_initialized,0,&ok)||!ok)return 0;
    PTR getter=dmethod("Steamworks","CSteamAPIContext","GetSteamUserStats","");
    PTR (*return_type)(PTR)=(PTR(*)(PTR))GetProcAddress(mf_game,"il2cpp_method_get_return_type");
    if(!getter||!return_type){mf_event("steam-stats-binding-missing");return -1;}
    char *type=da.type_name(return_type(getter));
    int valid=type&&deq(type,"System.IntPtr");if(type)da.free(type);
    if(!valid){mf_event("steam-stats-return-type-rejected");return -1;}
    PTR value=call(getter,0,0,&ok),receiver=0;
    if(ok&&value)dcopy(&receiver,unbox(value),sizeof(receiver));
    if(!ok||!receiver){mf_event("steam-stats-receiver-missing");return -1;}
    PTR sdk=GetModuleHandleA("steam_api64.dll");
    if(!sdk||!mf_stats_install(sdk,receiver)){mf_event("steam-stats-isolation-failed");return -1;}
    mf_event("steam-stats-isolated");return 1;
}
static void mf_report(void *user, const char *event, const MoonTestCase *test,
                      int status, const char *reason, unsigned assertions, int cleanup_ok) {
    (void)user;
    char buffer[1400], *end = mf_prefix(buffer, event);
    const char *states[] = {"pending", "pass", "fail", "blocked"};
    end = cat(end, ",\"status\":"); end = mf_quote(end, states[status], 16);
    if (test) { end = cat(end, ",\"case\":"); end = mf_quote(end, test->id, 127); }
    if (reason) { end = cat(end, ",\"reason\":"); end = mf_quote(end, reason, 200); }
    end = cat(end, ",\"assertions\":"); end = num(end, assertions);
    end = cat(end, ",\"cleanup_ok\":"); end = cat(end, cleanup_ok ? "true" : "false");
    if (!test) {
        end = cat(end, ",\"count\":"); end = num(end, mf.finished); mf_quit_frame = mf_frame + 3;
        end=cat(end,",\"steam_stats_isolated\":");end=cat(end,mf_stats_active?"true":"false");
        end=cat(end,",\"steam_mutations_blocked\":");end=num(end,__atomic_load_n(&mf_stats_rejected,__ATOMIC_RELAXED));
        end=cat(end,",\"steam_unrelated_calls_forwarded\":");end=num(end,__atomic_load_n(&mf_stats_forwarded,__ATOMIC_RELAXED));
        end=cat(end,",\"steam_stats_receiver_scoped\":true");
    }
    mf_write(buffer, end);
}
static int mf_assert(const char *label, int passed, long long actual, long long expected) {
    if (!mf.active || !mf.in_callback || !label) return 0;
    char buffer[1200], *end = mf_prefix(buffer, "assertion");
    end = cat(end, ",\"case\":"); end = mf_quote(end, mf.selected[mf.index]->id, 127);
    end = cat(end, ",\"label\":"); end = mf_quote(end, label, 127);
    end = cat(end, ",\"passed\":"); end = cat(end, passed ? "true" : "false");
    end = cat(end, ",\"actual\":"); end = mf_signed(end, actual);
    end = cat(end, ",\"expected\":"); end = mf_signed(end, expected);
    mf_write(buffer, end);
    return mf_check(&mf, passed);
}
static int mf_registration(const MoonTestCase *tests, unsigned count) { return mf_register(&mf, tests, count); }
static const MoonTesting mf_api = {MF_ABI, sizeof(MoonTesting), mf_registration, mf_assert};
static unsigned mf_isolation_status(void){return mf_stats_active?MF_ISOLATE_STEAM_STATS:0;}
static unsigned long long mf_isolation_count(void){return __atomic_load_n(&mf_stats_rejected,__ATOMIC_RELAXED);}
static const MoonTestIsolation mf_isolation={MF_ABI,sizeof(MoonTestIsolation),mf_isolation_status,mf_isolation_count};

static int mf_alive(PTR object) {
    int ok = 1; void *args[] = {object};
    PTR value = object ? call(mf_alive_method, 0, args, &ok) : 0;
    return ok && value && *(unsigned char *)unbox(value);
}
static int mf_host(void) {
    int ok = 1;
    if (!boolean(mf_joined, 0, &ok)) return ok;
    PTR host = call(mf_host_id, 0, 0, &ok), local = call(mf_local_id, 0, 0, &ok);
    U64 a = 0, b = 0;
    if (host) dcopy(&a, unbox(host), 8);
    if (local) dcopy(&b, unbox(local), 8);
    return ok && a && a == b;
}
static int mf_profile_matches(void) {
    int ok = 1; PTR path = call(mf_path, 0, 0, &ok);
    if (!ok || !path) return 0;
    int length = mf_string_length(path);
    if (length <= 0 || length >= 1024) return 0;
    const unsigned short *chars = mf_string_chars(path);
    for (int i = 0; i < length; i++) {
        unsigned c = chars[i], expected = (unsigned char)mf_expected_profile[i];
        if (c == '/') c = '\\';
        if (expected == '/') expected = '\\';
        if (c > 127 || c != expected) return 0;
    }
    return !mf_expected_profile[length];
}
static int mf_seal(void) {
    const char *ids[MF_LIMIT]; unsigned count = 0;
    char *at = mf_selection;
    while (*at) {
        if (count == MF_LIMIT) return 0;
        ids[count++] = at;
        while (*at && *at != ',') at++;
        if (*at) { *at++ = 0; if (!*at) return 0; }
    }
    return mf_select(&mf, ids, count);
}
static void mf_update(void *walker, void *user) {
    (void)user;
    if (mf_thread && mf_thread != GetCurrentThreadId()) return;
    mf_thread = GetCurrentThreadId(); mf_frame++;
    if(!mf_stats_active&&mf_isolate_stats()!=1)return;
    if (mf_quit_frame && mf_frame >= mf_quit_frame) {
        int ok = 1; call(mf_quit, 0, 0, &ok); mf_quit_frame = 0; return;
    }
    if (mf.complete || !mf_alive(walker)) return;
    PTR world = 0; field_value(walker, mf_world_field, &world);
    int ok = 1;
    unsigned flags = mf_alive(world) && !boolean(mf_restoring, world, &ok) && ok ? MF_READY : 0;
    if (mf_host()) flags |= MF_HOST;
    if (boolean(mf_focused, 0, &ok) && ok) flags |= MF_FOCUSED;
    if (!mf.sealed) {
        int input_ok=1;
        int menu_blocked=(flags&MF_READY)?boolean(mf_menu_blocked,walker,&input_ok):1;
        U64 now=mf_clock();
        if(now-mf_wait_logged>=1000){
            mf_wait_logged=now;char buffer[700],*end=mf_prefix(buffer,"world-wait");
            end=cat(end,",\"flags\":");end=num(end,flags);end=cat(end,",\"frame\":");end=num(end,mf_frame);
            end=cat(end,",\"menu_blocked\":");end=num(end,(U64)menu_blocked);
            end=cat(end,",\"steam_mutations_blocked\":");end=num(end,__atomic_load_n(&mf_stats_rejected,__ATOMIC_RELAXED));
        end=cat(end,",\"steam_unrelated_calls_forwarded\":");end=num(end,__atomic_load_n(&mf_stats_forwarded,__ATOMIC_RELAXED));
        end=cat(end,",\"steam_stats_receiver_scoped\":true");mf_write(buffer,end);
        }
        if ((flags & (MF_READY | MF_HOST | MF_FOCUSED)) != (MF_READY | MF_HOST | MF_FOCUSED)||menu_blocked||!input_ok) { mf_ready_frames = 0; return; }
        if (++mf_ready_frames < 5) return;
        if (!mf_profile_matches()) { mf_event("profile-mismatch"); mf.complete = 1; mf_quit_frame = mf_frame + 3; return; }
        if (!mf_seal()) { mf_event("selection-rejected"); mf.complete = 1; mf_quit_frame = mf_frame + 3; return; }
        mf_event("world-ready");
    }
    MoonTestContext context = {mf_clock(), mf_frame, world, walker, flags, 0};
    mf_tick(&mf, &context);
}
static int mf_menu(int id, Context *context) {
    (void)id;
    if(mf.complete)return 0;
    if (mf_thread && mf_thread != GetCurrentThreadId()) return 0;
    mf_thread = GetCurrentThreadId();
    int isolated=mf_isolate_stats();
    if(isolated!=1){
        if(isolated<0){int quit_ok=1;mf.complete=1;call(mf_quit,0,0,&quit_ok);}
        return 0;
    }
    if(!mf_boot||mf_boot_stage>=4)return 0;
    if (++mf_boot_frames < 8) return 0;
    PTR menu = (PTR)context->rcx, kit = 0;
    field_value(menu, mf_menu_kit, &kit);
    if (!kit) return 0;
    int ok = 1;
    if (!mf_profile_matches()) { mf_event("profile-mismatch"); mf_boot_stage = 4; return 0; }
    if (mf_boot_stage == 0) {
        int slot = 1; void *args[] = {&slot}; PTR used = call(mf_slot_used, 0, args, &ok);
        if (!ok || !used || !*(unsigned char *)unbox(used)) { mf_event("prepared-slot-missing"); mf_boot_stage = 4; return 0; }
        mf_boot_stage = 1; call(mf_menu_start, menu, 0, &ok); mf_event(ok ? "direct-start" : "direct-start-failed");
    } else if (mf_boot_stage == 1) {
        mf_boot_stage = 2;
        int slot = 1; void *args[] = {&slot}; call(mf_menu_slot, menu, args, &ok);
        args[0] = &mf_invite; call(mf_menu_privacy, menu, args, &ok);
        int selected = 0, privacy = 0;
        field_value(menu, mf_menu_chosen, &selected); field_value(menu, mf_menu_private, &privacy);
        if (!ok || selected != 1 || privacy != mf_invite) { mf_event("direct-selection-failed"); mf_boot_stage = 4; }
    } else if (mf_boot_stage == 2) {
        mf_boot_stage = 3; call(mf_menu_play, menu, 0, &ok); mf_event(ok ? "direct-play" : "direct-play-failed");
    }
    return 0;
}
static int mf_env(const char *name, char *out, DWORD capacity) {
    DWORD length = GetEnvironmentVariableA(name, out, capacity);
    return length && length < capacity;
}
__declspec(dllexport) const MoonTesting *MoonLab_Testing_v1(unsigned abi, unsigned size) {
    return abi == MF_ABI && size == sizeof(mf_api) ? &mf_api : 0;
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain, PTR game) {
    (void)integer;
    char enabled[8], folder[1000], path[1100];
    if (!mf_env("MOONLAB_FAST_TEST", enabled, sizeof(enabled)) || !deq(enabled, "1") ||
        !mf_env("MOONLAB_RUN_ID", mf_run, sizeof(mf_run)) || !mf_name(mf_run, 0) ||
        !mf_env("MOONLAB_EVIDENCE", folder, sizeof(folder)) ||
        !mf_env("MOONLAB_FAST_PROFILE", mf_expected_profile, sizeof(mf_expected_profile)) ||
        !mf_env("MOONLAB_FAST_CASES", mf_selection, sizeof(mf_selection))) return 0;
    cat(cat(path, folder), "\\fast-tests.jsonl");
    mf_log = CreateFileA(path, 0x40000000, 1, 0, 1, 0x80, 0);
    if (mf_log == (PTR)(U64)-1 || !bind_runtime(domain, game) || !dapi(game)) return 0;
    mf_game=game;
    cat(cat(path,folder),"\\fast-stats.jsonl");mf_stats_log=CreateFileA(path,0x40000000,1,0,1,0x80,0);
    if(mf_stats_log==(PTR)(U64)-1)return 0;
    int isolated=mf_isolate_stats();if(isolated<0)return 0;
    if(!isolated)mf_event("steam-stats-isolation-pending");
    mf_clock = (U64 (*)(void))GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetTickCount64");
    mf_string_length = (int (*)(PTR))GetProcAddress(game, "il2cpp_string_length");
    mf_string_chars = (const unsigned short *(*)(PTR))GetProcAddress(game, "il2cpp_string_chars");
#define MF_BIND(to, ns, type, name, signature) to = dmethod(ns, type, name, signature); if (!to) { mf_event("binding-" #to); return 0; }
    MF_BIND(mf_alive_method, "UnityEngine", "Object", "op_Implicit", "UnityEngine.Object");
    MF_BIND(mf_restoring, "MoonCleaners", "MoonWorld", "get_IsRestoringRendering", "");
    MF_BIND(mf_focused, "UnityEngine", "Application", "get_isFocused", "");
    MF_BIND(mf_path, "UnityEngine", "Application", "get_persistentDataPath", "");
    MF_BIND(mf_quit, "UnityEngine", "Application", "Quit", "");
    MF_BIND(mf_joined, "MoonCleaners", "MoonCrew", "get_Joined", "");
    MF_BIND(mf_host_id, "MoonCleaners", "MoonCrew", "get_HostId", "");
    MF_BIND(mf_local_id, "Steamworks", "SteamUser", "GetSteamID", "");
    MF_BIND(mf_steam_initialized,"MoonCleaners","MoonSteam","get_Initialized","");
    MF_BIND(mf_menu_blocked,"MoonCleaners","MoonWalker","get_MenuBlocked","");
    mf_world_field = get_field(find_class("MoonCleaners", "MoonWalker"), "world");
    if (!mf_clock || !mf_string_length || !mf_string_chars || !mf_world_field) return 0;
    mf.report = mf_report;
    if (!moon_runtime_subscribe_frame(mf_update, 0)) return 0;
    mf_boot = mf_env("MOONLAB_FAST_BOOT", enabled, sizeof(enabled)) && deq(enabled, "1");
    if (mf_boot) {
        MF_BIND(mf_menu_start, "MoonCleaners", "MoonMainMenu", "StartGame", "");
        MF_BIND(mf_menu_slot, "MoonCleaners", "MoonMainMenu", "ClickSlot", "System.Int32");
        MF_BIND(mf_menu_play, "MoonCleaners", "MoonMainMenu", "Play", "");
        MF_BIND(mf_slot_used, "MoonCleaners", "MoonSaveSlots", "Used", "System.Int32");
        mf_menu_privacy = method("MoonCleaners", "MoonMainMenu", "ChoosePrivacy", 1);
        if (!mf_menu_privacy || method_token(mf_menu_privacy) != 100668126) return 0;
        /* The enum is resolved from the actual parameter type. No prior-build
         * privacy constant or argument layout is copied into the caller. */
        PTR (*type_class)(PTR) = (PTR (*)(PTR))GetProcAddress(game, "il2cpp_class_from_type");
        int (*value_size)(PTR, DWORD *) = (int (*)(PTR, DWORD *))GetProcAddress(game, "il2cpp_class_value_size");
        void (*static_get)(PTR, void *) = (void (*)(PTR, void *))GetProcAddress(game, "il2cpp_field_static_get_value");
        PTR privacy = type_class ? type_class(da.param(mf_menu_privacy, 0)) : 0;
        PTR invite = privacy ? get_field(privacy, "InviteOnly") : 0; DWORD align = 0;
        if (!value_size || !static_get || !invite || value_size(privacy, &align) != 4) return 0;
        static_get(invite, &mf_invite);
        PTR menu = find_class("MoonCleaners", "MoonMainMenu");
        mf_menu_chosen = get_field(menu, "chosenSlot"); mf_menu_private = get_field(menu, "chosenPrivacy"); mf_menu_kit = get_field(menu, "kit");
        if (!mf_menu_chosen || !mf_menu_private || !mf_menu_kit) return 0;
    }
    /* Every private run guards the native menu before a world starts, including
     * manual starts. Only the optional boot flag invokes menu operations. */
    if(!prepare_hook(dmethod("MoonCleaners","MoonMainMenu","Animate",""),100668108,0,game))return 0;
    before_hook=mf_menu;if(!install_hooks())return 0;
#undef MF_BIND
    const MoonRuntimeApiV1 *runtime = moon_runtime_connect();
    const MoonServiceLimitV1 limits[] = {{"max_cases", MF_LIMIT}};
    MoonServicePublicationLimitsV1 publication = {
        .base = {sizeof(publication), "testing", MF_ABI, sizeof(mf_api), &mf_api},
        .limit_count = 1, .reserved = 0, .limits = limits};
    if (!runtime || !runtime->publish(&publication.base)) return 0;
    MoonServicePublicationV1 isolation_publication={sizeof(isolation_publication),"isolation",MF_ABI,sizeof(mf_isolation),&mf_isolation};
    if(!runtime->publish(&isolation_publication))return 0;
    mf_event("ready");
    return 1;
}
