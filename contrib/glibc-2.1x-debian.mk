all_rtld_routines := $$(rtld-routines)
all_rtld_routines += $$(sysdep-rtld-routines)
all_rtld_routines += $(patsubst %.c,%.o,$(patsubst %.s,%.o,$(patsubst %.S,%.o,$(EXT_FILES))))

rtld_obj_dir := $(RTLD_OBJ_DIR)

# PROBLEM: when we have a fresh build of libc,
# our rtld objects will look new, even though we
# now have to replace them with our rebuilt one.
# Since rebuilding the .os is cheap, we force it.
RTLD_OS := $(shell rm -f $(rtld_obj_dir)/librtld.os \
	rm -f $(rtld_obj_dir)/librtld.os \
	rm -f $(rtld_obj_dir)/librtld.os.map \
	rm -f $(rtld_obj_dir)/rtld-libc.a \
	rm -f $(rtld_obj_dir)/rtld.os \
	rm -f $(rtld_obj_dir)/librtld.mk \
	rm -f $(rtld_obj_dir)/librtld.map*; \
	echo $(RTLD_OBJ_DIR)/librtld.os)

$(RTLD_OS): $(EXT_SRC)# preload.os
	$(MAKE) VPATH="$(shell pwd)" -C $(rtld_obj_dir) \
	    all-rtld-routines='$(all_rtld_routines)' ;; \

LD_SO_VERLIST := $(rtld_obj_dir)/../ld.map
LD_SO_DYNLIST :=
LD_EXT_SO_OBJS := $(RTLD_OS)
LD_SO_SONAME := ld-linux-x86-64.so.2
LD_SO_LDOPTS := --defsym=_begin=0 -z combreloc -z relro --hash-style=both -z defs
