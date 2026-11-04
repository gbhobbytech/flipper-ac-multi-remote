#pragma once

#include <stdbool.h>
#include <infrared/signal/infrared_signal.h>

typedef enum {
    AcLgVariantLg,
    AcLgVariantLg2,
} AcLgVariant;

/* The two LG A/C formats have distinct header timings but share the 28-bit
 * state layout and checksum used by the public IRremoteESP8266 reference. */
bool ac_lg_matches_variant(const InfraredSignal* signal, AcLgVariant variant);
bool ac_lg_build_probe(AcLgVariant variant, bool power_on, InfraredSignal* signal);
