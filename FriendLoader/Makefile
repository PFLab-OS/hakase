ccflags-y += -I $(PWD)/../.docker_tmp/FriendLoader_include -D__FRIEND_LOADER__
obj-m := friend_loader.o
friend_loader-objs := main.o cpudevice.o cpu_hotplug.o call_interface.o version.o
KERN_VER ?= $(shell uname -r)
KERN_SRC ?= /lib/modules/$(KERN_VER)/build

.PHONY: all

all:
	$(MAKE) -C $(KERN_SRC) M=$(CURDIR) modules

clean:
	rm -f $(friend_loader-objs)
