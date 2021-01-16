LANG_NAME := qwlang
DEPENDENCIES := ./src/*.c main.c
## -Wimplicit-function-declaration
execute: compile
	./$(LANG_NAME)
compile: $(DEPENDENCIES)
	clang $(DEPENDENCIES) -o $(LANG_NAME) 