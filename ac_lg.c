#include "ac_lg.h"

#include <stddef.h>
#include <stdint.h>
#include <infrared/signal/infrared_signal.h>

/* LG A/C timings, in microseconds. Reference framing: IRremoteESP8266
 * ir_LG.cpp. The wizard offers one neutral Cool 24 C / Auto probe per format. */
#define LG_HEADER_MARK 8500U
#define LG_HEADER_SPACE 4250U
#define LG2_HEADER_MARK 3200U
#define LG2_HEADER_SPACE 9900U
#define LG_BIT_MARK 550U
#define LG2_BIT_MARK 480U
#define LG_ONE_SPACE 1600U
#define LG_ZERO_SPACE 550U
#define LG_FOOTER_GAP 39750U
#define LG_FRAME_BITS 28U
#define LG_RAW_TIMINGS (2U + (LG_FRAME_BITS * 2U) + 2U)
#define LG_FRAME_ON_COOL24_AUTO 0x880095EU
#define LG_FRAME_OFF 0x88C0051U
#define LG_SIGNATURE 0x88U

static bool duration_in_range(uint32_t measured, uint32_t expected) {
    const uint32_t tolerance = expected * 30U / 100U;
    return measured >= expected - tolerance && measured <= expected + tolerance;
}

static uint8_t checksum(uint32_t frame) {
    frame >>= 4U;
    uint8_t sum = 0U;
    for(size_t i = 0U; i < 4U; i++) {
        sum = (uint8_t)(sum + (frame & 0x0FU));
        frame >>= 4U;
    }
    return (uint8_t)(sum & 0x0FU);
}

static bool valid_frame(uint32_t frame) {
    return (uint8_t)(frame >> 20U) == LG_SIGNATURE &&
           (frame & 0x0FU) == checksum(frame);
}

static bool protocol_timing(
    AcLgVariant variant,
    uint32_t* header_mark,
    uint32_t* header_space,
    uint32_t* bit_mark) {
    if(!header_mark || !header_space || !bit_mark) return false;
    switch(variant) {
    case AcLgVariantLg:
        *header_mark = LG_HEADER_MARK;
        *header_space = LG_HEADER_SPACE;
        *bit_mark = LG_BIT_MARK;
        return true;
    case AcLgVariantLg2:
        *header_mark = LG2_HEADER_MARK;
        *header_space = LG2_HEADER_SPACE;
        *bit_mark = LG2_BIT_MARK;
        return true;
    default:
        return false;
    }
}

static bool decode_frame(
    const InfraredRawSignal* raw,
    AcLgVariant variant,
    uint32_t* frame_out) {
    if(!raw || !raw->timings || !frame_out) return false;

    uint32_t header_mark = 0U;
    uint32_t header_space = 0U;
    uint32_t bit_mark = 0U;
    if(!protocol_timing(variant, &header_mark, &header_space, &bit_mark)) return false;

    const size_t required = 2U + (LG_FRAME_BITS * 2U) + 1U;
    if(raw->timings_size < required) return false;

    for(size_t start = 0U; start + required <= raw->timings_size; start++) {
        if(!duration_in_range(raw->timings[start], header_mark) ||
           !duration_in_range(raw->timings[start + 1U], header_space)) {
            continue;
        }

        uint32_t frame = 0U;
        size_t index = start + 2U;
        bool valid = true;
        for(int bit = (int)LG_FRAME_BITS - 1; bit >= 0; bit--) {
            if(!duration_in_range(raw->timings[index], bit_mark)) {
                valid = false;
                break;
            }
            const uint32_t space = raw->timings[index + 1U];
            if(duration_in_range(space, LG_ONE_SPACE)) {
                frame |= 1UL << bit;
            } else if(!duration_in_range(space, LG_ZERO_SPACE)) {
                valid = false;
                break;
            }
            index += 2U;
        }
        if(valid && duration_in_range(raw->timings[index], bit_mark) &&
           valid_frame(frame)) {
            *frame_out = frame;
            return true;
        }
    }
    return false;
}

bool ac_lg_matches_variant(const InfraredSignal* signal, AcLgVariant variant) {
    if(!signal || !infrared_signal_is_raw(signal)) return false;
    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    uint32_t frame = 0U;
    return decode_frame(raw, variant, &frame);
}

bool ac_lg_build_probe(AcLgVariant variant, bool power_on, InfraredSignal* signal) {
    if(!signal) return false;

    uint32_t header_mark = 0U;
    uint32_t header_space = 0U;
    uint32_t bit_mark = 0U;
    if(!protocol_timing(variant, &header_mark, &header_space, &bit_mark)) return false;

    const uint32_t frame = power_on ? LG_FRAME_ON_COOL24_AUTO : LG_FRAME_OFF;
    if(!valid_frame(frame)) return false;

    uint32_t timings[LG_RAW_TIMINGS];
    size_t count = 0U;
    timings[count++] = header_mark;
    timings[count++] = header_space;
    for(int bit = (int)LG_FRAME_BITS - 1; bit >= 0; bit--) {
        timings[count++] = bit_mark;
        timings[count++] = (frame & (1UL << bit)) ? LG_ONE_SPACE : LG_ZERO_SPACE;
    }
    timings[count++] = bit_mark;
    timings[count++] = LG_FOOTER_GAP;

    infrared_signal_set_raw_signal(
        signal, timings, count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
    return true;
}
