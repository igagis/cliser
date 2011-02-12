subdirs :=
subdirs += test




default all clean:
	@$(MAKE) $(subdirs) target=$@

.PHONY: $(subdirs)
$(subdirs):
	@$(MAKE) -C $@ $(target)


