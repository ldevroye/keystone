#ifndef EAPP_TEST_H
#define EAPP_TEST_H

#include "common.h"
#include "checkpoint.h"
#include "fault.h"
#include "crypto.h"

#include <stddef.h>
#include <limits.h>
#include <string.h>


#ifndef EAPP_TESTING
#define EAPP_TESTING 0
#endif

#ifndef EAPP_RUNS
#define EAPP_RUNS 50
#endif

#ifndef EAPP_LOGGING
#define EAPP_LOGGING 1
#endif

#ifndef EAPP_AVG_FAULT_TESTING
#define EAPP_AVG_FAULT_TESTING 0
#endif

#ifndef EAPP_ROUND_TRIP_TESTING
#define EAPP_ROUND_TRIP_TESTING 0
#endif

#ifndef EAPP_BREAK_EVEN_TESTING
#define EAPP_BREAK_EVEN_TESTING 0
#endif

#ifndef EAPP_BLOB_SIZE_TESTING
#define EAPP_BLOB_SIZE_TESTING 0
#endif


#ifndef EAPP_CYCLE_BREAKDOWN_TESTING
#define EAPP_CYCLE_BREAKDOWN_TESTING 0
#endif


// tests
void avg_fault_test();
int run_blob_size_test();
int run_round_trip_test();
int run_cycle_breakdown_test();
int run_break_even_test();
int run_eapp_tests();

#endif
