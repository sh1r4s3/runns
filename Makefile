include config.mk

all: $(DAEMON) $(CLIENT) $(HELPER_LIB)

$(DAEMON): runns.o
	$(CC) -o $@ $<

$(CLIENT): $(CLIENT).o
	$(CC) -o $@ $<

$(HELPER_LIB): librunns.c
	$(CC) -o $@ -shared -fPIC $<

.PHONY: tests
tests: tests_build tests_run

tests_build:
	$(MAKE) -C tests/

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
