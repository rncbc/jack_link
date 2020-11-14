NAME     = jack_link

VERSION ?= $(shell \
	git describe --tags --dirty --abbrev=6 2>/dev/null \
	| sed 's/^[^0-9]\+//;s/-g/git./;s/[_|-]\+/./g')

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
TARGET   = $(NAME)

ifneq ($(NAME),)
CCFLAGS += -D_NAME="$(NAME)"
endif

ifneq ($(VERSION),)
CCFLAGS += -D_VERSION="$(VERSION)"
endif

LINK_VERSION = $(shell cd link && \
	git describe --tags --dirty --abbrev=6 2>/dev/null \
	| sed 's/^[^0-9]\+//;s/-g/git./;s/[_|-]\+/./g')

ifneq ($(LINK_VERSION),)
CCFLAGS += -D_LINK_VERSION="$(LINK_VERSION)"
endif

CCFLAGS += -g -O2 -std=c++11
CCFLAGS += -Wno-multichar

#https://stackoverflow.com/questions/714100/os-detecting-makefile
ifeq ($(OS),Windows_NT) 
    DETECTED_OS := Windows
else
    DETECTED_OS := $(shell sh -c 'uname 2>/dev/null || echo Unknown')
endif

ifeq ($(DETECTED_OS), Linux)
CCFLAGS += -DLINK_PLATFORM_LINUX=1
else
ifeq ($(DETECTED_OS), Darwin)
CCFLAGS += -DLINK_PLATFORM_MACOSX=1
else
CCFLAGS += -DLINK_PLATFORM_WINDOWS=1
endif
endif

CCFLAGS += -Ilink/include

CCFLAGS += -DASIO_STANDALONE=1
CCFLAGS += -Ilink/modules/asio-standalone/asio/include

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
	rm -vf *.o

distclean:	clean
	rm -vf $(TARGET)
