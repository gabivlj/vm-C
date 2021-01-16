#ifndef qw_common_h
#define qw_common_h
#include <string.h>
// #define DEBUG_PRINT_CODE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t isize;

#define todo()                                           \
  fprintf(stderr, "Unimplemented -- Exiting program\n"); \
  exit(1);

#define assert_or_exit(expr) \
  if (!(expr)) exit(1);

#endif