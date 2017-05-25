# tarball rules (FIXME: fix/improve) -------------------------------------------------

# generic extraction rule
.PHONY: extract-%
extract-%: %
	test -d $$( tar --list -af "$<" | grep '/.*/' | head -n1 | xargs dirname ) || \
	    tar -k -xaf "$<"

# eglibc 2.19 build
TAR_eglibc-2.19 := eglibc-2.19-svnr25243.tar.bz2
$(TAR_eglibc-2.19):
	wget http://downloads.yoctoproject.org/releases/eglibc/eglibc-2.19-svnr25243.tar.bz2
.PHONY: eglibc-2.19-build
eglibc-2.19-build: extract-$(TAR_eglibc-2.19)
	eglibc_dir=eglibc-2.19/libc || exit 1; \
	(test -e "$$eglibc_dir"-build/config.log && \
	        ! grep 'exit 1' "$$eglibc_dir"-build/config.log) || \
	    (mkdir -p eglibc-2.19-build && cd eglibc-2.19-build && \
	CFLAGS="-g -O2" ../eglibc-2.19/libc/configure --prefix=/usr); \
	cd eglibc-2.19-build && $(MAKE) -j3

# glibc 2.24 build
TAR_glibc-2.24 := glibc-2.24.tar.xz
$(TAR_glibc-2.24):
	wget https://ftp.gnu.org/gnu/glibc/glibc-2.24.tar.xz
.PHONY: glibc-2.24-build
glibc-2.24-build: extract-$(TAR_glibc-2.24)
	glibc_dir=glibc-2.24; \
	(test -e "$$glibc_dir"-build/config.log && \
	        ! grep 'exit 1' "$$glibc_dir"-build/config.log) || \
	    (mkdir -p glibc-2.24-build && cd glibc-2.24-build && \
	CFLAGS="-g -O2" ../glibc-2.24/configure --prefix=/usr --disable-selinux); \
	cd glibc-2.24-build && $(MAKE) -j3

