#include "test.h"
#include "shock.h"

// shock_movement_direction() tells in which of 8 sectors of 45 degrees the
// vehicle is being accelerated in the horizontal plane, or
// SHOCK_DIRECTION_NONE when the horizontal acceleration stays below
// SHOCK_MOVE_TRESHOLD.  The vertical axis is found from gravity, so the same
// samples give the same answer whatever way the board is mounted.
//
// The samples are fed through shock_task() -> shock_evaluate(), i.e. through
// the real averaging and the real direction filter, so a "step" below is a
// change of the acceleration, not of the raw reading.
//
// Two things smooth the signal on the way and both matter when reading the
// expectations: the high-pass signal itself only exists while the acceleration
// is *changing* (it decays once the acceleration settles, after about a second),
// and the direction filter then averages SHOCK_MOVE_WINDOW samples of it.

#define _SECTOR_MAX  7

// a step well above the threshold, but still a plausible vehicle acceleration
// (~0.2 G; 1 G is about 16000 raw units)
#define _STEP  (SHOCK_MOVE_TRESHOLD * 4)
// a brief but hard lateral knock (~0.4 G) — a door slam, a pothole
#define _JOLT  (SHOCK_MOVE_TRESHOLD * 8)

// Durations are plain numbers of shock_task() periods (10 ms each) rather than
// multiples of SHOCK_MOVE_WINDOW, so that the tests state the physical duration
// of the manoeuvre and do not quietly shrink along with the window.  The window
// the module ships with is 16 samples, i.e. 160 ms; the *_filter_* tests below
// are the ones that would notice a change of it.
#define _SETTLE_SAMPLES  (2000) // 20 s, for the slow average (~256) to settle
#define _HOLD_SAMPLES    (64)   // 640 ms — well past anything the filter delays
#define _JOLT_SAMPLES    (6)    // 60 ms of knock
#define _GLITCH_SAMPLES  (8)    // 80 ms of glitch
#define _FILL_SAMPLES    (8)    // 80 ms — half of the shipped window

extern s16 mock_accl_x;
extern s16 mock_accl_y;
extern s16 mock_accl_z;

static void _feed(u32 samples, s16 x, s16 y, s16 z)
{
    mock_accl_x = x;
    mock_accl_y = y;
    mock_accl_z = z;

    while (samples--)
        shock_task();
}

TEST(shock_dir_none_when_still)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000); // gravity only, no movement

    ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE);
}

TEST(shock_dir_along_x)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   _STEP, 0, 16000);
    ASSERT_EQ(shock_movement_direction(), 0);

    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,  -_STEP, 0, 16000);
    ASSERT_EQ(shock_movement_direction(), 4); // the opposite sector
}

TEST(shock_dir_along_y)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   0, _STEP, 16000);
    ASSERT_EQ(shock_movement_direction(), 2);

    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   0, -_STEP, 16000);
    ASSERT_EQ(shock_movement_direction(), 6);
}

TEST(shock_dir_diagonal)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   _STEP, _STEP, 16000);
    ASSERT_EQ(shock_movement_direction(), 1);

    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,  -_STEP, -_STEP, 16000);
    ASSERT_EQ(shock_movement_direction(), 5);
}

TEST(shock_dir_below_treshold_is_none)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   SHOCK_MOVE_TRESHOLD / 4, 0, 16000);

    ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE);
}

// the reported sector must follow the board: with gravity along x it is the y
// and z axes that span the plane of movement
TEST(shock_dir_follows_board_orientation)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 16000, 0, 0);          // board rotated, gravity on x
    _feed(_HOLD_SAMPLES,   16000 + _STEP, 0, 0);  // acceleration along gravity
    ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE); // not movement

    _feed(_SETTLE_SAMPLES, 16000, 0, 0);
    _feed(_HOLD_SAMPLES,   16000, _STEP, 0);      // and now across the plane
    ASSERT_EQ(shock_movement_direction(), 0);     // y is the first horizontal axis
}

// the feature the dynamic store period is built on: a change of the direction
// is visible as a different sector
TEST(shock_dir_change_is_reported)
{
    u8 first;

    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   _STEP, 0, 16000);
    first = shock_movement_direction();
    ASSERT(first <= _SECTOR_MAX);

    _feed(_SETTLE_SAMPLES, 0, 0, 16000);         // go straight again
    _feed(_HOLD_SAMPLES,   0, _STEP, 16000);     // and turn by 90 degrees

    ASSERT(shock_movement_direction() != first);
    ASSERT(shock_movement_direction() != SHOCK_DIRECTION_NONE);
}

// The filter's job: a brief lateral knock on a stationary vehicle is not the
// vehicle moving sideways.  The level is deliberately high — it is the duration,
// not the level, that separates a bump from a turn, and the same knock held for
// _HOLD_SAMPLES is reported.
TEST(shock_dir_filter_rejects_brief_jolt)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);          // stationary
    _feed(_JOLT_SAMPLES,   0, _JOLT, 16000);
    ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE);

    _feed(_HOLD_SAMPLES,   0, _JOLT, 16000);      // the same knock, held
    ASSERT_EQ(shock_movement_direction(), 2);
}

// The price of the filter: a real acceleration is not reported the moment it
// starts, but one window of samples later.
TEST(shock_dir_filter_delays_a_step)
{
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);

    _feed(1, _STEP, 0, 16000);
    ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE); // not instantly

    _feed(_FILL_SAMPLES, _STEP, 0, 16000);        // 90 ms in total: still filling
    ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE);

    _feed(_HOLD_SAMPLES, _STEP, 0, 16000);
    ASSERT_EQ(shock_movement_direction(), 0);     // one window later it is there
}

// a short burst against the direction being driven in must not be reported as a
// turn
TEST(shock_dir_filter_rejects_glitch)
{
    u8 i;

    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   _STEP, 0, 16000);      // driving, direction is x+ (0)
    ASSERT_EQ(shock_movement_direction(), 0);

    for (i = 0; i < _GLITCH_SAMPLES; i++)
    {
        _feed(1, -_STEP, 0, 16000);
        ASSERT(shock_movement_direction() != 4);  // never the opposite sector
        ASSERT_EQ(shock_movement_direction(), 0);
    }

    // a real reversal, held well past the window, is reported
    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);
    _feed(_HOLD_SAMPLES,   -_STEP, 0, 16000);
    ASSERT_EQ(shock_movement_direction(), 4);
}

// Vibration is not movement in any direction: a lateral shake that reverses
// every 80 ms never becomes a reported sector.  Note that this does not prove
// the direction filter is at work - the movement signal is itself a high-pass
// average, so this holds with the filter disabled too.  It is here because the
// tracer relies on it to not store points while the vehicle is standing and
// shaking.
TEST(shock_dir_vibration_is_not_a_direction)
{
    u8 i;

    shock_reinit();
    _feed(_SETTLE_SAMPLES, 0, 0, 16000);

    for (i = 0; i < 8; i++)
    {
        _feed(_FILL_SAMPLES,  _STEP, 0, 16000);
        ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE);
        _feed(_FILL_SAMPLES, -_STEP, 0, 16000);
        ASSERT_EQ(shock_movement_direction(), SHOCK_DIRECTION_NONE);
    }
}
