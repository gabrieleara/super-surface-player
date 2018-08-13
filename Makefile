# TODO: define NDEBUG if compilation command is not a debug one

#---------------------------------------------------
# Paths
#---------------------------------------------------
vpath %.c src src/api
vpath %.h inc inc/api
vpath %.o obj

DIR_OBJ = obj
DIR_RES = res
DIR_DIS = dist
DIR_DEP = dep

#---------------------------------------------------
# Files
#---------------------------------------------------

# Global destinaion file
DEST = $(DIR_DIS)/super

# Object files related to source files
APIS_SRC = time.c ptask.c
MODULES_SRC = main.c audio.c video.c

SOURCES = $(APIS_SRC) $(MODULES_SRC)

OBJECTS = $(addprefix $(DIR_OBJ)/,$(SOURCES:.c=.o))

#---------------------------------------------------
# Flags
#---------------------------------------------------

# Phony tagets are always executed
# TODO: compile clean debug compile-debug super help
.PHONY: main directories compile clean debug compile-debug super help

# Compiler
CC = gcc

# Compilation options
CFLAGS += -Wall -pedantic

# Include paths
INCLUDES = -iquote inc
#-Iinc -I-

# Linking options
LDLIBS = -lpthread -lm -lrt
LDALLEGRO = `allegro-config --libs`

# Adds includes declaration in COMPILE.c rule, so that CFLAGS/CPPFLAGS are
# untouched and can be modified from outside
COMPILE.c += $(INCLUDES)

# Specifies where to put object files
# OUTPUT_OPTION = -o
# $(DIR_OBJ)/$@

# Make directory
MKDIR = mkdir -p

#---------------------------------------------------
# Phony Rules
#---------------------------------------------------

# Default make command
main: compile super

# Default compilation command
# FIXME: in this case resources are always copied from res to distribution folder
compile: CFLAGS += -D NDEBUG
compile: $(DEST)
	cp -R $(DIR_RES) $(DIR_DIS)

# Clean all make sub-products
clean:
	rm -rf $(DIR_OBJ)/*.o $(DIR_DIS)/*

# Create directories that are not synchronized using git
directories: $(DIR_OBJ) $(DIR_DIS)

# Compile in debug mode
debug: compile-debug

compile-debug: CFLAGS += -ggdb -O0
compile-debug: LDFLAGS += -ggdb
compile-debug: $(DEST)
	cp -R $(DIR_RES) $(DIR_DIS)


# Enabling Real-Time Scheduling (since superuser privileges are required)
super:
	sudo chown root $(DEST)
	sudo chgrp root $(DEST)
	sudo chmod a+s $(DEST)

#---------------------------------------------------
# File-specific Rules
#---------------------------------------------------

# Final executable file is created using this rule
$(DEST): $(OBJECTS) $(LOADLIBES) $(LDLIBS)
	$(LINK.o) $^ -o $@ $(LDALLEGRO)

# All directories are created using this rule
$(DIR_OBJ) $(DIR_DIS):
	$(MKDIR) $@

#---------------------------------------------------
# Generic Rules
#---------------------------------------------------

# Generic compilation rule
$(DIR_OBJ)/%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<


# Ugly rules used to auto-generate dependency rules
# Place a hyphen before include to suppress warning
DEPENDENCIES = $(addprefix $(DIR_DEP)/,$(SOURCES))

include $(subst .c,.d,$(DEPENDENCIES))

$(DIR_DEP)/%.d: %.c
	$(CC) -M $(CPPFLAGS) $(INCLUDES) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(DIR_OBJ)\/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
