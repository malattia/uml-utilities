DIRS = redhat rpm test transformiix uml_net uml_router

clean:
	for d in $(DIRS); do make -C $$d clean; done
