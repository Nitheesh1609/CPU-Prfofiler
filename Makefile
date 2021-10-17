# A variable named MODULE which refers to project2
MODULE	 = project3
#  Creates a module object file as project2.0
obj-m += $(MODULE).o

#  KERNELDIR which is assigned to the Kernel module build directory.
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

#  PWD is assigned to the current directory
PWD := $(shell pwd)

#  Default targt is all and which will build the targets named ex3
all: $(MODULE)


#  This statement builds the .o file for the respective .c file with the CC compiler.
#  %.o:%.c means any file ending in .o depends on the same filename ending in .c.
#  $< includes the first prerequsit filename in the target line.
#  $@ includes the target
%.o: %.c
	@echo "  CC      $<"
	@$(CC) -c $< -o $@

#  To make module change from KERNELDIR to PWD directory for the source files.
$(MODULE):
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

#  To make clean change from KERNELDIR to PWD directory for the files to be cleaned.
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
