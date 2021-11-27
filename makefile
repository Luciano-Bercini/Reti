Source_dir = source
Bin_dir = bin

.PHONY: Project # Not a file that needs to be built.

Project:
	mkdir -p bin
	$(MAKE) -C $(Source_dir)

clean:
	$(MAKE) -C $(Source_dir) clean	
