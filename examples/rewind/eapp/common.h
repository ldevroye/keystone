#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "checkpoint.h"
#include "fault.h"

#define OCALL_PRINT_BUFFER 1

void computation();
int run_enclave(unsigned long runs, struct fault_model fault_model, int return_on_fault);
void eapp_print(const char* str);
int format_value(char *buf, const int counter, const char* val);
int format_unsigned_value(char *buf, const unsigned long value, const char* val);
int format_float_value(char *buf, double value, const char *val);
uintptr_t read_stack_pointer(void);

#endif