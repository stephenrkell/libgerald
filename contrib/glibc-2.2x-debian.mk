all_rtld_routines := $$(rtld-routines)
all_rtld_routines += $$(sysdep-rtld-routines)
ext_routines := $(patsubst %.c,%,$(patsubst %.s,%,$(patsubst %.S,%,$(EXT_SRC))))
all_rtld_routines += $(ext_routines)
rtld_obj_dir := $(RTLD_OBJ_DIR)

# PROBLEM: when we have a fresh build of libc,
# our rtld objects will look new, even though we
# now have to replace them with our rebuilt one.
# Since rebuilding the .os is cheap, we force it.
RTLD_OS := $(shell rm -f \
	$(rtld_obj_dir)/librtld.os \
	$(rtld_obj_dir)/librtld.os \
	$(rtld_obj_dir)/librtld.os.map \
	$(rtld_obj_dir)/rtld-libc.a \
	$(rtld_obj_dir)/rtld.os \
	$(rtld_obj_dir)/librtld.mk \
	$(rtld_obj_dir)/librtld.map* \
	$(rtld_obj_dir)/librtld.map* \
	$(foreach f,$(ext_routines),$(rtld_obj_dir)/$(f).os $(rtld_obj_dir)/$(f).os.d $(rtld_obj_dir)/$(f).test); \
	echo $(RTLD_OBJ_DIR)/librtld.os)

$(RTLD_OS) $(rtld_obj_dir)/ld.so: do-libc-rebuild

.PHONY: do-libc-rebuild
do-libc-rebuild: $(EXT_SRC)
	$(MAKE) VPATH="$(shell pwd)" -C $(LIBC_SRC_DIR)/elf \
	    objdir='$(realpath $(rtld_obj_dir)/..)' \
	    all-rtld-routines='$(all_rtld_routines)'

LD_SO_VERLIST := $(rtld_obj_dir)/../ld.map
LD_SO_DYNLIST :=
LD_EXT_SO_OBJS := $(RTLD_OS)
LD_SO_SONAME := ld-linux-x86-64.so.2
LD_SO_LDOPTS := --defsym=_begin=0 -z combreloc -z relro --hash-style=both -z defs
