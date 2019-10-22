#ifndef BFC_H
#define BFC_H
#include <stddef.h>

typedef enum {ADD, MOV, IN, OUT, LOOP_L, LOOP_R} bfc_ir_op;
typedef struct {bfc_ir_op op; int mag;} bfc_ir;

bfc_ir* gen_ir(const char* prog, size_t prsz, size_t* irsz);
char* gen_mc(const bfc_ir* ir, size_t irsz, size_t* prsz);
void write_elf(const char* prog, size_t prsz, const char* out_path);

#endif
