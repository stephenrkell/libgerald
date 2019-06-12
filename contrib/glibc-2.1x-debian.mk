ALL_RTLD_ROUTINES := $$(rtld-routines)
ALL_RTLD_ROUTINES += $$(sysdep-rtld-routines)
ALL_RTLD_ROUTINES += $(patsubst %.c,%.o,$(patsubst %.s,%.o,$(patsubst %.S,%.o,$(EXT_SRC))))

# PROBLEM: when we have a fresh build of libc,
# our rtld objects will look new, even though we
# now have to replace them with our rebuilt one.
# Since rebuilding the .os is cheap, we force it.
RTLD_OS := $(shell rm -f $(RTLD_OBJ_DIR)/librtld.os \
	rm -f $(RTLD_OBJ_DIR)/librtld.os \
	rm -f $(RTLD_OBJ_DIR)/librtld.os.map \
	rm -f $(RTLD_OBJ_DIR)/rtld-libc.a \
	rm -f $(RTLD_OBJ_DIR)/rtld.os \
	rm -f $(RTLD_OBJ_DIR)/librtld.mk \
	rm -f $(RTLD_OBJ_DIR)/librtld.map*; \
	echo $(RTLD_OBJ_DIR)/librtld.os)

$(RTLD_OS): $(EXT_SRC)# preload.os
	$(MAKE) VPATH="$(shell pwd)" -C $(RTLD_OBJ_DIR) \
	    all-rtld-routines='$(ALL_RTLD_ROUTINES)'

LD_SO_VERLIST := $(RTLD_OBJ_DIR)/../ld.map
LD_SO_DYNLIST :=
LD_EXT_SO_OBJS := $(RTLD_OS)
LD_SO_SONAME := ld-linux-x86-64.so.2
LD_SO_LDOPTS := --defsym=_begin=0 -z combreloc -z relro --hash-style=both -z defs
