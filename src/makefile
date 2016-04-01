include prorab.mk


this_name := cliser

this_soname := 0

this_srcs += cliser/ClientThread.cpp
this_srcs += cliser/Connection.cpp
this_srcs += cliser/ConnectionsThread.cpp
this_srcs += cliser/ServerThread.cpp


this_cxxflags += -Wall #enable all warnings
this_cxxflags += -Wno-comment #no warnings on nested comments
this_cxxflags += -funsigned-char #the 'char' type is unsigned
this_cxxflags += -Wnon-virtual-dtor #warn if base class has non-virtual destructor
this_cxxflags += -fno-operator-names #do not treat 'and', 'bitand','bitor','compl','not','or','xor' as keywords
this_cxxflags += -Werror #treat warnings as errors
this_cxxflags += -Wfatal-errors # stop on first error encountered
this_cxxflags += -fstrict-aliasing #strict aliasing (in order to comply with the c++ standard more strictly)
this_cxxflags += -g #include debugging symbols
this_cxxflags += -fPIC
this_cxxflags += -std=c++11
this_cxxflags += -O3

this_ldlibs += -lsetka -lnitki -lpogodi


ifeq ($(debug),true)
    this_cxxflags += -DDEBUG
endif




$(eval $(prorab-build-lib))
