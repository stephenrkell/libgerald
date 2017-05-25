#include <unistd.h>
#include <string.h>
#include <alloca.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <elf.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <dlfcn.h>
#include <err.h>
#include <limits.h>

/* glibc functions we can't use in this file, and how to work around it:
 * 
 * 
 * sysconf -- use auxv
 * snprintf -- format it ourself
 * statvfs -- 
 * warnx -- 
 * dlsym -- 
 * abort
 * strncmp -- hmm, #error "strncmp not implemented so far"
 *  -- hmm, we only use strcmp so this shouldn't happen.
 *     How does rtld-strncmp get into rtld-modules?
 *     seems to be from what builds libc/build/elf/librtld.mk
 *     which gets its input from    libc/build/elf/librtld.map
 *       ... which contains strncmp! from getenv
 *            BUT ld.so seems to do getenv
 * getenv -- write our own
 * strndup -- write our own
 * strncpy -- write our own
 * errno -- hmm, should be taken care of by IS_IN_rtld
 * 
 */

char *my_getenv(const char *name) { return NULL; }

char *my_strncpy(char *dst, const char *src, size_t len)
{
	char *orig_dst = dst;
	char *end = dst + len;
	while (dst < end && '\0' != (*dst++ = *src++));
	return orig_dst;
}

void my_abort(void) __attribute__((noreturn));
void my_abort(void) { while(1); /* FIXME */ }

/* In this HACKy, RACEy proof-of-concept, we preload the exec functions so that 
 * 
 * 1. If exec'ing something dynamically linked (the common case), 
      and an interpreter is not specified on the command line,
 *    we set ourselves to be( the interpreter (and remove ourselves from LD_PRELOAD).
 * 2. If exec'ing something setuid, we do something. FIXME.
 *    One possible big-hammer solution is to make ourselves, as loader,
 *    setuid. Although we will have dropped permissions by the time we 
 *    reach the exec, the new instance of the loader can notice that it's
 *    loading something setuid, and retain the higher privilege. 
 *    Or it could even do something clever to elevate permissions on
 *    a per-library basis, via sandboxing perhaps.
 * 3. If exec'ing something statically linked, we emulate the loader:
 *    walk its phdrs, map its LOADs and transfer control. (FIXME)
 * 
 * We also preload syscall, similarly.
 * 
 * Of course, using preload to catch execs is unsound, because
 * programs can do system calls directly in assembly. A proper solution
 * to this would do trap-syscalls thing.
 */

const char *interpreter_to_use = /* FIXME */ "/lib64/ld-linux-x86-64.so.2";
extern char **environ;
//static void rewrite_ld_preload(const char *in, char *out, size_t sz)
//{
//	memcpy(out, in, sz); // FIXME
//}
struct hash_bang_word
{
	char *chars;
	size_t len;
};

static _Bool is_directory(int fd, struct stat *st, void *data);
static _Bool is_regular_file(int fd, struct stat *st, void *data);
static _Bool is_noexec_fs(int fd, struct stat *st, void *data);
static _Bool is_empty_file(int fd, struct stat *st, void *data);
static _Bool is_elf_with_multiple_interps(int fd, struct stat *st, void *data);
static _Bool is_hash_bang_file(int fd, struct stat *st, void *data, struct hash_bang_word *out_hash_bang_words);
static _Bool is_elf_static_executable(int fd, struct stat *st, void *data);
static _Bool is_elf_dynamic_executable(int fd, struct stat *st, void *data);
static _Bool is_elf_interpreter(int fd, struct stat *st, void *data);
static _Bool is_elf_shared_library(int fd, struct stat *st, void *data);
static _Bool is_executable(int fd, struct stat *st, void *data);

/* Danger of races: anything in the fs might change between the prediction
 * and the actuality. There's no way for our interpreter to return an error
 * that will abort the execve; execve has by definition succeeded when our
 * interpreter runs. 
 * 
 * To avoid filesystem races, we pass a file descriptor to the interpreter
 * so that it sees the same file we did. That only covers the first hop
 * in a possibly many-hop chain of interpreters; all but the first is "not our code".
 * HOWEVER I think that's fine because hash-bang (or whatever) has the same problem.
 * So if we can pass an fd to our interpreter and use a magic filename to mean "exec
 * from this fd" -- /proc/<pid>/fd should do -- then we're sorted. 
 */
 
