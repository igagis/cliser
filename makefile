include prorab.mk


$(eval $(prorab-build-subdirs))

#TODO: move to makefile in pkg-config dir
install::
#install pkg-config files
	@install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	@install pkg-config/*.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig
