SUBDIRS = mconsole moo port-helper uml_net uml_router
TARBALL = uml_utilities_$(shell date +%Y%m%d).tar

all install: 
	set -e ; for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done

tarball : clean
	cd .. ; tar cf $(TARBALL) tools ; bzip2 -f $(TARBALL)

clean:
	rm -rf *~
	set -e ; for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done