#define ROUND_UP_TO_PAGE_SIZE(sz, pgsz) (((sz) % (pgsz) == 0) ? (sz) : ((pgsz) * (1 + (sz) / (pgsz))))

static int
predict_exec_return_value(const char *filename, char **argv, int *out_fd)
{
	int fd = open(filename, O_RDONLY);
	if (fd == -1) return errno;

	struct stat st;
	int ret = fstat(fd, &st);
	if (ret != 0) { close(fd); return errno; }
	
	unsigned long page_size =  4096;  //sysconf(_SC_PAGE_SIZE);
	void *data = mmap(NULL, ROUND_UP_TO_PAGE_SIZE(st.st_size, page_size), PROT_READ, MAP_PRIVATE, fd, 0);
	
	if (data == MAP_FAILED) { close(fd); return EINVAL; }
	
	int to_return = 0;
	/* Some failures cases we don't have to worry about. 
	 * These are the ones where 
	 * if exec'ing /path/to/exe will fail,
	 * exec'ing /lib/ld.so /path/to/exe will *also* fail
	 * (i.e. *fail to exec* -- cf. exec but then exit with error).
	 * I've marked these ALSOFAIL.
	 * We can pretend no failure will happen.
	 * 
	 * The ones we care about are where exec'ing the binary won't work,
	 * but exec'ing the interpreter *will* appear work (then fail later).
	 * I've marked these LATEFAIL.
	 */
	/* Linux (execve(2) man page as of 2014/10) lists the following reasons
	 * for failure. 
	 
       E2BIG  The total number of bytes in the environment (envp) and argument list  (argv)
              is too large.

	 -- ALSOFAIL */ /*

       EACCES Search  permission is denied on a component of the path prefix of filename or
              the name of a script interpreter.  (See also path_resolution(7).)

	 -- LATEFAIL */ 
	
	// CHECK search permission all the way to the filename.
	/* If it's a hash-bang file, our caller is going to recursively expand
	 * the interpreters, so we only need to check the file itself. */
	
	 /*

       EACCES The file or a script interpreter is not a regular file.

	 -- LATEFAIL */ 
	  /*

       EISDIR An ELF interpreter was a directory.

	 -- LATEFAIL */
	
	if (is_directory(fd, &st, data)) { to_return = EISDIR; goto out; }
	if (!is_regular_file(fd, &st, data)) { to_return = EACCES; goto out; }
	 
	 /*

       EACCES Execute permission is denied for the file or a script or ELF interpreter.

	 -- LATEFAIL */
	if (!is_executable(fd, &st, data)) { to_return = EACCES; goto out; }
	 
	  /*

       EACCES The filesystem is mounted noexec.

	 -- LATEFAIL */ 
	if (is_noexec_fs(fd, &st, data)) { to_return = EACCES; goto out; }

	 /*

       EAGAIN (since Linux 3.1)
              Having changed its real UID using one of the set*uid() calls, the caller was?
              and  is  now  still?above its RLIMIT_NPROC resource limit (see setrlimit(2)).
              For a more detailed explanation of this error, see NOTES.

	 -- ALSOFAIL */ /*

       EFAULT filename or one of the pointers in the vectors argv or  envp  points  outside
              your accessible address space.

	 -- ALSOFAIL */ /*

       EINVAL An  ELF  executable  had more than one PT_INTERP segment (i.e., tried to name
              more than one interpreter).

	 -- LATEFAIL */
	if (is_elf_with_multiple_interps(fd, &st, data)) { to_return = EINVAL; goto out; }
	 
	  /*

       EIO    An I/O error occurred.

	 -- LATEFAIL */
	 // HMM -- not much we can do here
	 
/*

       ELIBBAD
              An ELF interpreter was not in a recognized format.

	 -- LATEFAIL */ /*

       ELOOP  Too many symbolic links were encountered in resolving filename or the name of
              a script or ELF interpreter.

	 -- LATEFAIL */ /*

       EMFILE The process has the maximum number of files open.

	 -- ALSOFAIL */ /*

       ENAMETOOLONG
              filename is too long.

	 -- LATEFAIL */ /*

       ENFILE The system limit on the total number of open files has been reached.

	 -- ALSOFAIL */ /*

       ENOENT The  file filename or a script or ELF interpreter does not exist, or a shared
              library needed for file or interpreter cannot be found.

	 -- LATEFAIL */ /*

       ENOEXEC
              An executable is not in a recognized format, is for the  wrong  architecture,
              or has some other format error that means it cannot be executed.

	 -- LATEFAIL */ /*

       ENOMEM Insufficient kernel memory was available.

	 -- ALSOFAIL */ /*

       ENOTDIR
              A  component of the path prefix of filename or a script or ELF interpreter is
              not a directory.

	 -- LATEFAIL */ /*

       EPERM  The filesystem is mounted nosuid, the user is not the superuser, and the file
              has the set-user-ID or set-group-ID bit set.

	 -- LATEFAIL */ /*

       EPERM  The  process  is being traced, the user is not the superuser and the file has
              the set-user-ID or set-group-ID bit set.

	 -- LATEFAIL */ /*

       ETXTBSY
              Executable was open for writing by one or more processes.

	 -- LATEFAIL */
out:
	munmap(data, st.st_size);
	if (to_return == 0) *out_fd = fd; else close(fd);
	return to_return;
}

