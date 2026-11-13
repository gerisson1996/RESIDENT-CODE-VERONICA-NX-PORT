/*
 * Nintendo Switch: the motion sensor of the controller in use (handheld, Pro Controller or the
 * right Joy-Con of a pair) for gyro aiming (pc/glue/tps_glue.c). Apart from the game's files:
 * <switch.h> clashes with the PS2 shim's names.
 */
#include <switch.h>
#include <math.h>

static HidSixAxisSensorHandle h_hand, h_full, h_dual[2];
static int started;
static u64 last_tick;
static float grav[3] = { 0.0f, 0.0f, -1.0f };   /* the low-passed acceleration: where down is */

static void start(void)
{
    started = 1;
    if (R_SUCCEEDED(hidGetSixAxisSensorHandles(&h_hand, 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld)))
        hidStartSixAxisSensor(h_hand);
    if (R_SUCCEEDED(hidGetSixAxisSensorHandles(&h_full, 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey)))
        hidStartSixAxisSensor(h_full);
    if (R_SUCCEEDED(hidGetSixAxisSensorHandles(h_dual, 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual))) {
        hidStartSixAxisSensor(h_dual[0]);
        hidStartSixAxisSensor(h_dual[1]);
    }
    last_tick = armGetSystemTick();
}

/* the turn since the last call, radians: *yaw about the vertical (gravity, so it works however the
   controller is held; > 0 turns left), *pitch about the controller's left-right axis (> 0 up).
   0: no sensor */
int nx_gyro_read(float* yaw, float* pitch)
{
    HidSixAxisSensorState s;
    HidSixAxisSensorHandle h;
    u32 style;
    u64 now;
    float dt, w[3], n;
    *yaw = *pitch = 0.0f;
    if (!started) start();
    now = armGetSystemTick();
    dt = (float)armTicksToNs(now - last_tick) * 1e-9f;
    last_tick = now;
    if (dt > 0.1f) dt = 0.1f;   /* (a pause, a load) */
    style = hidGetNpadStyleSet(HidNpadIdType_No1);
    if (style & HidNpadStyleTag_NpadFullKey) h = h_full;
    else if (style & HidNpadStyleTag_NpadJoyDual) h = h_dual[1];
    else if (hidGetNpadStyleSet(HidNpadIdType_Handheld) & HidNpadStyleTag_NpadHandheld) h = h_hand;
    else return 0;
    if (hidGetSixAxisSensorStates(h, &s, 1) < 1) return 0;
    /* angular velocity: rotations a second */
    w[0] = s.angular_velocity.x * 6.2831853f;
    w[1] = s.angular_velocity.y * 6.2831853f;
    w[2] = s.angular_velocity.z * 6.2831853f;
    n = sqrtf(s.acceleration.x * s.acceleration.x + s.acceleration.y * s.acceleration.y + s.acceleration.z * s.acceleration.z);
    if (n > 0.5f && n < 1.5f) {   /* (still enough to tell down) */
        grav[0] += (s.acceleration.x / n - grav[0]) * 0.05f;
        grav[1] += (s.acceleration.y / n - grav[1]) * 0.05f;
        grav[2] += (s.acceleration.z / n - grav[2]) * 0.05f;
    }
    n = sqrtf(grav[0] * grav[0] + grav[1] * grav[1] + grav[2] * grav[2]);
    if (n < 1e-3f) return 1;
    /* yaw: the turn about "up" (minus gravity) */
    *yaw = -(w[0] * grav[0] + w[1] * grav[1] + w[2] * grav[2]) / n * dt;
    *pitch = w[0] * dt;
    if (fabsf(*yaw) < 0.0004f) *yaw = 0.0f;   /* hand tremble */
    if (fabsf(*pitch) < 0.0004f) *pitch = 0.0f;
    return 1;
}
