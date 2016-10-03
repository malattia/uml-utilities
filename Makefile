TOPLEVELMAKEFILE := 1
TUNCTL  := $(shell [ -e /usr/include/linux/if_tun.h ] && echo tunctl)

SUBDIRS := lib jail jailtest humfsify mconsole moo port-helper $(TUNCTL) \
           uml_net uml_switch watchdog umlfs
UMLVER  := $(shell date +%Y%m%d)
TARBALL := uml_utilities_$(UMLVER).tar.bz2

include config.mk

all install clean:
	set -e ; for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done
	$(if $(filter clean,$@),rm -rf *~)
	$(if $(filter clean,$@),rm -rf rm -f uml_util.spec)


tarball : clean spec
	cd .. ;					\
	mv tools tools-$(UMLVER);		\
	tar cjf $(TARBALL) tools-$(UMLVER);	\
	mv tools-$(UMLVER) tools


spec:
	sed -e 's/__UMLVER__/$(UMLVER)/' < uml_util.spec.in > uml_util.spec


.PHONY: tarball spec
