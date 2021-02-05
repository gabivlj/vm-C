#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./src/qw_chunk.h"
#include "./src/qw_compiler.h"
#include "./src/qw_debug.h"
#include "./src/qw_object.h"
#include "./src/qw_vm.h"

#define EXIT_IF_ERR(expr, msg, code) \
  if (!(expr)) {                     \
    fprintf(stderr, msg);            \
    exit(code);                      \
  }

static void repl() {
  char line[1024];
  Chunk chunk;
  init_chunk(&chunk);
  for (;;) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    ValueArray arr = chunk.constants;
    init_chunk(&chunk);
    chunk.constants = arr;
    if (!compile(line, &chunk)) {
      continue;
    }
    if (!interpret(&chunk)) {
      continue;
    }
    print_value(*stack_vm());
    printf("\n");
  }
  free_chunk(&chunk);
}

static char* read_file(const char* path) {
  FILE* file = fopen(path, "rb");
  EXIT_IF_ERR(file != NULL, "couldn't find passed file", 74);
  fseek(file, 0L, SEEK_END);
  isize file_size = ftell(file);
  rewind(file);
  // + 1 because \0 ended
  char* buffer = (char*)malloc(file_size + 1);
  EXIT_IF_ERR(buffer != NULL, "file is too big", 74);
  isize bytes_read = fread(buffer, sizeof(char), file_size, file);
  EXIT_IF_ERR(bytes_read < file_size, "couldn't read all file", 74);
  buffer[bytes_read] = '\0';
  fclose(file);
  return buffer;
}

static void run_file(const char* path) {
  char* source = read_file(path);
  InterpretResult result = interpret_source(source);
  free(source);
  if (result == INTERPRET_COMPILER_ERROR) {
    exit(65);
  } else if (result == INTERPRET_RUNTIME_ERROR) {
    exit(70);
  }
}

int main(int argc, const char* argv[]) {
  init_vm();
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    run_file(argv[0]);
  } else {
    fprintf(stderr, "Usage: qw [path]\n");
    exit(64);
  }
  free_vm();
}