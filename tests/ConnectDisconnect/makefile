include prorab.mk


this_name := tests


this_srcs := main.cpp



this_cflags += -Wall
this_cflags += -g
this_cflags += -std=c++11


this_ldlibs += -lcliser -lsetka -lnitki -pthread

this_ldflags += -L$(prorab_this_dir)../../src

ifeq ($(debug),true)
    this_cflags += -DDEBUG
endif



$(eval $(prorab-build-app))



define this_rules
test:: $(prorab_this_name)
	@(cd $(prorab_this_dir); LD_LIBRARY_PATH=../../src $$^)
endef
$(eval $(this_rules))


#dependency on cliser
$(prorab_this_name): $(abspath $(prorab_this_dir)../../src/libcliser$(prorab_lib_extension))

$(eval $(call prorab-include,$(prorab_this_dir)../../src/makefile))


















define TODO_efqwefqweghwha
platform := linux

binary_name := condisc_test

ifeq ($(platform),win32)
binary_name := $(binary_name).exe
endif


srcs := main.cpp

cliserlib := ../../src/libcliser

ifeq ($(platform),linux)
    cliserlib := $(cliserlib).so
endif
ifeq ($(platform),win32)
    cliserlib := $(cliserlib).a
endif


#compiler flags
ecf := -DDEBUG

cf := $(ecf)
cf += -Wall
cf += -g
cf += -I../../src


ifeq ($(platform),win32)
    libs := $(cliserlib) ../../../ting/src/libting.a -lws2_32
    cf += -I../../../ting/src
    remove := del /Q /F 
else
    libs := -lting -Wl,$(cliserlib)
    remove := rm -rf
endif


compiler := g++


default: $(binary_name)


test: $(binary_name)
	@echo running $^...
ifeq ($(platform),win32)
	@./$^
else
	@LD_LIBRARY_PATH=../../src ./$^
endif


.PHONY: $(cliserlib)
$(cliserlib):
	@$(MAKE) -C ../../src build_config=debug


$(binary_name): $(srcs) $(cliserlib)
	@echo Building $@...
	@$(compiler) -o $@ $(cf) $(srcs) $(libs)

clean:
	@$(remove) $(binary_name)


include ../../targets.mk
endef