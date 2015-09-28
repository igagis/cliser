include prorab.mk


this_name := cliser

this_soname := 0

this_srcs += cliser/ClientThread.cpp
this_srcs += cliser/Connection.cpp
this_srcs += cliser/ConnectionsThread.cpp
this_srcs += cliser/ServerThread.cpp


this_cflags += -Wall #enable all warnings
this_cflags += -Wno-comment #no warnings on nested comments
this_cflags += -funsigned-char #the 'char' type is unsigned
this_cflags += -Wnon-virtual-dtor #warn if base class has non-virtual destructor
this_cflags += -fno-operator-names #do not treat 'and', 'bitand','bitor','compl','not','or','xor' as keywords
this_cflags += -Werror #treat warnings as errors
this_cflags += -Wfatal-errors # stop on first error encountered
this_cflags += -fstrict-aliasing #strict aliasing (in order to comply with the c++ standard more strictly)
this_cflags += -g #include debugging symbols
this_cflags += -fPIC
this_cflags += -std=c++11

this_ldlibs += -lsetka -lnitki -lpogodi


ifeq ($(debug),true)
    this_cflags += -DDEBUG
endif




$(eval $(prorab-build-lib))
