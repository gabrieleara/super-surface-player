#---------------------------------------------------
# Paths
#---------------------------------------------------
vpath %.c src src/api
vpath %.h inc inc/api
vpath %.o obj

DIR_RES = res
DIR_SRC = src
DIR_OBJ = obj
DIR_DIS = dist
DIR_DEP = dep
DIR_INC = inc
DIR_API = inc/api
DIR_TEST= test_files
DIRECTORIES = $(DIR_OBJ) $(DIR_DIS) $(DIR_DEP) $(DIR_TEST) $(DIR_INC)

#---------------------------------------------------
# Files
#---------------------------------------------------

# Global destinaion file
DEST = $(DIR_DIS)/super

# Source files
APIS_SRC = time_utils.c ptask.c
MODULES_SRC = main.c audio.c video.c
SOURCES = $(APIS_SRC) $(MODULES_SRC)

# Header files
# APIS_HEADERS = $(addprefix $(DIR_API)/,$(APIS_SRC:.c=.h)) $(DIR_API)/std_emu.h
# MODULES_HEADERS = $(addprefix $(DIR_INC)/,$(MODULES_SRC:.c=.h))
# HEADERS = $(APIS_HEADERS) $(MODULES_HEADERS)

# Object files
OBJECTS = $(addprefix $(DIR_OBJ)/,$(SOURCES:.c=.o))

# Doxygen input file
DOXYFILE = docs/Doxyfile.in

# Extra option to prevent prints when generating Doxygen
DOXFLAGS = > /dev/null 2> /dev/null

#---------------------------------------------------
# Flags
#---------------------------------------------------

# Phony tagets are always executed
.PHONY: main directories compile clean clean-dep debug compile-release compile-debug super help docs docs-verbose

# Compiler
CC = gcc

# Compilation options
override CFLAGS += -Wall -Wextra -pedantic

# Include paths
INCLUDES = -iquote inc

# Linking options
LDLIBS = -lpthread -lm -lrt -lasound -lfftw3
LDALLEGRO = `allegro-config --libs`

# Adds includes declaration in COMPILE.c rule
COMPILE.c += $(INCLUDES)

# Specifies where to put object files
# OUTPUT_OPTION = -o
# $(DIR_OBJ)/$@

# Make directory
MKDIR = mkdir -p

# Doxygen command
DOXYGEN = doxygen

#---------------------------------------------------
# Phony Rules
#---------------------------------------------------

# Default make command
main: directories compile-release docs super

# Prints all accepted targets
help:
	@echo "Following is the list of all accepted targets from command line."
	@echo ""
	@echo "\tmain\t\tdefault command, equivalent to \`directories compile-release docs super\`"
	@echo ""
	@echo "\tclean\t\tclears the build tree"
	@echo "\tclean-dep\tcleans all the files in the dep folder"
	@echo "\tcompile-debug\tcompiles the program in debug mode"
	@echo "\tcompile-release\tcompiles the program in release mode"
	@echo ""
	@echo "\tdebug\t\tbuild in debug mode, equivalent to \`directories compile-debug\`"
	@echo "\t\t\tIt is preferred to call clean before."
	@echo ""
	@echo "\tdirectories \tcreates all directories needed to build the program"
	@echo "\tdocs\t\tgenerates doxygen documentation"
	@echo "\tdocs-verbose\tlike \`docs\`, but printing all doxygen information"
	@echo "\thelp\t\tprints this message"
	@echo ""
	@echo "\tsuper\t\tsets the owner of the compiled program as root and it sets also its"
	@echo "\t\t\tSUID bit, to use real-time scheduling even when running as normal user"
	@echo ""
	@echo "Other targets may be defined for internal use only."

# Default compilation command
# NOTICE: in this case resources are always copied from res to distribution folder
compile-release: override CFLAGS += -D NDEBUG
compile-release: compile

compile: $(DEST)
	cp -R $(DIR_RES) $(DIR_DIS)

# Clean all make sub-products
clean:
	rm -rf $(DIR_OBJ)/*.o $(DIR_DIS)/* $(DIR_SRC)/res

clean-dep:
	rm -f $(DIR_DEP)/*.d

# Create directories that are not synchronized using git
directories: $(DIRECTORIES)

# Compile in debug mode
debug: directories compile-debug

compile-debug: override CFLAGS += -ggdb -O0
compile-debug: LDFLAGS += -ggdb
compile-debug: compile
	cp -R $(DIR_RES) $(DIR_SRC)

# Generate documentation (by default suppresses any output)
docs:
	$(DOXYGEN) $(DOXYFILE) $(DOXFLAGS)

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
