#include <stddef.h>
#include <string.h>

#include "app/eapp_utils.h"
#include "checkpoint.h"
#include "eapp_test.h"
#include "fault.h"
#include <alloca.h>
#include <stddef.h>
#include <string.h>

#include "app/eapp_utils.h"
#include "checkpoint.h"
#include "eapp_test.h"
#include "fault.h"

int format_value(char *buf, const int counter, const char* val)
{
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);
    int pos = pfx_len;

    unsigned int u;
    if (counter == 0)
    {
        buf[pos++] = '0';
    }
    else
    {
        int neg = 0;
        if (counter < 0)
        {
            neg = 1;
            u = (unsigned)(-counter);
        }
        else
        {
            u = (unsigned)counter;
        }

        char rev[16];
        int ri = 0;
        while (u)
        {
            rev[ri++] = (char)('0' + (u % 10));
            u /= 10;
        }

        if (neg)
        {
            buf[pos++] = '-';
        }

        for (int j = ri - 1; j >= 0; --j)
        {
            buf[pos++] = rev[j];
        }
    }

    buf[pos] = '\0';
    return pos;
}

int format_unsigned_value(char *buf, const unsigned long value, const char* val)
{
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);

    int pos = pfx_len;
    unsigned long u = value;

    if (u == 0)
    {
        buf[pos++] = '0';
    }
    else
    {
        char rev[32];
        int ri = 0;
        while (u)
        {
            rev[ri++] = (char)('0' + (u % 10));
            u /= 10;
        }

        for (int j = ri - 1; j >= 0; --j)
        {
            buf[pos++] = rev[j];
        }
    }

    buf[pos] = '\0';
    return pos;
}

int format_float_value(char *buf, double value, const char *val)
{
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);

    int pos = pfx_len;
    if (value < 0.0)
    {
        buf[pos++] = '-';
        value = -value;
    }

    unsigned long whole = (unsigned long)value;
    double fraction = value - (double)whole;
    unsigned long scaled = (unsigned long)(fraction * 100.0 + 0.5);

    char rev[32];
    int ri = 0;
    do
    {
        rev[ri++] = (char)('0' + (whole % 10));
        whole /= 10;
    } while (whole != 0);

    for (int j = ri - 1; j >= 0; --j)
    {
        buf[pos++] = rev[j];
    }

    buf[pos++] = '.';
    buf[pos++] = (char)('0' + (scaled / 10));
    buf[pos++] = (char)('0' + (scaled % 10));
    buf[pos] = '\0';
    return pos;
}

void test_fault_avg()
{
    struct fault_model fault_model = MODEL_DEFAULT;
    int counter = 0;
    int nb_faults = 0;

    for (; counter < 10000; counter++)
    {
        if (fault_should_trigger(&fault_model))
        {
            nb_faults++;
        }
    }

    char formated_counter[32], formated_fault[32], formated_rate[32];
    double rate = nb_faults == 0 ? 0.0 : (double)counter / (double)nb_faults;
    format_value(formated_counter, counter, "counter");
    format_value(formated_fault, nb_faults, "nb faults");
    format_float_value(formated_rate, rate, "rate");
    eapp_print(formated_counter);
    eapp_print(formated_fault);
    eapp_print(formated_rate);
}

int run_round_trip_test()
{
    struct rewind_state expected_state = {1, 2, 3};
    struct rewind_state restored_state;
    struct checkpoint checkpoint;
    struct sealed_checkpoint blob;
    const size_t state_offset = STACK_SNAPSHOT_SIZE - sizeof(expected_state);

    memset(&checkpoint, 0, sizeof(checkpoint));
    memset(&restored_state, 0, sizeof(restored_state));
    memset(&blob, 0, sizeof(blob));

    checkpoint.checkpoint_seq = 42;
    memset(checkpoint.stack_data, 0xA5, sizeof(checkpoint.stack_data));
    memcpy(checkpoint.stack_data + state_offset, &expected_state, sizeof(expected_state));

    if (seal_checkpoint_blob(&blob, &checkpoint) != 0)
    {
        eapp_print("round-trip test sealing failed");
        return -1;
    }

    if (open_checkpoint_blob(&checkpoint, &blob) != 0)
    {
        eapp_print("round-trip test opening failed");
        return -1;
    }

    memcpy(&restored_state,
           checkpoint.stack_data + (STACK_SNAPSHOT_SIZE - sizeof(restored_state)),
           sizeof(restored_state));

    if (restored_state.a != expected_state.a ||
        restored_state.b != expected_state.b ||
        restored_state.counter != expected_state.counter)
    {
        eapp_print("round-trip test validation failed");
        return -1;
    }

    eapp_print("round-trip test passed");
    return 0;
}

int run_eapp_tests()
{
    test_fault_avg();

    if (run_round_trip_test() != 0)
    {
        eapp_print("failed run round-trip");
    }

    return 0;
}
