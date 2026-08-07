#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>


#include "checkpoint.h"
#include "fault.h"

#define OCALL_PRINT_BUFFER 1

struct fault_model;

void computation();
int test_run_enclave(unsigned long runs, struct fault_model* fault_model, int return_on_fault, int checkpoint_enabled, int resume_enabled, int fault_enabled);
int run_enclave(unsigned long runs, struct fault_model* fault_model);
void eapp_print(const char* str);
int format_value(char *buf, const int counter, const char* val);
int format_unsigned_value(char *buf, const unsigned long value, const char* val);
int format_float_value(char *buf, double value, const char *val);
uintptr_t read_stack_pointer(void);
uint64_t read_cycle_counter(void);
unsigned long advance_wrapped(unsigned long value);
uint64_t advance_wrapped(uint64_t value);

#endif