static _Bool is_directory(int fd, struct stat *st, void *data)
{
	return S_ISDIR(st->st_mode);
}
static _Bool is_regular_file(int fd, struct stat *st, void *data)
{
	return S_ISREG(st->st_mode);
}
static _Bool is_noexec_fs(int fd, struct stat *st, void *data)
{
	struct statvfs s;
	int ret = 0;//fstatvfs(fd, &s); // FIXME: use raw syscall
	return ret == 0 && (s.f_flag & ST_NOEXEC);
}
static _Bool is_empty_file(int fd, struct stat *st, void *data)
{
	return is_regular_file(fd, st, data) && st->st_size == 0;
}
static _Bool is_elf_with_multiple_interps(int fd, struct stat *st, void *data)
{
	return 0; /* FIXME */
}
#define MAX_HASH_BANG_WORDS 2 /* Linux */

static _Bool is_hash_bang_file(int fd, struct stat *st, void *data, struct hash_bang_word *out_hash_bang_words)
{
	/* FIXME: This is naive */
	if (((char*) data)[0] == '#' && ((char*) data)[1] == '!')
	{
		/* word-split what remains, up to MAX_HASH_BANG_WORDS */
		unsigned nwords = 0;
		char *p_c = (char*) data + 2;
		while (nwords < MAX_HASH_BANG_WORDS)
		{
			char *word_start = p_c;
			/* Look for the end of a word. */
			while (*p_c != '\0' && *p_c != ' ') ++p_c;
			
			out_hash_bang_words[nwords].chars = word_start;
			out_hash_bang_words[nwords].len = p_c - word_start;
			
			++nwords;
			
			if (*p_c == '\0') break;
		}
		return 1;
	}
	return 0;
}

static unsigned has_dynamic_segment(Elf64_Phdr *phdrs)
{
	for (; phdrs->p_type != PT_NULL; ++phdrs)
	{ if (phdrs->p_type == PT_DYNAMIC) return phdrs->p_offset; }
	return 0;
}

#define IS_ELF(x) ( \
	((char*) (x))[0] == '\177' && \
	((char*) (x))[1] == 'E' && \
	((char*) (x))[2] == 'L' && \
	((char*) (x))[3] == 'F')

static _Bool is_elf_static_executable(int fd, struct stat *st, void *data)
{
	return 0 == IS_ELF(((Elf64_Ehdr *) data)->e_ident)
		&& ((Elf64_Ehdr *) data)->e_type == ET_EXEC
		&& !has_dynamic_segment((void*)((char*) data + ((Elf64_Ehdr *) data)->e_phoff));
}

static _Bool is_elf_dynamic_executable(int fd, struct stat *st, void *data)
{
	return 0 == IS_ELF(((Elf64_Ehdr *) data)->e_ident)
		&& ((Elf64_Ehdr *) data)->e_type == ET_EXEC
		&& has_dynamic_segment((void*)((char*) data + ((Elf64_Ehdr *) data)->e_phoff));
}

static Elf64_Off addr_to_offset(Elf64_Addr addr, Elf64_Phdr *phdr)
{
	for (; phdr->p_type != PT_NULL; ++phdr)
	{
		if (phdr->p_vaddr <= addr && addr < phdr->p_vaddr + phdr->p_filesz)
		{
			return phdr->p_offset + (addr - phdr->p_vaddr);
		}
	}
	return (Elf64_Off) -1;
}

