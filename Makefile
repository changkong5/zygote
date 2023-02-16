CC = gcc

CFLAGS := -g
LDFLAGS := -lm -lpthread
 
Q = @
ROOT := $(shell pwd)
INCLUDE := -I$(ROOT)/include

SOURCE = $(shell find -type f -name "*.c")
#SOURCE = $(shell find $(ROOT) -type f -name "*.c")

# SOURCES= $(wildcard *.c)  
# FILES =$(notdir $(SOURCES))
# OBJS = $(patsubst %.c，%.o，$(SOURCES))

#OBJECT = $(SOURCE:.c=.o)
OBJECT = $(patsubst %.c,%.o,$(SOURCE))

DIRECTORY1 = $(foreach f,$(SOURCE), $(dir $(f)))

DIRECTORY = $(patsubst %/,%, $(DIRECTORY1))


all:main

.PHONY: clean

main: $(OBJECT)
#	@echo "==SOURCE== $(SOURCE)"
#	@echo "==OBJECT== $(OBJECT)"
#	@echo "==DIRECTORY== $(DIRECTORY)"
	$(Q)$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)


clean:
	@rm -f $(OBJECT) main


%.o: %.c
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@ $(LDFLAGS)


# usr:
#	@for n in $(USR_SUB_DIR); do $(MAKE) -C $$n ; done
	
# clean11:
# 	@for n in $(USR_SUB_DIR); do $(MAKE) -C $$n clean; done

