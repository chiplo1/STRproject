CC =  gcc # Path to compiler
L_FLAGS = -lrt -lpthread 
#C_FLAGS = -g

all: linux
.PHONY: all

# Project compilation
linux: linux.c
	$(CC) $< -o $@ $(C_FLAGS) $(L_FLAGS)
	
.PHONY: clean 

clean:
	rm -f *.c~ 
	rm -f *.o
	rm linux

# Some notes
# $@ represents the left side of the ":"
# $^ represents the right side of the ":"
# $< represents the first item in the dependency list   

