default: src

contrib/config.mk:
	$(MAKE) -C contrib

ifeq ($(RTLD_OBJ_DIR),)
include contrib/config.mk
endif

.PHONY: src
src:
	$(MAKE) -C src RTLD_OBJ_DIR="$(RTLD_OBJ_DIR)" LIBC_SRC_DIR="$(LIBC_SRC_DIR)" PKG_VER="$(PKG_VER)"
