#ifndef EAPP_TEST_H
#define EAPP_TEST_H

#include "common.h"

#ifndef EAPP_TESTING
#define EAPP_TESTING 0
#endif

#ifndef EAPP_RUNS
#define EAPP_RUNS 50
#endif

#ifndef EAPP_LOGGING
#define EAPP_LOGGING 1
#endif


// tests
void test_fault_avg();
int run_round_trip_test();
int run_cycle_breakdown_test();
int run_break_even_test();
int run_eapp_tests();

#endif
