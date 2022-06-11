#ifndef __ENUM_H
#define __ENUM_H

#include <stdlib.h>
#include <stdint.h>

typedef uint16_t word_t;

typedef struct
{
  const char *name;
  size_t n_params;
} opcode_t;

opcode_t OPCODE_LIST[22] = {
  {.name = "HALT",.n_params = 0},
  {.name = "SET",.n_params = 2},
  {.name = "PUSH",.n_params = 1},
  {.name = "POP",.n_params = 1},
  {.name = "EQ",.n_params = 3},
  {.name = "GT",.n_params = 3},
  {.name = "JMP",.n_params = 1},
  {.name = "JT",.n_params = 2},
  {.name = "JF",.n_params = 2},
  {.name = "ADD",.n_params = 3},
  {.name = "MULT",.n_params = 3},
  {.name = "MOD",.n_params = 3},
  {.name = "AND",.n_params = 3},
  {.name = "OR",.n_params = 3},
  {.name = "NOT",.n_params = 2},
  {.name = "RMEM",.n_params = 2},
  {.name = "WMEM",.n_params = 2},
  {.name = "CALL",.n_params = 1},
  {.name = "RET",.n_params = 0},
  {.name = "OUT",.n_params = 1},
  {.name = "IN",.n_params = 1},
  {.name = "NOP",.n_params = 0},
};

#endif /* __ENUM_H */
