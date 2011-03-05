platform := linux

binary_name := cliser_test

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
cf := -Wall -I../../src
cf += -DDEBUG


ifeq ($(platform),win32)
    libs := $(cliserlib) -lws2_32
    cf += -I../../../ting/src
    remove := del /Q /F 
else
    libs := -lpthread -lrt -Wl,$(cliserlib)
    remove := rm -rf
endif


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
	@g++ -o $@ $(cf) $(srcs) $(libs)

clean:
	@$(remove) $(binary_name)
