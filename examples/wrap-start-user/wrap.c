#include <unistd.h>

/* This file includes some code that our link-time build logic
 * arranges to run very early during ld.so startup.
 *
 * Here are some notes about glibc's ld.so startup sequence, which
 * is quite convoluted.
 *
 * The raw ELF entry point is _start. But note also what comes after it.

0000000000001180 <_start>:
    1180:       48 89 e7                mov    %rsp,%rdi
    1183:       e8 08 0e 00 00          callq  1f90 <_dl_start>
0000000000001188 <_dl_start_user>:
    1188:       49 89 c4                mov    %rax,%r12
    118b:       8b 05 07 76 02 00       mov    0x27607(%rip),%eax        # 28798 <_dl_skip_args>
    1191:       5a                      pop    %rdx
    1192:       48 8d 24 c4             lea    (%rsp,%rax,8),%rsp
    1196:       29 c2                   sub    %eax,%edx
    1198:       52                      push   %rdx
    1199:       48 89 d6                mov    %rdx,%rsi
    119c:       49 89 e5                mov    %rsp,%r13
    119f:       48 83 e4 f0             and    $0xfffffffffffffff0,%rsp
    11a3:       48 8b 3d 76 7e 02 00    mov    0x27e76(%rip),%rdi        # 29020 <_rtld_global>
    11aa:       49 8d 4c d5 10          lea    0x10(%r13,%rdx,8),%rcx
    11af:       49 8d 55 08             lea    0x8(%r13),%rdx
    11b3:       31 ed                   xor    %ebp,%ebp
    11b5:       e8 86 f5 00 00          callq  10740 <_dl_init>
    11ba:       48 8d 15 ef f8 00 00    lea    0xf8ef(%rip),%rdx        # 10ab0 <_dl_fini>
    11c1:       4c 89 ec                mov    %r13,%rsp
    11c4:       41 ff e4                jmpq   *%r12
    11c7:       66 0f 1f 84 00 00 00    nopw   0x0(%rax,%rax,1)
    11ce:       00 00 

 * In other words, _dl_start *returns* and control is suddenly in _dl_start_user.
 * That eventually calls _dl_init and does an indirect jump somewhere else.
 * Which one actually passes control to the user program? (Spoiler: it's the indirect jump.
 * But constructors will be run first, by _dl_init.)
 *
 * They all come from dl-machine.h in sysdeps/x86_64/dl-machine.h (et al), which
 * (in 2.27) is annotated like so.
 #define RTLD_START asm ("\n\
.text\n\
        .align 16\n\
.globl _start\n\
.globl _dl_start_user\n\
_start:\n\
        movq %rsp, %rdi\n\
        call _dl_start\n\
_dl_start_user:\n\
        # Save the user entry point address in %r12.\n\
        movq %rax, %r12\n\
        # See if we were run as a command with the executable file\n\
        # name as an extra leading argument.\n\
        movl _dl_skip_args(%rip), %eax\n\
        # Pop the original argument count.\n\
        popq %rdx\n\
        # Adjust the stack pointer to skip _dl_skip_args words.\n\
        leaq (%rsp,%rax,8), %rsp\n\
        # Subtract _dl_skip_args from argc.\n\
        subl %eax, %edx\n\
        # Push argc back on the stack.\n\
        pushq %rdx\n\
        # Call _dl_init (struct link_map *main_map, int argc, char **argv, char **env)\n\
        # argc -> rsi\n\
        movq %rdx, %rsi\n\
        # Save %rsp value in %r13.\n\
        movq %rsp, %r13\n\
        # And align stack for the _dl_init call. \n\
        andq $-16, %rsp\n\
        # _dl_loaded -> rdi\n\
        movq _rtld_local(%rip), %rdi\n\
        # env -> rcx\n\
        leaq 16(%r13,%rdx,8), %rcx\n\
        # argv -> rdx\n\
        leaq 8(%r13), %rdx\n\
        # Clear %rbp to mark outermost frame obviously even for constructors.\n\
        xorl %ebp, %ebp\n\
        # Call the function to run the initializers.\n\
        call _dl_init\n\
        # Pass our finalizer function to the user in %rdx, as per ELF ABI.\n\
        leaq _dl_fini(%rip), %rdx\n\
        # And make sure %rsp points to argc stored on the stack.\n\
        movq %r13, %rsp\n\
        # Jump to the user's entry point.\n\
        jmp *%r12\n\
.previous\n\
");

 * Important points here are that:
 * _dl_start returns the user's entry point address
 * _dl_init is called and then returns. In fact it's called
 *      multiple times, once per "worker"... not ideal.
 * _dl_fini is passed to the program entry point, in %rdx.
 *
 * Which of these symbols are wrappable with --wrap?
 * _dl_start is defined in the same object
 *       so we'd have to do "unbind"
 * _dl_init is defined in a different object, so seems fair game.
 * BUT actually --wrap doesn't work for any of them -- since the
 * glibc build combines everything into the .os file first, after
 * which there are no UND references. Even if we unbind, unbinding
 * ignores section-internal references, and all calls are now
 * .text-internal. We avoid this by using the replacement technique,
 * with -z muldefs (see my forthcoming blog post...).
 */
/* Don't do this, now that we can wrap _dl_start. */
#if 0
struct link_map;
void __orig__dl_init (struct link_map *main_map, int argc, char **argv, char **env);
void __wrap__dl_init (struct link_map *main_map, int argc, char **argv, char **env)
{
	const char msg[] = "wrapped init!\n";
	write(2, msg, sizeof msg);
	__orig__dl_init(main_map, argc, argv, env);
}
#endif
/* What about _dl_start? Where is it defined? In rtld.c. But it's
 * static so normally there is no relocation record generated. 
 * However, adding the -ffunction-sections option into the build of
 * rtld.os ensures that the necessary relocation record is available
 * at the call from _start to _dl_start. This is not a dynamic
 * reloc -- it goes away during the ordinary static link, so it's
 * not too intrusive to force its creation here. Note that when
 * _dl_start is called, we haven't done bootstrap relocation yet,
 * so we call the real _dl_start *first*. Once it returns, we can
 * do what we like. */
unsigned long long /* ElfW(Addr) */ __orig__dl_start(void *arg);
unsigned long long /* ElfW(Addr) */ __wrap__dl_start(void *arg)
{
	unsigned long long ret = __orig__dl_start(arg);
	const char msg[] = "wrapped start!\n";
	write(2, msg, sizeof msg);
	return ret;
}
