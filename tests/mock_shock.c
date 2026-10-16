// Shared host mock for the accelerometer pulled in by shock.c.
// The tests set the raw sample vector and drive shock_task(); the direction
// evaluation in sdk/hal/shock.c is then tested against it (test_shock.c).
#include "lis2dh12.h"

s16 mock_accl_x = 0;
s16 mock_accl_y = 0;
s16 mock_accl_z = 0;

bool lis2dh12_init(void)
{
    return (true);
}

bool lis2dh12_start(void)
{
    return (true);
}

bool lis2dh12_read_data(s16 *x, s16 *y, s16 *z)
{
    *x = mock_accl_x;
    *y = mock_accl_y;
    *z = mock_accl_z;
    return (true);
}
