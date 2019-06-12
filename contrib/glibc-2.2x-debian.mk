ALL_RTLD_ROUTINES := $$(rtld-routines)
ALL_RTLD_ROUTINES += $$(sysdep-rtld-routines)
EXT_ROUTINES := $(patsubst %.c,%,$(patsubst %.s,%,$(patsubst %.S,%,$(EXT_SRC))))
ALL_RTLD_ROUTINES += $(EXT_ROUTINES)

ifeq ($(RTLD_OBJ_DIR),)
$(error Check configuration: RTLD_OBJ_DIR should be set)
else
$(info RTLD_OBJ_DIR: $(RTLD_OBJ_DIR))
endif
ifeq ($(realpath $(RTLD_OBJ_DIR)),)
$(error Check configuration: RTLD_OBJ_DIR does not exist: $(RTLD_OBJ_DIR))
endif
$(info RTLD_OBJ_DIR: $(RTLD_OBJ_DIR))

# PROBLEM: when we have a fresh build of libc,
# our rtld objects will look new, even though we
# now have to replace them with our rebuilt one.
# Since rebuilding the .os is cheap, we force it.
RTLD_OS := $(shell rm -f \
	$(RTLD_OBJ_DIR)/librtld.os \
	$(RTLD_OBJ_DIR)/librtld.os \
	$(RTLD_OBJ_DIR)/librtld.os.map \
	$(RTLD_OBJ_DIR)/rtld-libc.a \
	$(RTLD_OBJ_DIR)/rtld.os \
	$(RTLD_OBJ_DIR)/librtld.mk \
	$(RTLD_OBJ_DIR)/librtld.map* \
	$(RTLD_OBJ_DIR)/librtld.map* \
	$(foreach f,$(EXT_ROUTINES),$(RTLD_OBJ_DIR)/$(f).os $(RTLD_OBJ_DIR)/$(f).os.d $(RTLD_OBJ_DIR)/$(f).test); \
	echo $(RTLD_OBJ_DIR)/librtld.os)

$(RTLD_OS) $(RTLD_OBJ_DIR)/ld.so: do-libc-rebuild

# the important thing to rebuild is librtld.os
.PHONY: do-libc-rebuild
do-libc-rebuild: $(EXT_SRC)
	LD_SO_LDFLAGS="$(LD_EXT_SO_LDFLAGS)" $(MAKE) VPATH="$(shell pwd)" -C $(LIBC_SRC_DIR)/elf \
	    objdir='$(realpath $(RTLD_OBJ_DIR)/..)' \
	    all-rtld-routines='$(ALL_RTLD_ROUTINES)' $(RTLD_OBJ_DIR)/librtld.os

LD_SO_VERLIST := $(RTLD_OBJ_DIR)/../ld.map
LD_SO_DYNLIST :=
LD_EXT_SO_OBJS := $(RTLD_OS)
LD_SO_SONAME := ld-linux-x86-64.so.2
LD_SO_LDOPTS += --defsym=_begin=0 -z combreloc -z relro --hash-style=both -z defs

# This should help make ld.so startup logic more interposable
CFLAGS-rtld.c += -ffunction-sections
