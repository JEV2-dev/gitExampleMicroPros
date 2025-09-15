#include <stdio.h>
#include <inttypes.h>      
#include "esp_timer.h"     
#include "lookuptable.h"

// y(x) = -11.288519230192662 + 0.0148860811*x + 3.87457794e-06*x^2
static inline int32_t regression_func(int32_t x)
{
    const double intercept = -11.288519230192662;
    const double a1 = 1.48860811e-02;
    const double a2 = 3.87457794e-06;

    const double xd = (double)x;
    const double y  = intercept + a1*xd + a2*xd*xd;

    return (int32_t)(y + (y >= 0 ? 0.5 : -0.5));
}

void app_main(void)
{
    const int base = 1234;     
    const int N = 10000;

    volatile int32_t sink = 0;
    int64_t t0, t1;

    int32_t r_demo = regression_func(base);
    int32_t l_demo = lut_get(base);
    printf("Demo x=%d -> regression=%" PRId32 ", lut=%" PRId32 "\n",
           base, r_demo, l_demo);

    // --- Regresión ---
    t0 = esp_timer_get_time();
    for (int i = 0; i < N; ++i) {
        sink ^= regression_func(base + (i & 7));
    }
    t1 = esp_timer_get_time();
    int64_t dt_reg = t1 - t0;

    // --- LUT ---
    t0 = esp_timer_get_time();
    for (int i = 0; i < N; ++i) {
        sink ^= lut_get(base + (i & 7));
    }
    t1 = esp_timer_get_time();
    int64_t dt_lut = t1 - t0;

    printf("Regression: %" PRId64 " us / %d calls  => %.3f ns/call\n",
           dt_reg, N, (dt_reg * 1000.0) / N);
    printf("LUT:        %" PRId64 " us / %d calls  => %.3f ns/call\n",
           dt_lut, N, (dt_lut * 1000.0) / N);

    printf("Sink=%" PRId32 "\n", sink);
}


// metodo para correr el codigo en la esp32 desde terminal
// idf.py set-target esp32
// idf.py build
// idf.py -p /dev/ttyUSB0 flash monitor