CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
STRIP ?= strip

CFLAGS += -I.
LDFLAGS += -static

SRC = src/*.c

shorkstall: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o shorkstall $(LDFLAGS)
	$(STRIP) shorkstall

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin

install: shorkstall
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 shorkstall $(DESTDIR)$(BINDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/shorkstall

clean:
	rm -f shorkstall

.PHONY: install uninstall clean
