gateway:
	arm-linux-gnueabihf-gcc main.c -o gateway -lpthread

test:
	@gcc test.c ./sources/yaml.c -o test -s -lfyaml -I./include -L./lib