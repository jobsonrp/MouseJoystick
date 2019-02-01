obj- := dummy.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# List of programs to build
hostprogs-y := mousejoystick

# Tell kbuild to always build the programs
always := $(hostprogs-y)

HOSTCFLAGS_mousejoystick.o += -I$(objtree)/usr/include
