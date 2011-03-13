subdirs :=
subdirs += Simple
subdirs += ConnectDisconnect



default all clean test:
	@$(MAKE) $(subdirs) target=$@

.PHONY: $(subdirs)
$(subdirs):
	@$(MAKE) -C $@ $(target)


