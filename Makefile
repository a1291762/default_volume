# program definition
TARGET = default_volume
OBJECTS = my.res.o default_volume.o trayicon.o main.o

# enable "all" warnings and treat warnings as errors
CFLAGS += -Wall -Werror

# enable debugging and disable optimisation
CFLAGS += -g -O0 

# Build with UNICODE support
CFLAGS += -municode

# produce a non-console application
CFLAGS += -mwindows

# link to these system libraries
LIBS = -lshlwapi -lole32

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

%.res.o: %.rc
	windres $< $@

clean:
	-rm $(TARGET) $(OBJECTS)

.PHONY: all clean
