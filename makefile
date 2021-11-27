Source_dir = source

.PHONY: Project # Not a file that needs to be built.

Project:
	$(MAKE) -C $(Source_dir)

clean:
	$(MAKE) -C $(Source_dir) clean	
