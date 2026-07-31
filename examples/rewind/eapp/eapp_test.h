#ifndef EAPP_TEST_H
#define EAPP_TEST_H

// tests
void test_fault_avg();
int run_round_trip_test();
int run_eapp_tests();

// placeholders
void eapp_print(const char* str);
int format_float_value(char *buf, double value, const char *val);
int format_unsigned_value(char *buf, const unsigned long value, const char* val);
int format_value(char *buf, const int counter, const char* val);

#endif
