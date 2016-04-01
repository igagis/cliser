include prorab.mk


this_name := tests


this_srcs := main.cpp



this_cxxflags += -Wall
this_cxxflags += -g
this_cxxflags += -std=c++11


this_ldlibs += -lcliser -lsetka -lnitki -pthread

this_ldflags += -L$(prorab_this_dir)../../src

ifeq ($(debug),true)
    this_cxxflags += -DDEBUG
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
