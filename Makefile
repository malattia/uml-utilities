SUBDIRS = mconsole moo uml_net uml_router
TARBALL = ../uml_utilities_$(shell date +%Y%m%d).tar

all clean install: 
	set -e ; for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done

tarball : clean
	tar cf $(TARBALL) . ; bzip2 -f $(TARBALL)
