#pragma once

/*
 * Protocol identification catalogue.
 *
 * Four separate kinds of record are kept here on purpose:
 *   - Manufacturer:     the brand on the unit (a brand is NOT a protocol).
 *   - Protocol family:  a related set of IR encodings (e.g. MITSUBISHI_AC).
 *   - Protocol variant: one specific encoding that a driver implements.
 *   - Remote entry:     a remote model code that has been confirmed on a
 *                       real unit to use a given variant.
 *
 * Rules for adding records:
 *   - Only add a variant when this app has a working driver for it.
 *   - Only add a remote entry after it has been tested on a real unit
 *     with this app. Research notes (e.g. IRremoteESP8266 model lists)
 *     are leads for testing, not entries.
 *   - Mitsubishi Electric and Mitsubishi Heavy Industries are different
 *     manufacturers with unrelated protocols. Never merge them.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <infrared/signal/infrared_signal.h>

#define AC_IDENTIFY_MAX_MATCHES 4U

typedef struct {
    const char* name; /* Display name, upper case, <= 20 chars. */
} AcManufacturer;

typedef struct {
    const char* name; /* Family identifier, e.g. "MITSUBISHI_AC". */
} AcProtocolFamily;

typedef struct {
    size_t family;    /* Index into the family table. */
    const char* name; /* Variant within the family, e.g. "144-BIT". */
    bool supports_state_control; /* False means identification + learned replay only. */
    /* Returns true only when the signal is a valid frame for this variant
     * (signature, length and checksum all check out). */
    bool (*matches)(const InfraredSignal* signal);
} AcProtocolVariant;

typedef struct {
    size_t manufacturer; /* Index into the manufacturer table. */
    const char* remote_code;
    size_t variant; /* Index into the variant table. */
    const char* tested_unit; /* NULL means reference mapping, not app-tested yet. */
} AcRemoteEntry;

typedef enum {
    AcIdentifyNoMatch,
    AcIdentifyMatch,
    AcIdentifyAmbiguous,
} AcIdentifyResult;

size_t ac_manufacturer_count(void);
const AcManufacturer* ac_manufacturer_get(size_t index);

size_t ac_variant_count(void);
const AcProtocolVariant* ac_variant_get(size_t index);
const AcProtocolFamily* ac_variant_family(size_t variant_index);

/* Remote entries filtered by manufacturer, addressed by position in that
 * filtered list. */
size_t ac_remote_count_for_manufacturer(size_t manufacturer);
const AcRemoteEntry* ac_remote_get_for_manufacturer(size_t manufacturer, size_t position);

/* Runs every implemented matcher over a captured signal. Writes up to
 * AC_IDENTIFY_MAX_MATCHES variant indices to matches[]. */
AcIdentifyResult ac_identify_signal(
    const InfraredSignal* signal,
    size_t matches[AC_IDENTIFY_MAX_MATCHES],
    size_t* match_count);
