#---------------------------------------------------
# Target file to be compiled by default
#---------------------------------------------------
MAIN = main

#---------------------------------------------------
# CC is the compiler to be used
#---------------------------------------------------
CC = gcc

#---------------------------------------------------
# CFLAGS are the options passed to the compiler
#---------------------------------------------------
CFLAGS = -Wall -lpthread -lm -lrt

#---------------------------------------------------
# LIBS are the external libraries to be used
#---------------------------------------------------
LIBS = `allegro-config --libs`

#---------------------------------------------------
# FOLDERS
#---------------------------------------------------
FOLDER_SRC = src
FOLDER_API = $(FOLDER_SRC)/api
FOLDER_LIB = $(FOLDER_SRC)/lib
FOLDER_OBJ = obj
FOLDER_DIS = dist

#---------------------------------------------------
# CUSTOM_LIBS are extra files that need to be compiled only once
#---------------------------------------------------

# TODO: add CUSTOM_LIBS
CUSTOM_LIBS =

# Object files

CUSTOM_LIBS_OBJ =

#---------------------------------------------------
# API are modules that contain only functions
#---------------------------------------------------

API_PTASK = $(FOLDER_API)/ptask.c
API_TIME = $(FOLDER_API)/time.c

APIS = $(API_PTASK) $(API_TIME)

# Object files

API_PTASK_O = $(FOLDER_OBJ)/ptask.o
API_TIME_O = $(FOLDER_OBJ)/time.o

APIS_OBJ = $(API_PTASK_O) $(API_TIME_O)

#---------------------------------------------------
# MODULES are the source files to be compiled into OBJS
#---------------------------------------------------
MODULE_MAIN = $(FOLDER_SRC)/main.c

MODULES = $(MODULE_MAIN)

# Object files

MODULE_MAIN_O = $(FOLDER_OBJ)/main.o

MODULES_OBJ = $(MODULE_MAIN_O)


#---------------------------------------------------
# Dependencies
#---------------------------------------------------
DEST = $(FOLDER_DIS)/super

$(MAIN): compile super

compile: $(MODULES_OBJ) $(APIS_OBJ) $(CUSTOM_LIBS)
	$(CC) -o $(DEST) $(MODULES_OBJ) $(APIS_OBJ) $(CUSTOM_LIBS) $(LIBS) $(CFLAGS)

#---------------------------------------------------
# Compilers for MODULES source files
#---------------------------------------------------

$(MODULE_MAIN_O): $(MODULE_MAIN)
	$(CC) -o $(MODULE_MAIN_O) -c $(MODULE_MAIN) $(CFLAGS)

#---------------------------------------------------
# Compilers for APIS source files
#---------------------------------------------------

$(API_PTASK_O): $(API_PTASK)
	$(CC) -o $(API_PTASK_O) -c $(API_PTASK) $(CFLAGS)


$(API_TIME_O): $(API_TIME)
	$(CC) -o $(API_TIME_O) -c $(API_TIME) $(CFLAGS)

#---------------------------------------------------
# Commands that can be specified inline
#---------------------------------------------------
clean:
	rm -rf $(APIS_OBJ) $(MODULES_OBJ) $(DEST)

cleanlibs:
	rm -rf $(CUSTOM_LIBS)

#------------------------------------------------------------
#                        Debug stuff
#------------------------------------------------------------
debug: compile-debug super

compile-debug: #debug-libs
	$(CC) -o $(DEST) $(MODULES) $(APIS) $(CUSTOM_LIBS) $(LIBS) $(CFLAGS) -ggdb -D SUPER_DEBUG


#------------------------------------------------------------
#                Enabling Real-Time Scheduling
#------------------------------------------------------------
super:
	sudo chown root $(DEST)
	sudo chgrp root $(DEST)
	sudo chmod a+s $(DEST)

