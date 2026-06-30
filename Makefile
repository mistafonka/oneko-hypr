CC ?= cc
PKG := gtk+-3.0 gtk-layer-shell-0 gdk-pixbuf-2.0
CFLAGS ?= -O2 -pipe
CFLAGS += -Wall -Wextra -std=c11 $(shell pkg-config --cflags $(PKG))
LDLIBS += $(shell pkg-config --libs $(PKG)) -lm

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/oneko-hypr

all: oneko-hypr

oneko-hypr: src/main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: oneko-hypr
	install -Dm755 oneko-hypr "$(DESTDIR)$(BINDIR)/oneko-hypr"
	install -Dm644 assets/oneko.gif "$(DESTDIR)$(DATADIR)/oneko.gif"

clean:
	rm -f oneko-hypr oneko-desktop

.PHONY: all install clean
