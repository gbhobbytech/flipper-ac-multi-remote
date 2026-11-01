#include "ac_protocols.h"
#include "ac_mitsubishi.h"

/* ---- Matchers (wrap the drivers' own decoders; no extra signatures) ---- */

/* Fixed scratch space for matchers, so identification needs no heap and adds
 * little to the app's 4 KB stack. Matchers only run on the app thread, one at
 * a time, so a single shared buffer is safe. Not reentrant. */
static uint32_t matcher_prefix_scratch[AC_MITSUBISHI_PREFIX_MAX];

static bool match_mitsubishi_ac_144(const InfraredSignal* signal) {
    uint8_t frame[AC_MITSUBISHI_FRAME_BYTES];
    size_t prefix_count = 0;
    return ac_mitsubishi_decode_profile(signal, frame, matcher_prefix_scratch, &prefix_count);
}

/* Mitsubishi Heavy messages use 38 kHz pulse-distance timings, LSB-first.
 * The payload contains a fixed signature followed by inverted byte pairs. */
static bool match_mitsubishi_heavy(const InfraredSignal* signal, size_t byte_count) {
    if(!signal || !infrared_signal_is_raw(signal) ||
       (byte_count != 11U && byte_count != 19U)) {
        return false;
    }

    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    const size_t bit_count = byte_count * 8U;
    const size_t required = 2U + bit_count * 2U + 1U;
    if(!raw || !raw->timings || raw->timings_size < required) return false;

    for(size_t header = 0; header + required <= raw->timings_size && header <= 128U; header++) {
        const uint32_t header_mark = raw->timings[header];
        const uint32_t header_space = raw->timings[header + 1U];
        if(header_mark < 2800U || header_mark > 3500U ||
           header_space < 1400U || header_space > 1850U) {
            continue;
        }

        uint8_t frame[19] = {0};
        bool valid = true;
        for(size_t bit = 0; bit < bit_count; bit++) {
            const size_t mark_index = header + 2U + bit * 2U;
            const uint32_t mark = raw->timings[mark_index];
            const uint32_t space = raw->timings[mark_index + 1U];
            if(mark < 260U || mark > 520U) {
                valid = false;
                break;
            }

            if(space >= 250U && space <= 700U) {
                frame[bit / 8U] |= (uint8_t)(1U << (bit % 8U));
            } else if(space < 850U || space > 1550U) {
                valid = false;
                break;
            }
        }

        const uint32_t footer_mark = raw->timings[header + 2U + bit_count * 2U];
        if(!valid || footer_mark < 260U || footer_mark > 520U) continue;

        static const uint8_t signature[] = {0xADU, 0x51U, 0x3CU};
        if(frame[0] != signature[0] || frame[1] != signature[1] || frame[2] != signature[2]) {
            continue;
        }

        if(byte_count == 11U) {
            if(frame[3] != 0xD9U || frame[4] != 0x26U) continue;
        } else if(frame[3] != 0xE5U || frame[4] != 0x1AU) {
            continue;
        }

        for(size_t i = 3U; i + 1U < byte_count; i += 2U) {
            if((uint8_t)(frame[i] ^ frame[i + 1U]) != 0xFFU) {
                valid = false;
                break;
            }
        }
        if(valid) return true;
    }

    return false;
}

static bool match_mitsubishi_heavy_88(const InfraredSignal* signal) {
    return match_mitsubishi_heavy(signal, 11U);
}

static bool match_mitsubishi_heavy_152(const InfraredSignal* signal) {
    return match_mitsubishi_heavy(signal, 19U);
}

/* ---- Catalogue ---- */

enum {
    ManufacturerMitsubishiElectric,
    ManufacturerMitsubishiHeavy,
};

static const AcManufacturer manufacturers[] = {
    [ManufacturerMitsubishiElectric] = {.name = "MITSUBISHI ELECTRIC"},
    [ManufacturerMitsubishiHeavy] = {.name = "MITSUBISHI HEAVY IND."},
};

enum {
    FamilyMitsubishiAc,
    FamilyMitsubishiHeavy,
};

