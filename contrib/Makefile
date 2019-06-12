default: build config.mk

# PROBLEM: libc and ld.so are coupled, at least
# in the case of glibc: libc is a client of 
# private structures defined in ld.so.
# 
# I believe the same is true of musl libc, which
# at least has the decency to make them the same
# binary.
# 
# Perhaps the best solution for working around this
# would be to reflect on the active libc and synthesise
# whatever interfaces it expects. That is doable (up to a 
# point), using liballocs introspection, but will require 
# some hacking. For now, to work around this, we want to 
# "simply" ensure that we build a ld.so that is compatible
# with the system-installed libc. Note that even compiler
# flags can affect the ABI. Perhaps the best we can do is
# match the version, then do a DWARF-based check that we're
# compatible (i.e. after accounting for compiler flags).

TARGET ?= $(shell (gcc -dumpmachine || clang -dumpmachine) )
target := $(TARGET)
$(info TARGET is $(target))

ifeq ($(LIBC_BUILD_DIR),)

# Bootstrap via /etc/os-release.
# How to tell between eglibc and glibc? That's why we
# need the distributor's package management utilities.
OS_ID := $(shell case "$(target)" in \
(x86_64*-linux-gnu) \
    . /etc/os-release && echo "$$ID" ;; \
(*) echo "Target triple not yet supported: $(target)" 1>&2 ;; \
esac)
ifeq ($(OS_ID),debian)
    os_family := debianlike
else
ifeq ($(OS_ID),ubuntu)
    os_family := debianlike
else
    os_family := $(OS_ID)
endif
endif

$(info OS_ID is $(OS_ID))
$(info os_family is $(os_family))
ifeq ($(os_family),debianlike)

# we only use GLIBC_PKGSTR if GLIBC_SRC_DIR is not set
GLIBC_PKGSTR := $(shell \
   binpkg="$$( dpkg -S $$(readlink -f $$(ls -1 /lib/x86_64*-linux-gnu/libc.so* | head -n1 ) ) | tr -s '[:blank:]' '\t' | cut -f1 | sed 's/:$$//')"; \
   dpkg-query --showformat='$${Package}=$${Version}\n' -W "$$binpkg" | sort | uniq )
$(info GLIBC_PKGSTR is $(GLIBC_PKGSTR))
LIBC_RULES_MK := $(shell case "$(GLIBC_PKGSTR)" in \
 (*2.19) echo glibc-2.1x-debian.mk;;\
 (*2.2*) echo glibc-2.2x-debian.mk;;\
esac)

ifeq ($(LIBC_RULES_MK),)
$(error Did not recognise package/version: $(GLIBC_PKGSTR))
endif

# HACK: have to grab and unpack the package before we know its src dirname
GLIBC_SRC_DIR ?= $(shell apt-get source "$(GLIBC_PKGSTR)" 1>&2 && \
                 find -maxdepth 1 -mindepth 1 -type d | xargs ls -1dt | head -n1 )
LIBC_SRC_DIR := $(GLIBC_SRC_DIR)
libc_src_dir := $(LIBC_SRC_DIR)

ifeq ($(LIBC_SRC_DIR),)
$(error Could not get libc source dir for package string $(GLIBC_PKGSTR) / source dir $(GLIBC_SRC_DIR))
endif

# HACK -- use change-prone knowledge of the build process here
# HACK: please remove arch-specific strings here
LIBC_BUILD_DIR := $(LIBC_SRC_DIR)/build-tree
libc_build_dir := $(LIBC_BUILD_DIR)
$(info libc_build_dir is $(libc_build_dir))

.PHONY: build
build: $(LIBC_BUILD_DIR)/amd64-libc/elf/ld.so

$(LIBC_BUILD_DIR)/amd64-libc/elf/ld.so: $(LIBC_SRC_DIR)
	cd $(LIBC_SRC_DIR) && dpkg-buildpackage --no-sign -nc -rfakeroot

config.mk: | build
	(/bin/echo 'RTLD_OBJ_DIR := $(realpath $(libc_build_dir))/amd64-libc/elf'; \
	 /bin/echo 'OS_ID := $(OS_ID)'; \
	 /bin/echo 'LIBC_SRC_DIR := $(realpath $(LIBC_SRC_DIR))'; \
	 /bin/echo 'PKG_VER := $(notdir $(LIBC_SRC_DIR))'; \
	 /bin/echo 'LIBC_RULES_MK := $(realpath $(LIBC_RULES_MK))'; \
	 ) \
| sed '/^\\$$/ d' | sed 's/ *$$//' > "$@" || rm -f "$@"

else # the non-Debian-like case

#    libcfile="$$(basename $$(readlink -f $$(ls -1 /lib/x86_64*-linux-gnu/libc.so* | head -n1 ) ) )"; \
#    case "$$libcfile" in \
#    (libc-2.19.so) echo "eglibc-2.19";; \
#    (libc-2.24.so) echo  "glibc-2.24";; \
#    (*) ;; \
#    esac;; \

libc_src_dir := $(LIBC_SRC_DIR)
$(info LIBC_SRC_DIR is $(libc_src_dir))

LIBC_BUILD_DIR := $(shell case "$(target)" in \
(x86_64*-linux-gnu) \
    . /etc/os-release && case "$$ID" in \
    (debian|ubuntu) \
        case "$(libc_src_dir)" in \
            (glibc-*) echo "$(libc_src_dir)"/build;; \
            (eglibc-*) echo "$(libc_src_dir)"/libc/build;; \
            (*) echo "Error: no known libc build dir for $$ID (srcdir: $(libc_src_dir))" 1>&2;; \
        esac ;; \
    (*) echo "OS ID not yet supported: $$ID" 1>&2 ;; \
    esac;; \
(*) echo "Target triple not yet supported: $(target)" 1>&2 ;; \
esac)

# default *also* includes extracting that
default: extract

endif

endif

libc_build_dir := $(LIBC_BUILD_DIR)
ifeq ($(libc_build_dir),)
$(error "Could not guess which C library build is needed on platform $(target).")
endif
