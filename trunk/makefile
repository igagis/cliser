subdirs :=
subdirs += src
subdirs += test




default all clean runtest:
	@$(MAKE) $(subdirs) target=$@

.PHONY: $(subdirs)
$(subdirs):
	@$(MAKE) -C $@ $(target)


