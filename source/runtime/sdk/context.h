#ifndef MOONLAB_HOOK_CONTEXT_V1_H
#define MOONLAB_HOOK_CONTEXT_V1_H
/* Only the documented AMD64 prefix through RIP is accessed. */
typedef struct {
    unsigned long long home[6];unsigned int flags,mxcsr;unsigned short segments[6];unsigned int eflags;
    unsigned long long debug[6],rax,rcx,rdx,rbx,rsp,rbp,rsi,rdi,r8,r9,r10,r11,r12,r13,r14,r15,rip;
} MoonHookContext;
_Static_assert(__builtin_offsetof(MoonHookContext,rip)==248,"AMD64 CONTEXT layout");
#endif
