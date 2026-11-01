#include "ac_protocols.h"
#include "ac_mitsubishi.h"
#include "ac_mhi.h"

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

static bool match_mitsubishi_heavy_variant(
    const InfraredSignal* signal,
    AcMhiVariant expected_variant) {
    AcMhiVariant detected_variant = AcMhiVariantUnknown;
    uint8_t state[AC_MHI_MAX_STATE_BYTES];
    return ac_mhi_decode_signal(signal, &detected_variant, state) &&
           detected_variant == expected_variant;
}

static bool match_mitsubishi_heavy_88(const InfraredSignal* signal) {
    return match_mitsubishi_heavy_variant(signal, AcMhiVariant88);
}

static bool match_mitsubishi_heavy_152(const InfraredSignal* signal) {
    return match_mitsubishi_heavy_variant(signal, AcMhiVariant152);
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
