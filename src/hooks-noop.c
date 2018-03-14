#include <unistd.h>
#include <stdlib.h>
#include <elf.h>
#include <link.h>

/* On glibc's ld.so (at least 2.24), _start is a text symbol
 * that calls _dl_start. At the return site, the subsequent
 * instructions are under the symbol _dl_start_user. So,
 * attempting to wrap _dl_start_user doesn't work, because
 * it is never called.
 * See glibc-2.24/sysdeps/i386/dl-machine.h
 *
 * We can't --wrap _dl_start because it is generated in raw
 * asm. Even -ffunction-sections won't get us a relocation
 * record at the call site.
 *
 * So what's a robust way to get control early in ld.so's
 * execution? Maybe a constructor?
 *
 * Other ideas:
 *
 * 1. investigate the ELF .init / _init convention properly.
 * 2. abuse __gmon_start__? make sure we don't break it
 *
 * On both of those, note that stock ld.so doesn't have
 * .init, and doesn't mention __gmon_start__ at all.
 * Since it's up to the libc to run these constructors
 * (I THINK... I'm pretty sure that e.g. in a static-linked case,
 * the kernel wouldn't bundle extra logic to run .init first),
 * these are no good.
 *
 * We can add an "-init _dl_srk_init" option to the link command
 * which will name us by DT_INIT.
 */

//void *__real__dl_start(void *arg); /* HACK: guess signature */
//void *__wrap__dl_start(void *arg)
//{
static void init(void) __attribute__((constructor));
static void init(void)
{
//	void *ret = __real__dl_start(arg);
	const char str[] = "Hello from very early in dynamic linking!\n";
	write(2, str, sizeof str);
//	return ret;
}

static void early_init(void) __attribute__((used,section(".init")));
static void (__attribute__((used,section(".init"))) early_init) (void)
{
//	void *ret = __real__dl_start(arg);
	const char str[] = "Hello from REALLY early in dynamic linking!\n";
	write(2, str, sizeof str);
//	return ret;
}

/* This only gets called slightly earlier, when the init logic
 * runs the DT_INIT entry rather than DT_INIT_ARRAY. */
void _dl_srk_init(void);
void _dl_srk_init(void)
{
	const char str[] = "Hello from some time early in dynamic linking!\n";
	write(2, str, sizeof str);
}

/* IDEA: wrap a different symbol in the ld.so. Maybe dl_main? */
//void __def_dl_main(const ElfW(Phdr) *phdr, ElfW(Word) phnum,
//                    ElfW(Addr) *user_entry, ElfW(auxv_t) *auxv);
//void __wrap___ref_dl_main(const ElfW(Phdr) *phdr, ElfW(Word) phnum,
//                     ElfW(Addr) *user_entry, ElfW(auxv_t) *auxv)
//{
//	const char str[] = "Hello from very early in dynamic linking!\n";
//	write(2, str, sizeof str);
//	__def_dl_main(phdr, phnum, user_entry, auxv);
//}
