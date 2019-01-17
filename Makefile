#---------------------------------------------------
# Paths
#---------------------------------------------------
vpath %.c src src/api
vpath %.h inc inc/api
vpath %.o obj

DIR_RES = res

DIR_OBJ = obj
DIR_DIS = dist
DIR_DEP = dep
DIR_TEST= test_files
DIRECTORIES = $(DIR_OBJ) $(DIR_DIS) $(DIR_DEP) $(DIR_TEST)

#---------------------------------------------------
# Files
#---------------------------------------------------

# Global destinaion file
DEST = $(DIR_DIS)/super

# Object files related to source files
APIS_SRC = time_utils.c ptask.c
MODULES_SRC = main.c audio.c video.c

SOURCES = $(APIS_SRC) $(MODULES_SRC)
OBJECTS = $(addprefix $(DIR_OBJ)/,$(SOURCES:.c=.o))

DOXFLAGS = > /dev/null 2> /dev/null

#---------------------------------------------------
# Flags
#---------------------------------------------------

# Phony tagets are always executed
# IDEA: The help command should print what are the accepted make commands
.PHONY: main directories compile clean clean-dep debug compile-debug super help docs docs-verbose

# Compiler
CC = gcc

# Compilation options
CFLAGS += -Wall -Wextra -pedantic

# Include paths
INCLUDES = -iquote inc

# Linking options
LDLIBS = -lpthread -lm -lrt -lasound -lfftw3
LDALLEGRO = `allegro-config --libs`

# Adds includes declaration in COMPILE.c rule, so that CFLAGS/CPPFLAGS are
# untouched and can be modified from outside
COMPILE.c += $(INCLUDES)

# Specifies where to put object files
# OUTPUT_OPTION = -o
# $(DIR_OBJ)/$@

# Make directory
MKDIR = mkdir -p

# Doxygen command
DOXYGEN = doxygen docs/Doxyfile.in

#---------------------------------------------------
# Phony Rules
#---------------------------------------------------

# Default make command
main: directories compile docs super

# Default compilation command
# NOTICE: in this case resources are always copied from res to distribution folder
compile: CFLAGS += -D NDEBUG
compile: $(DEST)
	cp -R $(DIR_RES) $(DIR_DIS)

# Clean all make sub-products
clean:
	rm -rf $(DIR_OBJ)/*.o $(DIR_DIS)/*

clean-dep:
	rm -f $(DIR_DEP)/*.d

# Create directories that are not synchronized using git
directories: $(DIRECTORIES)

# Compile in debug mode
debug: directories compile-debug

compile-debug: CFLAGS += -ggdb -O0
compile-debug: LDFLAGS += -ggdb
compile-debug: $(DEST)
	cp -R $(DIR_RES) $(DIR_DIS)

# Generate documentation (by default suppresses any output)
docs:
	$(DOXYGEN) $(DOXFLAGS)

# Generate documentation, but with detailed output
docs-verbose: DOXFLAGS =
docs-verbose: docs

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
	$(LINK.o) $(OUTPUT_OPTION) $^ $(LDALLEGRO)

# All directories are created using this rule
$(DIRECTORIES):
	$(MKDIR) $@

#---------------------------------------------------
# Generic Rules
#---------------------------------------------------

# Generic compilation rule
$(DIR_OBJ)/%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<


# Horrible rules used to auto-generate dependency rules
# The hyphen before `include` is used to suppress unnecessary warnings
DEPENDENCIES = $(addprefix $(DIR_DEP)/,$(SOURCES))

-include $(subst .c,.d,$(DEPENDENCIES))

$(DIR_DEP)/%.d: %.c
	@echo "Generating $@ ..."
	@$(MKDIR) $(DIR_DEP)
	@$(CC) -M $(CPPFLAGS) $(INCLUDES) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(DIR_OBJ)\/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
