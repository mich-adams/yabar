VERSION ?= $(shell git describe)
CPPFLAGS += -DVERSION=\"$(VERSION)\" -D_POSIX_C_SOURCE=199309L -DYA_INTERNAL \
			-DYA_ENV_VARS -DYA_INTERNAL_EWMH -DYA_ICON -DYA_MUTEX -DYA_VAR_WIDTH \
			-DYA_BSPWM
CFLAGS += -std=c99 -Iinclude -pedantic -Wall -flto -O2 `pkg-config --cflags pango pangocairo libconfig gdk-pixbuf-2.0 alsa`
DEPS += pango pangocairo libconfig gdk-pixbuf-2.0 alsa
ifdef PLAYERCTL
CFLAGS += -DPLAYERCTL
CPPFLAGS += -DPLAYERCTL
DEPS += playerctl
endif

LDFLAGS += -flto -O2 
LDLIBS += -liw -lxcb -lpthread -lxcb-randr -lxcb-ewmh -lxcb-icccm -lxkbcommon -lxkbcommon-x11 -lxcb-xkb -lm `pkg-config --libs $(DEPS)` 
PROGRAM := yabar
DOCS := $(PROGRAM).1
PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
MANPREFIX ?= $(PREFIX)/share/man

OBJS := $(wildcard src/*.c) $(wildcard src/intern_blks/*.c)
OBJS := $(OBJS:.c=.o)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
all: $(PROGRAM) $(DOCS)
$(PROGRAM): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
docs: $(DOCS)
install:
	mkdir -p "$(DESTDIR)$(BINPREFIX)"
	cp -pf $(PROGRAM) "$(DESTDIR)$(BINPREFIX)"
	mkdir -p "$(DESTDIR)$(MANPREFIX)"/man1
	cp -pf "doc/$(PROGRAM).1" "$(DESTDIR)$(MANPREFIX)/man1"
uninstall:
	rm -f "$(DESTDIR)$(BINPREFIX)/$(PROGRAM)"
	rm -f "$(DESTDIR)$(MANPREFIX)/man1/$(PROGRAM).1"
$(PROGRAM).1: doc/$(PROGRAM).1.asciidoc
	a2x --doctype manpage --format manpage "doc/$(PROGRAM).1.asciidoc"
clean:
	rm -f src/*.o src/intern_blks/*.o $(PROGRAM) "doc/$(PROGRAM).1"

.PHONY: all install uninstall clean
