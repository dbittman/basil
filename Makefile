DIRS=as sim example
export DIRS
all:
	@for i in $$DIRS; do \
		$(MAKE) -C $$i ;\
	done

clean:
	@for i in $$DIRS; do \
		$(MAKE) -C $$i clean ;\
	done

