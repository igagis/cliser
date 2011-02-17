platform := linux

binary_name := cliser_test

ifeq ($(platform),win32)
binary_name := $(binary_name).exe
endif


srcs := main.cpp
srcs += ../src/cliser/srv/Connection.cpp
srcs += ../src/cliser/srv/ConnectionsThread.cpp
srcs += ../src/cliser/srv/Server.cpp
srcs += ../src/cliser/srv/TCPAcceptorThread.cpp
srcs += ../src/cliser/srv/ThreadsKillerThread.cpp
srcs += ../src/cliser/util/util.cpp


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

$(binary_name): $(srcs)
	@echo Building $@...
	@g++ -o $@ $(cf) $^ $(libs)

clean:
	@$(remove) $(binary_name)