static _Bool is_elf_interpreter(int fd, struct stat *st, void *data)
{
	/* Tricky -- what defines an interpreter? I think it's the soname. */
	if (0 != IS_ELF(((Elf64_Ehdr *) data)->e_ident)
		|| ((Elf64_Ehdr *) data)->e_type != ET_DYN) return 0;
	Elf64_Phdr *phdrs = (void*)((char*) data + ((Elf64_Ehdr *) data)->e_phoff);
	unsigned dyn_off = has_dynamic_segment(phdrs);
	if (dyn_off)
	{
		Elf64_Dyn *p_dyn = (void*)((char*) data + dyn_off);
		for (; p_dyn->d_tag != DT_NULL; ++p_dyn)
		{
			if (p_dyn->d_tag == DT_SONAME)
			{
				Elf64_Addr soname_addr = p_dyn->d_un.d_ptr;
				Elf64_Off soname_off = addr_to_offset(soname_addr, phdrs);
				if (soname_off != (Elf64_Off) -1)
				{
					return 0 == strcmp(interpreter_to_use, (char*) data + soname_off);
				}
				else return 0;
			}
		}
	}
	// no dyn or no soname, so not an interp
	return 0;
}

static _Bool is_elf_shared_library(int fd, struct stat *st, void *data)
{
	return 0 == IS_ELF(((Elf64_Ehdr *) data)->e_ident)
		&& ((Elf64_Ehdr *) data)->e_type == ET_DYN
		&& !is_elf_interpreter(fd, st, data);
}

static _Bool is_executable(int fd, struct stat *st, void *data)
{
	return (st->st_mode & 001)
			|| ((st->st_mode & 0100) && geteuid() == st->st_uid)
			|| ((st->st_mode & 0010) && (getegid() == st->st_gid || group_member(st->st_gid)));
}

static int push_interpreter(const char *filename, char ***p_pre_padded_argv, unsigned *p_spaces)
{
#define PUSH_ONE_ARG(arg) \
	do { \
		if (*p_spaces < 1) return ELOOP; \
		(*p_pre_padded_argv--)[(*p_spaces)--] = arg; \
	} while (0)
	
	int fd = open(filename, O_RDONLY);
	if (fd == -1) return ENOENT;
	
	struct stat st;
	int ret = fstat(fd, &st);
	if (ret != 0) return errno;
	
	unsigned long page_size = 4096; //sysconf(_SC_PAGE_SIZE);
	void *data = mmap(NULL, ROUND_UP_TO_PAGE_SIZE(st.st_size, page_size), PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) return EINVAL;
	
	struct hash_bang_word hash_bang_words[MAX_HASH_BANG_WORDS] = { [0] = { NULL, 0 } };
	
	int to_return;
	
	// what is it?
	if (is_empty_file(fd, &st, data))
	{
		/* Is it the empty file? This should return ENOEXEC, not 0!
		 * (The reason that "true" can be implemented by an executable empty file
		 * is that the shell (or libc's exeve()) fails over to doing sh <file>.) */
		to_return = ENOEXEC;
	}
	else if (is_hash_bang_file(fd, &st, data, hash_bang_words))
	{
		for (struct hash_bang_word *p = &hash_bang_words[0];
					p - hash_bang_words < MAX_HASH_BANG_WORDS && p->chars;
					++p)
		{
			char *str = calloc(1, 1 + p->len);
			if (!str) my_abort();
			my_strncpy(str, p->chars, p->len); // FIXME: free this
			// FIXME: handle error
			PUSH_ONE_ARG(str);
		}
		
		to_return = 0;
	}
	else if (is_elf_static_executable(fd, &st, data))
	{
		to_return = 0;
	}
	else if (is_elf_dynamic_executable(fd, &st, data))
	{
		to_return = 0;
	}
	else if (is_elf_interpreter(fd, &st, data))
	{
		/* The caller *asked* to invoke an interpreter. 
		 * Can we chain-load interpreters?
		 * Probably not.
		 * HMM. What to do?
		 * Actually, why not?
		 * If I try
		 * $ /lib64/ld-linux-x86-64.so.2 /lib64/ld-linux-x86-64.so.2 /bin/true
		 * 
		 * I get
		 * "loader cannot load itself".
		 * BAH.
		 * 
		 * So we have to choose between interpreters.
		 * FIXME: replace their interpreter with us.
		 */
		to_return = 0;
	}
	else if (is_elf_shared_library(fd, &st, data))
	{
		/* Might be another shared library, like some builds of glibc,
		 * that have a PT_INTERP so can be run directly. Since it's not a
		 * loader, chain-loading shouldn't be a problem. So we use ourselves
		 * to load it. */
		to_return = 0;
	}
	else if (is_executable(fd, &st, data))
	{
		/* This is the binfmt-misc case. Nothing for now. */
		to_return = ENOEXEC;
	}
	else /* hmm, nothing we can execute */
	{
		to_return = ENOEXEC;
	}
	
	close(fd);
	munmap(data, page_size);
	return to_return;
}