static const AcProtocolFamily families[] = {
    [FamilyMitsubishiAc] = {.name = "MITSUBISHI_AC"},
    [FamilyMitsubishiHeavy] = {.name = "MHI"},
};

enum {
    VariantMitsubishiAc144,
    VariantMitsubishiHeavy88,
    VariantMitsubishiHeavy152,
};

static const AcProtocolVariant variants[] = {
    [VariantMitsubishiAc144] =
        {
            .family = FamilyMitsubishiAc,
            .name = "144-BIT",
            .supports_state_control = true,
            .matches = match_mitsubishi_ac_144,
        },
    [VariantMitsubishiHeavy88] =
        {
            .family = FamilyMitsubishiHeavy,
            .name = "88-BIT",
            .supports_state_control = false,
            .matches = match_mitsubishi_heavy_88,
        },
    [VariantMitsubishiHeavy152] =
        {
            .family = FamilyMitsubishiHeavy,
            .name = "152-BIT",
            .supports_state_control = false,
            .matches = match_mitsubishi_heavy_152,
        },
};

/* MHI remote mappings are reference leads; capture validation is required. */
static const AcRemoteEntry remotes[] = {
    {
        .manufacturer = ManufacturerMitsubishiElectric,
        .remote_code = "SG17B",
        .variant = VariantMitsubishiAc144,
        .tested_unit = "MSZ-AG35VGD",
    },
    {
        .manufacturer = ManufacturerMitsubishiHeavy,
        .remote_code = "RKX502A001C",
        .variant = VariantMitsubishiHeavy88,
        .tested_unit = NULL,
    },
    {
        .manufacturer = ManufacturerMitsubishiHeavy,
        .remote_code = "RLA502A700B",
        .variant = VariantMitsubishiHeavy152,
        .tested_unit = NULL,
    },
};

#define COUNT_OF_TABLE(table) (sizeof(table) / sizeof((table)[0]))

size_t ac_manufacturer_count(void) {
    return COUNT_OF_TABLE(manufacturers);
}

const AcManufacturer* ac_manufacturer_get(size_t index) {
    return index < COUNT_OF_TABLE(manufacturers) ? &manufacturers[index] : NULL;
}

size_t ac_variant_count(void) {
    return COUNT_OF_TABLE(variants);
}

const AcProtocolVariant* ac_variant_get(size_t index) {
    return index < COUNT_OF_TABLE(variants) ? &variants[index] : NULL;
}

const AcProtocolFamily* ac_variant_family(size_t variant_index) {
    const AcProtocolVariant* variant = ac_variant_get(variant_index);
    if(!variant || variant->family >= COUNT_OF_TABLE(families)) return NULL;
    return &families[variant->family];
}

size_t ac_remote_count_for_manufacturer(size_t manufacturer) {
    size_t count = 0;
    for(size_t i = 0; i < COUNT_OF_TABLE(remotes); i++) {
        if(remotes[i].manufacturer == manufacturer) count++;
    }
    return count;
}

const AcRemoteEntry* ac_remote_get_for_manufacturer(size_t manufacturer, size_t position) {
    for(size_t i = 0; i < COUNT_OF_TABLE(remotes); i++) {
        if(remotes[i].manufacturer != manufacturer) continue;
        if(position == 0U) return &remotes[i];
        position--;
    }
    return NULL;
}

AcIdentifyResult ac_identify_signal(
    const InfraredSignal* signal,
    size_t matches[AC_IDENTIFY_MAX_MATCHES],
    size_t* match_count) {
    size_t count = 0;
    if(signal) {
        for(size_t i = 0; i < COUNT_OF_TABLE(variants); i++) {
            if(variants[i].matches && variants[i].matches(signal)) {
                if(count < AC_IDENTIFY_MAX_MATCHES) matches[count] = i;
                count++;
            }
        }
    }

    if(match_count) *match_count = count < AC_IDENTIFY_MAX_MATCHES ? count : AC_IDENTIFY_MAX_MATCHES;
    if(count == 0U) return AcIdentifyNoMatch;
    return count == 1U ? AcIdentifyMatch : AcIdentifyAmbiguous;
}
