subdirs :=
subdirs += src
subdirs += tests




default all clean test:
	@$(MAKE) $(subdirs) target=$@

.PHONY: $(subdirs)
$(subdirs):
	@$(MAKE) -C $@ $(target)


