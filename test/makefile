platform := linux

binary_name := cliser_test

ifeq ($(platform),win32)
binary_name := $(binary_name).exe
endif


srcs := main.cpp

cliserlib := ../src/libcliser.so

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


runtest: $(binary_name)
	@echo running $^...
	@LD_LIBRARY_PATH=../src ./$^


$(cliserlib):
	$(MAKE) -C ../src


$(binary_name): $(srcs) $(cliserlib)
	@echo Building $@...
	@g++ -o $@ $(cf) $(srcs) $(libs) -Wl,$(cliserlib)

clean:
	@$(remove) $(binary_name)
