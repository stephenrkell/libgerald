EXT_SRC := wrap.c
EXT_DYNSYMS := 

#LD_EXT_SO_LDFLAGS += --wrap _dl_init
LD_SO_LDOPTS += $(LD_EXT_SO_LDFLAGS)
include ../../mk/Makerules

# PROBLEM: we also have to pass these flags to the glibc build process,
# somehow, else the undefined reference in wrap.o will prevent linking.
# WAIT.
# Why is the glibc build including our wrap.o?
# It shouldn't.
# We run the glibc build process as
# 	LD_SO_LDFLAGS="$(LD_EXT_SO_LDFLAGS)" $(MAKE) VPATH="$(shell pwd)" -C $(LIBC_SRC_DIR)/elf \
#	    objdir='$(realpath $(rtld_obj_dir)/..)' \
#	    all-rtld-routines='$(all_rtld_routines)'
# .. so it's our use of all_rtld_routines (which is a list of .o files without the .o suffix)
# that is adding to its dependencies.
# This is so that wrap.c (or whatever)
# can use libc routines not in the vanilla libc build,
# hence pulling them into librtld.os

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
