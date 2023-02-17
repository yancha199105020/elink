Q := @
OUT_DIR := output
CC= arm-linux-gnueabihf-gcc
SRC_INC += sources
all: prepare 


prepare:
	$(Q)mkdir -p $(OUT_DIR)

clean:
	@rm -rf $(OUT_DIR)
test:
	@gcc test.c ./sources/yaml.c -o test -s -lfyaml -I./include -L./lib