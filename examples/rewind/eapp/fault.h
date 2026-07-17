#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

// deterministic seed for repeatable fault schedules
#define SEED 0x6b656973746f6e68ULL
// set to 1 to derive a pseudo-random seed at runtime instead of using SEED
#define FAULT_RANDOMIZE_SEED 1
// average recurrence period in calls before a fault is allowed to fire again
#define PERIOD 10ULL

// splitmix64 constants are the standard mixer parameters used by the algorithm
// https://rosettacode.org/wiki/Pseudo-random_numbers/Splitmix64
// https://en.wikipedia.org/wiki/Linear_congruential_generator
#define SPLITMIX64_GAMMA 0x9e3779b97f4a7c15ULL
#define SPLITMIX64_MUL1 0xbf58476d1ce4e5b9ULL
#define SPLITMIX64_MUL2 0x94d049bb133111ebULL
#define SPLITMIX64_SHIFT_1 30
#define SPLITMIX64_SHIFT_2 27
#define SPLITMIX64_SHIFT_3 31

#define MODEL_DEFAULT \
{ \
    .seed = fault_default_seed(), \
    .step = 0, \
    .period = PERIOD, \
}

struct fault_model 
{
    uint64_t seed;
    uint64_t step;
    uint64_t period;
};

uint64_t fault_default_seed(void);
struct fault_model get_default_model();
int fault_should_trigger(struct fault_model *model);
void eapp_print(char* str); /// placeholder

#endif