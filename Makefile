#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := miot

EXTRA_COMPONENT_DIRS = ./protocol_examples_common

include $(IDF_PATH)/make/project.mk

