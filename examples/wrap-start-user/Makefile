EXT_SRC := wrap.c
EXT_DYNSYMS := 

# This should help make ld.so startup logic more interposable
CFLAGS-rtld.c += -ffunction-sections
export CFLAGS-rtld.c
# Exporting CFLAGS-rtld.c only works if
# (1) a shell that doesn't mind funky vars is used, or
# (2) make sees a command that looks simple enough to run without a shell.
# Commands with shell metacharacters are always run via a shell, and our
# libc recursive sub-make is such a command. So HACK: force the use of bash.
SHELL := /bin/bash

#LD_EXT_SO_LDFLAGS += --wrap _dl_init
LD_SO_LDOPTS += $(LD_EXT_SO_LDFLAGS)
include ../../mk/Makerules

# We also have to pass our these flags to the glibc build process,
# somehow, else the undefined reference in wrap.o will prevent linking.
# Remember: the glibc build including our wrap.o, so we can exploit
# its magic for pulling all needed libc routines into librtld.os.
# We expose wrap.o this way so that it is free to use libc routines
# not in the vanilla libc build,
# We run the glibc build process as
# 	LD_SO_LDFLAGS="$(LD_EXT_SO_LDFLAGS)" $(MAKE) VPATH="$(shell pwd)" -C $(LIBC_SRC_DIR)/elf \
#	    objdir='$(realpath $(rtld_obj_dir)/..)' \
#	    all-rtld-routines='$(all_rtld_routines)'
# .. so it's our use of all_rtld_routines (which is a list of .o files without the .o suffix)
# that is adding to its dependencies.

# On glibc-2.28, the makerule which does the ld.so build is
# $(objpfx)ld.so: $(objpfx)librtld.os $(ld-map)
# Link into a temporary file so that we don't touch $@ at all
# if the sanity check below fails.
#	$(LINK.o) -nostdlib -nostartfiles -shared -o $@.new		\
#		  $(LDFLAGS-rtld) -Wl,-z,defs $(z-now-$(bind-now))	\
#		  $(filter-out $(map-file),$^) $(load-map-file)		\
#		  -Wl,-soname=$(rtld-installed-name)			\
#		  -Wl,-defsym=_begin=0
#	$(call after-link,$@.new)
#	$(READELF) -s $@.new \
#	  | $(AWK) '($$7 ~ /^UND(|EF)$$/ && $$1 != "0:" && $$4 != "REGISTER") { print; p=1 } END { exit p != 0 }'
#	mv -f $@.new $@
#
# where on my build, LDFLAGS-rtld is
# -Wl,-z,combreloc -Wl,-z,relro -Wl,--hash-style=both
# and the input files ($(filter-out $(map-file),$^) $(load-map-file)) are
# /usr/local/src/glibc-2.28/build-tree/amd64-libc/elf/librtld.os -Wl,--version-script=/usr/local/src/glibc-2.28/build-tree/amd64-libc/ld.map 

# HMM, OK. Building just librtld.os seems to work now.

# explicitly set the default linker, to avoid polluting with our option hack below (it'd break this)
%/librtld-hacked.os: LD := $(LD)
%/librtld-hacked.os: %/librtld.os
	objcopy --globalize-symbol _dl_start $< && $(LD) -r -o $@ --defsym __orig__dl_start=_dl_start $+

# to ensure our defsym goes first, put it in the LD command itself
$(LD_EXT_SO): LD := $(LD) -z muldefs --defsym _dl_start=__wrap__dl_start