static int
prefill_argv(const char *filename, char **pre_padded_argv, unsigned padding_nspaces)
{
	/* The caller wants to exec filename with pre_padded_argv.
	 * We try to "unpeel one level" of interpretation, and recurse. */
	
	errno = 0;
	char **initial_argv = pre_padded_argv;
	int ret = push_interpreter(filename, &pre_padded_argv, &padding_nspaces);
	if (pre_padded_argv != initial_argv)
	{
		assert(ret == 0);
		if (padding_nspaces < 1) return ELOOP; // FIXME: better error code
		pre_padded_argv[padding_nspaces - 1] = (char*) interpreter_to_use;
		return prefill_argv(interpreter_to_use, pre_padded_argv, padding_nspaces - 1);
	}
	else if (ret)
	{
		return ret;
	}
	// else
	return 0;
}

static unsigned va_length(va_list ap)
{
	// count the argv
	unsigned count = 1;
	for (char *cur_arg = va_arg(ap, char*); cur_arg; cur_arg = va_arg(ap, char*))
	{
		++count;
	}
	return count;
}

static unsigned argv_length(char *const *argv)
{
	// count the argv
	unsigned count = 0;
	while (*argv) ++argv;
	return count;
}

/* Everyone delegates to us.
 * We unpeel the chain of interpreters until we hit an actual ELF
 * binary invocation. We might have to react to exec'ability failures along
 * the way, like a non-executable file. We return these errors to the caller.
 * 
 * If we detect an error, we return the error code we think the caller
 * should see. */
int execve(const char *filename, char *const argv[],
          char *const envp[])
{
	/* Everyone delegates to us. */
	/* Can we exec it? 
	 * GAH
	 * Complication.
	 * Our execve will rewrite it to an ld.so invocation.
	 * Exec'ing ld.so *will* succeed even if the direct exec might fail.
	 * So we need to be doubly sure that whenever we exec ld.so,
	 * it's only on something that won't fail. 
	 * Check its execute permission,
	 * and check that it's an ELF executable.
	 * We need to do this *recursively* -- see below.
	 */
#define ARGV_PAD 16
	size_t argv_len = argv_length(argv);
	char **pre_padded_argv = alloca(
		sizeof (char*) * (argv_len + ARGV_PAD + 1)
	);
	memcpy(pre_padded_argv + ARGV_PAD, argv, sizeof (char*) * (argv_len + 1));
	
	int predicted_ret = prefill_argv(filename, pre_padded_argv, ARGV_PAD);
	if (predicted_ret != 0) return predicted_ret;
	char **new_argv = pre_padded_argv;
	while (!*new_argv) ++new_argv;
	assert(new_argv < pre_padded_argv + ARGV_PAD + argv_len);
	
	int fd = -1;
	predicted_ret = predict_exec_return_value(new_argv[0], new_argv + 1, &fd);
	if (predicted_ret != 0) return predicted_ret;
	assert(fd != -1);
	
	/* Okay, now we have a bona fide ELF file which we think is exec'able. */
	/* We still have to munge LD_PRELOAD. What do we want the munging to do? */
	// char **modified_envp = rewrie_ld_preload(envp);
	char **new_envp = (char**) envp; // FIXME
	
	/* i.e. our execve will do the *modified* thing. */
	char filename_to_exec[100];
	//int written = snprintf(filename_to_exec, sizeof filename_to_exec, "/proc/%d/fd/%d", 
	//	(int) getpid(), fd);
	//if (written >= sizeof filename_to_exec) /* strange */ abort();

	static int (*orig_execve)(const char *filename, char *const argv[],
		char *const envp[]);
	// if (!orig_execve) orig_execve = dlsym(RTLD_NEXT, "execve");
	
	int ret = orig_execve(filename_to_exec, new_argv + 1, new_envp);
	
	/* We should never reach here! So warn. */
	// FIXME warnx("execve failed (%d) but was predicted to succeed", ret);
	return ret;
}

static void make_argv(char **out, char *arg, va_list ap)
{
	unsigned i = 0u;
	for (char *cur_arg = arg; cur_arg; cur_arg = va_arg(ap, char*))
	{
		out[i++] = cur_arg;
	}
	out[i] = NULL;
}

struct execve_args
{
	const char *filename;
	char *const *argv;
	char *const *envp;
};

static int walk_path(const char *filename, int (*dir_cb)(const char *, size_t, void*), void *arg)
{
	char *colon_pos;
	//size_t filename_len = strlen(filename);
	for (char *pos = my_getenv("PATH"); pos; pos = strchrnul(pos, ':'))
	{
		colon_pos = strchr(pos, ':'); // might points to final \0
		int ret = dir_cb(pos, colon_pos - pos, arg);
		if (ret) return ret;
	}
	return 0;
}

static char *build_file_path(char *buf, size_t buflen, 
		const char *dirpath, size_t dirlen, 
		const char *filename, size_t filelen)
{
	if (dirlen + 1 + filelen + 1 < buflen) return NULL;
	my_strncpy(buf, dirpath, dirlen);
	buf[dirlen] = '/';
	my_strncpy(&buf[dirlen + 1], filename, filelen);
	buf[dirlen + 1 + filelen] = '\0';
	return &buf[0];
}
static int dir_cb_resolve_and_exec(const char *dir, size_t dirlen, void *arg)
{
	struct execve_args *args = arg;
	
	/* Build the pathname. */
	char buf[PATH_MAX];
	char *built = build_file_path(&buf[0], sizeof buf, 
		dir, dirlen,
		args->filename, strlen(args->filename));

	if (!built)
	{
		// error! what to do?
		return -1; // abort();
	}

	return execve(built, args->argv, args->envp);
}

int execl(const char *path, const char *arg, ...)
{
	va_list ap;
	va_start(ap, arg);
	unsigned count = va_length(ap);
	va_end(ap);
	
	// alloca the argv and build it
	char **argv = alloca(sizeof (char*) * count);
	va_start(ap, arg);
	make_argv(argv, (char*) arg, ap);
	va_end(ap);
	
	// delegate
	return execve(path, argv, environ);
}

int execlp(const char *file, const char *arg, ...)
{
	va_list ap;
	va_start(ap, arg);
	unsigned count = va_length(ap);
	va_end(ap);
	
	// alloca the argv and build it
	char **argv = alloca(sizeof (char*) * count);
	va_start(ap, arg);
	make_argv(argv, (char*) arg, ap);
	va_end(ap);
	
	// delegate
	return execvpe(file, argv, environ);
}

int execle(const char *path, const char *arg,
          .../*, char * const envp[] */)
{
	va_list ap;
	va_start(ap, arg);
	unsigned count = va_length(ap);
	va_end(ap);
	
	// alloca the argv and build it
	char **argv = alloca(sizeof (char*) * count);
	va_start(ap, arg);
	make_argv(argv, (char*) arg, ap);
	va_end(ap);
	
	// the last element in the argv is actually the envp
	char **envp = (char**) argv[count - 1];
	argv[count - 1] = NULL;
	
	// delegate
	return execve(path, argv, envp);
}

int execv(const char *path, char *const argv[])
{
	return execve(path, argv, environ);
}

int execvp(const char *file, char *const argv[])
{
	return execvpe(file, argv, environ);
}

int execvpe(const char *file, char *const argv[],
            char *const envp[])
{
	/* Do the pathname resolution. */
	struct execve_args args = { file, argv, envp };
	int ret = walk_path(file, dir_cb_resolve_and_exec, &args);
	if (ret) return ret;
	return execve(args.filename, args.argv, args.envp);
}

/* FIXME: intercept syscall too */
// long syscall(long number, ...)
// {
// 	if (num == __NR_execve)
// 	{
// 		
// 	}
// }
