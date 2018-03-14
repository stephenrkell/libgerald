# NOTE: do NOT create any .c files in this directory unless
# you're okay for the ld.so build sstem to try to incorporate them
# into dl-allobjs.os.
#
# See a hack below for building hello from hello.nolang, which is
# really a C file.

LD_SO_LDOPTS += hooks-noop.o \
     -init _dl_srk_init \
     --wrap dl_main
#    --defsym __ref_dl_main=__def_dl_main

include ../../mk/Makerules

%/librtld-hacked.os: %/librtld.os
	cp "$<" "$@"
	#tmpfile="$$(mktemp)"; objcopy --unbind-sym dl_main "$<" "$$tmpfile" && \
    #   objcopy --globalize-symbol dl_main "$$tmpfile" "$@"

vpath %.c ../../src
hooks-noop.o: CFLAGS += -g -O1 -fPIC

$(LD_EXT_SO): hooks-noop.o

hello: hello.nolang
	$(CC) -o "$@" -x c "$<" $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)

hello-inplace: hello.nolang
	$(CC) -Wl,--dynamic-linker,$(shell pwd)/ld-ext.so -o "$@" -x c "$<" $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)

preload.so: preload.nolang
	$(CC) -shared -o "$@" -x c "$<" -fPIC $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)
