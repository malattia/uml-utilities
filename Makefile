DIRS = redhat test transformiix mconsole moo uml_net uml_router

TARBALL_DIRS = redhat mconsole moo uml_net uml_router
TARBALL = uml_utilities_$(shell date +%Y%m%d).tar

tarball:
	for d in $(TARBALL_DIRS); do make -C $$d clean; done
	cd .. ; \
	dirs=`for d in $(TARBALL_DIRS); do echo tools/$$d; done`; \
	tar cf $(TARBALL) $$dirs ; \
	bzip2 -f $(TARBALL)

clean:
	for d in $(DIRS); do make -C $$d clean; done
