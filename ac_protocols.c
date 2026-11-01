#include "ac_protocols.h"
#include "ac_mitsubishi.h"

#include <furi.h>

/* ---- Matchers (wrap the drivers' own decoders; no extra signatures) ---- */

static bool match_mitsubishi_ac_144(const InfraredSignal* signal) {
    uint8_t frame[AC_MITSUBISHI_FRAME_BYTES];
    size_t prefix_count = 0;
    uint32_t* prefix = malloc(AC_MITSUBISHI_PREFIX_MAX * sizeof(uint32_t));
    const bool matched = ac_mitsubishi_decode_profile(signal, frame, prefix, &prefix_count);
    free(prefix);
    return matched;
}

/* ---- Catalogue ---- */

enum {
    ManufacturerMitsubishiElectric,
};

static const AcManufacturer manufacturers[] = {
    [ManufacturerMitsubishiElectric] = {.name = "MITSUBISHI ELECTRIC"},
};

enum {
    FamilyMitsubishiAc,
};

static const AcProtocolFamily families[] = {
    [FamilyMitsubishiAc] = {.name = "MITSUBISHI_AC"},
};

enum {
    VariantMitsubishiAc144,
};

static const AcProtocolVariant variants[] = {
    [VariantMitsubishiAc144] =
        {
            .family = FamilyMitsubishiAc,
            .name = "144-BIT",
            .matches = match_mitsubishi_ac_144,
        },
};

/* Tested on real units with this app only. */
static const AcRemoteEntry remotes[] = {
    {
        .manufacturer = ManufacturerMitsubishiElectric,
        .remote_code = "SG17B",
        .variant = VariantMitsubishiAc144,
        .tested_unit = "MSZ-AP35VGD",
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
