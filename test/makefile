platform := linux

binary_name := cliser_test

ifeq ($(platform),win32)
binary_name := $(binary_name).exe
endif


ifeq ($(platform),win32)
    libs := -lws2_32
    remove := del /Q /F 
else
    libs := -lpthread -lrt
    remove := rm -rf
endif



#compiler flags
cf := -Wall -I../src
cf += -DDEBUG


default: $(binary_name)
	@echo running $^...
	@./$^

$(binary_name): main.cpp
	@echo Building $@...
	@g++ -o $@ $(cf) $< $(libs)

clean:
	@$(remove) $(binary_name)
