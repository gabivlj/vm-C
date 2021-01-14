LANG_NAME := qwlang_test
DEPENDENCIES := ../src/*.c entry_point.c

execute: compile
	./$(LANG_NAME)
compile: $(DEPENDENCIES)
	clang $(DEPENDENCIES) -o $(LANG_NAME)