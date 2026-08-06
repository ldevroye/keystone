#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

void eapp_print(const char* str);
int format_value(char *buf, const int counter, const char* val);
int format_unsigned_value(char *buf, const unsigned long value, const char* val);
int format_float_value(char *buf, double value, const char *val);
uintptr_t read_stack_pointer(void);

#endif