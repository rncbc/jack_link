NAME    ?= jack_link

VERSION ?= $(shell \
	git describe --tags --dirty --abbrev=6 2>/dev/null |\
	sed 's/^[vV]//;s/^[^_]\+_//;s/-g/git./;s/[_|-]\+/./g')

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
TARGET   = $(NAME)

ifneq ($(NAME),)
CCFLAGS += -D_NAME="$(NAME)"
endif

ifneq ($(VERSION),)
CCFLAGS += -D_VERSION="$(VERSION)"
endif

CCFLAGS += -g -O2 -std=c++11
CCFLAGS += -Wno-multichar
CCFLAGS += -DLINK_PLATFORM_LINUX=1
CCFLAGS += -Ilink/include

LDFLAGS += -ljack -lpthread

HEADERS  = jack_link.hpp
SOURCES  = jack_link.cpp

all:	$(TARGET)

$(TARGET):	$(SOURCES) $(HEADERS)
	g++ $(CCFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

install:	$(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m755 $(TARGET) $(DESTDIR)$(BINDIR)

uninstall:	$(DESTDIR)$(BINDIR)/$(TARGET)
	rm -vf $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -vf $(TARGET) *.o
