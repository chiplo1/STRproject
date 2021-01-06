# Standard Makefile to buiild Xenomai 3 applications, 
#   with alchemy skin (former native skin)
# Use xeno-config to get the correct compile and link flags
# Don't forget to specify the skin
XENO_CONFIG := /usr/xenomai/bin/xeno-config
CFLAGS := $(shell $(XENO_CONFIG) --skin=alchemy --cflags)
LDFLAGS := $(shell $(XENO_CONFIG) --skin=alchemy --ldflags)
CC := $(shell $(XENO_CONFIG) --cc)

EXECUTABLE := waveformGen

all: $(EXECUTABLE)

%: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -lm
	
clean:	
	rm $(EXECUTABLE)
	
