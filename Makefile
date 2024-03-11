include config.mk

all: $(DAEMON) $(CLIENT) $(HELPER_LIB)

$(DAEMON): runns.o
	$(CC) -o $@ $<

$(CLIENT): $(CLIENT).o
	$(CC) -o $@ $<

$(HELPER_LIB): librunns.c
	$(CC) -o $@ -shared -fPIC $<

.PHONY: tests
tests: container

container:
	@ git submodule status | grep -q -v '^-' || { echo 'tau submodule is not initialized'; exit 1; }
	@ git ls-tree -r --name-only @ | cpio -o -H newc > runns.cpio
	@ git --git-dir tau/.git ls-tree -r --name-only @ | sed -n 's;.*;tau/&;p' | cpio -o -A -H newc -F runns.cpio
	@ docker build -t runns_test --build-arg SRC=runns.cpio .

tests_build:
	$(MAKE) -C tests/ build

tests_run:
	$(MAKE) -C tests/ run

.PHONY: clean
clean:
	rm -f $(DAEMON) $(CLIENT) $(HELPER_LIB) *.o
	$(MAKE) -C tests/ clean

.PHONY: install
install: $(DAEMON) $(CLIENT) $(HELPER)
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp $^ $(DESTDIR)/$(PREFIX)/bin/
	chmod 755 $(DESTDIR)/$(PREFIX)/bin/{$(subst $(_),$(comma),$^)}

.PHONY: uninstall
uninstall: $(CLIENT) $(DAEMON) $(HELPER)
	rm -f $(DESTDIR)/$(PREFIX)/bin/{$(subst $(_),$(comma),$^)}

.PHONY: distclean
distclean:
	rm -vrf $(DAEMON) $(CLIENT) *.o autom4te.cache config.log config.status config.mk configure

define HELP_MESSAGE
The following rules are available:

  all (default) -- build the daemon, client and the helper libs
  tests -- build and run the unit tests in container
  tests_build -- just build the unit tests
  tests_run -- just run the unit tests
  clean -- remove object files, the built binaries including unit tests
  install -- install the daemon and client to the system
  uninstall -- revert install
  distclean -- remove all built binaries and configuration
endef

.PHONY: help
help:
	@ $(info $(HELP_MESSAGE))
