TARGET   = jack_link

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

clean:
	rm -vf $(TARGET) *.o
