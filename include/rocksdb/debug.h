//
// Created by xuhuai on 6/20/24.
//

#ifndef ROCKSDB_DEBUG_H
#define ROCKSDB_DEBUG_H

#include <execinfo.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utilities/omnicache.h"

static inline void print_trace(FILE* out) {
  void* array[10];
  char** strings;

  if (out == NULL) {
    out = stderr;
  }

  int size = backtrace(array, 10);
  strings = backtrace_symbols(array, size);
  fprintf(stderr, "size : %d\n", size);
  fprintf(stderr, "strings : %p\n", strings);
  if (strings != NULL) {
    fprintf(stderr, "Obtained %d stack frames.\n", size);
    for (int i = 0; i < size; i++) {
      fprintf(stderr, "%s\n", strings[i]);
    }
  }

  free(strings);
}

#define dbgprintf(fmt, ...)                    \
  do {                                         \
    static bool __enabled = envenabled("DEBUG"); \
    if (__enabled) {                             \
      fprintf(stderr, fmt, ##__VA_ARGS__);     \
    }                                          \
  } while (0)

#define dbglprintf(fmt, ...) \
  dbgprintf("%s:%d %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif  // ROCKSDB_DEBUG_H
