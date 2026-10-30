#include <furi.h>
#include <stdio.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <infrared/worker/infrared_worker.h>
#include <lib/infrared/signal/infrared_signal.h>
#include "ac_mitsubishi.h"

#define EVENT_QUEUE_SIZE 12U
#define MAX_PROFILES 8U
#define OK_DOUBLE_CLICK_MS 300U
#define PROFILE_DIRECTORY EXT_PATH("apps_data/gbhobbytech_ac_multi_remote")
#define PROFILE_PATH_FORMAT PROFILE_DIRECTORY "/AC_Unit_%02u.ir"
#define MITSUBISHI_TEMP_MIN 16U
#define MITSUBISHI_TEMP_MAX 31U
static const uint8_t mitsubishi_fan_codes[] = {0U, 1U, 2U, 3U, 4U};

typedef enum {
    KeypadModeControl,
    KeypadModeSelectUnit,
} KeypadMode;

typedef enum {
    PageControl,
    PageSelectUnit,
    PageLearnNormal,
    PageListenNormal,
    PageLearnOff,
    PageListenOff,
    PageLearnDone,
    PageLearnFailed,
} AppPage;

typedef enum {
    AppEventTypeInput,
    AppEventTypeSignalCaptured,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    FuriMessageQueue* event_queue;
    FuriMutex* capture_mutex;
    ViewPort* view_port;
    InfraredWorker* infrared_worker;
    InfraredSignal* capture_signal;
    InfraredSignal* normal_signal;
    KeypadMode mode;
    AppPage page;
    uint8_t profile_ids[MAX_PROFILES];
    size_t profile_count;
    size_t selected_unit_index;
    uint8_t active_profile_id;
    uint8_t pending_profile_id;
    uint8_t temperature_c;
    uint8_t fan_speed;
    uint8_t mitsubishi_frame[AC_MITSUBISHI_FRAME_BYTES];
    uint32_t mitsubishi_prefix[AC_MITSUBISHI_PREFIX_MAX];
    size_t mitsubishi_prefix_count;
    bool mitsubishi_supported;
    bool power_on;
    bool preview_power_on;
    bool capturing;
    bool capture_received;
    bool running;
} AcMultiRemoteState;

static const char* const fan_speed_names[] = {"AUTO", "LOW", "MED", "HIGH", "MAX"};
#define FAN_SPEED_COUNT (sizeof(fan_speed_names) / sizeof(fan_speed_names[0]))

static void app_request_redraw(AcMultiRemoteState* state) {
    view_port_update(state->view_port);
}

static void input_callback(InputEvent* input, void* context) {
    AcMultiRemoteState* state = context;

    if(input->type == InputTypeShort ||
       (input->type == InputTypeLong && input->key == InputKeyOk)) {
        AppEvent event = {
            .type = AppEventTypeInput,
            .input = *input,
        };
        furi_message_queue_put(state->event_queue, &event, FuriWaitForever);
    }
}

static void received_signal_callback(void* context, InfraredWorkerSignal* received_signal) {
    AcMultiRemoteState* state = context;
    if(furi_mutex_acquire(state->capture_mutex, FuriWaitForever) != FuriStatusOk) return;

    bool post_event = false;
    if(state->capturing && !state->capture_received) {
        if(infrared_worker_signal_is_decoded(received_signal)) {
            infrared_signal_set_message(
                state->capture_signal, infrared_worker_get_decoded_signal(received_signal));
        } else {
            const uint32_t* timings = NULL;
            size_t timings_count = 0;
            infrared_worker_get_raw_signal(received_signal, &timings, &timings_count);
            infrared_signal_set_raw_signal(
                state->capture_signal,
                timings,
                timings_count,
                INFRARED_COMMON_CARRIER_FREQUENCY,
                INFRARED_COMMON_DUTY_CYCLE);
        }
        state->capture_received = true;
        post_event = true;
    }

    furi_mutex_release(state->capture_mutex);

    if(post_event) {
        AppEvent event = {.type = AppEventTypeSignalCaptured};
        furi_message_queue_put(state->event_queue, &event, FuriWaitForever);
    }
}

static void draw_header(Canvas* canvas, const char* heading) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "AC MULTI REMOTE");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 20, heading);
    canvas_draw_line(canvas, 4, 23, 123, 23);
}

static void draw_control_key_hints(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);

    /* Up and down arrows for temperature. */
    canvas_draw_line(canvas, 6, 62, 6, 57);
    canvas_draw_line(canvas, 4, 59, 6, 57);
    canvas_draw_line(canvas, 8, 59, 6, 57);
    canvas_draw_line(canvas, 12, 57, 12, 62);
    canvas_draw_line(canvas, 10, 60, 12, 62);
    canvas_draw_line(canvas, 14, 60, 12, 62);
    canvas_draw_str(canvas, 17, 63, "TEMP");

    /* Left and right arrows for fan speed. */
    canvas_draw_line(canvas, 51, 60, 46, 60);
    canvas_draw_line(canvas, 46, 60, 48, 58);
    canvas_draw_line(canvas, 46, 60, 48, 62);
    canvas_draw_line(canvas, 55, 60, 60, 60);
    canvas_draw_line(canvas, 60, 60, 58, 58);
    canvas_draw_line(canvas, 60, 60, 58, 62);
    canvas_draw_str(canvas, 64, 63, "FAN");

    canvas_draw_str(canvas, 89, 63, "OK POWER");
}

static void draw_control_screen(Canvas* canvas, const AcMultiRemoteState* state) {
    char temperature_text[16];
    char power_text[12];
    char fan_text[12];
    char profile_text[24];
    char mode_text[12];

    if(state->active_profile_id && state->mitsubishi_supported) {
        const uint8_t mode_code = (state->mitsubishi_frame[6] >> 3) & 0x07U;
        const char* mode_name = "UNKNOWN";
        if(mode_code == 1U) mode_name = "HEAT";
        else if(mode_code == 2U) mode_name = "DRY";
        else if(mode_code == 3U) mode_name = "COOL";
        else if(mode_code == 4U) mode_name = "AUTO";
        else if(mode_code == 7U) mode_name = "FAN";

        snprintf(profile_text, sizeof(profile_text), "UNIT %02u", (unsigned)state->active_profile_id);
        snprintf(temperature_text, sizeof(temperature_text), "%u", (unsigned)state->temperature_c);
        snprintf(power_text, sizeof(power_text), "%s", state->power_on ? "ON" : "OFF");
        snprintf(fan_text, sizeof(fan_text), "%s", fan_speed_names[state->fan_speed]);
        snprintf(mode_text, sizeof(mode_text), "%s", mode_name);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 9, "AC MULTI REMOTE");
        canvas_draw_str(canvas, 96, 9, profile_text);
        canvas_draw_line(canvas, 4, 13, 123, 13);
        canvas_draw_str(canvas, 4, 22, "HOLD OK: MODE");
        canvas_draw_line(canvas, 4, 25, 123, 25);

        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 4, 48, temperature_text);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 30, 34, "\u00b0C");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 52, 32, "MODE");
        canvas_draw_str(canvas, 52, 41, "POWER");
        canvas_draw_str(canvas, 52, 50, "FAN");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 88, 32, mode_text);
        canvas_draw_str(canvas, 88, 41, power_text);
        canvas_draw_str(canvas, 88, 50, fan_text);

        canvas_draw_line(canvas, 4, 54, 123, 54);
        canvas_set_font(canvas, FontSecondary);
        draw_control_key_hints(canvas);
    } else {
        draw_header(canvas, "CONTROL / 2xOK: UNITS");

        if(state->active_profile_id) {
            snprintf(profile_text, sizeof(profile_text), "AC UNIT %02u: LEARNED", (unsigned)state->active_profile_id);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 33, profile_text);
            snprintf(power_text, sizeof(power_text), "POWER  %s", state->power_on ? "ON*" : "OFF*");
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 4, 46, power_text);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 56, "TEMP/FAN NOT LEARNED");
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 33, "NO PROFILE - PREVIEW");
            snprintf(temperature_text, sizeof(temperature_text), "TEMP  %u C", (unsigned)state->temperature_c);
            snprintf(power_text, sizeof(power_text), "POWER  %s", state->preview_power_on ? "ON" : "OFF");
            snprintf(fan_text, sizeof(fan_text), "FAN  %s", fan_speed_names[state->fan_speed]);

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 4, 46, temperature_text);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 55, power_text);
            canvas_draw_str(canvas, 70, 55, fan_text);
        }
    }
}
static void draw_unit_screen(Canvas* canvas, const AcMultiRemoteState* state) {
    char unit_text[32];

    draw_header(canvas, "SELECT UNIT");
    canvas_set_font(canvas, FontSecondary);

    if(state->profile_count == 0) {
        canvas_draw_str(canvas, 4, 37, "No remote profiles saved");
        canvas_draw_str(canvas, 4, 51, "OK: ADD REMOTE");
    } else if(state->selected_unit_index < state->profile_count) {
        const unsigned profile_id = (unsigned)state->profile_ids[state->selected_unit_index];
        snprintf(unit_text, sizeof(unit_text), "AC UNIT %02u", profile_id);
        canvas_draw_str(canvas, 4, 37, unit_text);
        canvas_draw_str(canvas, 4, 51, "< / > SELECT   OK: USE");
    } else {
        canvas_draw_str(canvas, 4, 37, "Add a new remote");
        canvas_draw_str(canvas, 4, 51, "< / > SELECT   OK: ADD");
    }
}

static void draw_learning_screen(Canvas* canvas, const AcMultiRemoteState* state) {
    canvas_set_font(canvas, FontSecondary);

    switch(state->page) {
    case PageLearnNormal:
        draw_header(canvas, "LEARN NORMAL STATE");
        canvas_draw_str(canvas, 4, 34, "Set the AC to usual settings.");
        canvas_draw_str(canvas, 4, 45, "OK to listen, then press a");
        canvas_draw_str(canvas, 4, 56, "setting button on its remote.");
        break;
    case PageListenNormal:
        draw_header(canvas, "LISTENING FOR NORMAL");
        canvas_draw_str(canvas, 4, 36, "Point remote at Flipper.");
        canvas_draw_str(canvas, 4, 47, "Press a setting button.");
        canvas_draw_str(canvas, 4, 58, "BACK: CANCEL");
        break;
    case PageLearnOff:
        draw_header(canvas, "LEARN OFF STATE");
        canvas_draw_str(canvas, 4, 34, "Unit should be on.");
        canvas_draw_str(canvas, 4, 45, "OK to listen, then press");
        canvas_draw_str(canvas, 4, 56, "POWER to switch it off.");
        break;
    case PageListenOff:
        draw_header(canvas, "LISTENING FOR OFF");
        canvas_draw_str(canvas, 4, 36, "Point remote at Flipper.");
        canvas_draw_str(canvas, 4, 47, "Press POWER to turn off.");
        canvas_draw_str(canvas, 4, 58, "BACK: CANCEL");
        break;
    case PageLearnDone:
        draw_header(canvas, "PROFILE SAVED");
        canvas_draw_str(canvas, 4, 36, "Normal and Off learned.");
        canvas_draw_str(canvas, 4, 48, "OK: RETURN TO CONTROLS");
        break;
    case PageLearnFailed:
        draw_header(canvas, "LEARNING FAILED");
        canvas_draw_str(canvas, 4, 36, "Could not save this profile.");
        canvas_draw_str(canvas, 4, 48, "BACK: RETURN TO UNITS");
        break;
    default:
        break;
    }
}

static void draw_callback(Canvas* canvas, void* context) {
    const AcMultiRemoteState* state = context;

    canvas_clear(canvas);
    if(state->page == PageControl) {
        draw_control_screen(canvas, state);
    } else if(state->page == PageSelectUnit) {
        draw_unit_screen(canvas, state);
    } else {
        draw_learning_screen(canvas, state);
    }
}

static void build_profile_path(uint8_t profile_id, char* path, size_t path_size) {
    snprintf(path, path_size, PROFILE_PATH_FORMAT, (unsigned)profile_id);
}

static bool profile_signal_exists(Storage* storage, const char* path, const char* signal_name) {
    bool found = false;
    FlipperFormat* format = flipper_format_file_alloc(storage);
    InfraredSignal* signal = infrared_signal_alloc();

    if(flipper_format_file_open_existing(format, path)) {
        found = infrared_signal_search_by_name_and_read(signal, format, signal_name) ==
                InfraredErrorCodeNone;
        flipper_format_file_close(format);
    }

    infrared_signal_free(signal);
    flipper_format_free(format);
    return found;
}

static size_t load_profiles(AcMultiRemoteState* state) {
    size_t count = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROFILE_DIRECTORY);

    for(uint8_t id = 1; id <= (uint8_t)MAX_PROFILES; id++) {
        char path[96];
        build_profile_path(id, path, sizeof(path));
        if(storage_common_stat(storage, path, NULL) == FSE_OK &&
           profile_signal_exists(storage, path, "Normal") &&
           profile_signal_exists(storage, path, "Off")) {
            state->profile_ids[count++] = id;
        }
    }

    furi_record_close(RECORD_STORAGE);
    return count;
}

static uint8_t find_free_profile_id(void) {
    uint8_t free_id = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROFILE_DIRECTORY);

    for(uint8_t id = 1; id <= (uint8_t)MAX_PROFILES; id++) {
        char path[96];
        build_profile_path(id, path, sizeof(path));
        if(storage_common_stat(storage, path, NULL) == FSE_NOT_EXIST) {
            free_id = id;
            break;
        }
    }

    furi_record_close(RECORD_STORAGE);
    return free_id;
}

static bool save_signal_to_profile(
    Storage* storage,
    const char* path,
    const char* name,
    const InfraredSignal* signal,
    bool create_file) {
    bool success = false;
    FlipperFormat* format = flipper_format_file_alloc(storage);
    const bool opened = create_file ? flipper_format_file_open_new(format, path) :
                                      flipper_format_file_open_append(format, path);

    if(opened) {
        success = true;
        if(create_file) {
            success = flipper_format_write_header_cstr(format, "IR signals file", 1);
        }
        if(success) {
            success = infrared_signal_save(signal, format, name) == InfraredErrorCodeNone;
        }
        flipper_format_file_close(format);
    }

    flipper_format_free(format);
    return success;
}

static bool save_new_profile(AcMultiRemoteState* state) {
    bool success = false;
    char path[96];
    build_profile_path(state->pending_profile_id, path, sizeof(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROFILE_DIRECTORY);

    const bool normal_saved =
        save_signal_to_profile(storage, path, "Normal", state->normal_signal, true);
    if(normal_saved) {
        success = save_signal_to_profile(storage, path, "Off", state->capture_signal, false);
    }

    if(!success && normal_saved) {
        storage_common_remove(storage, path);
    }

    furi_record_close(RECORD_STORAGE);
    return success;
}

static bool transmit_profile_signal(uint8_t profile_id, const char* signal_name) {
    bool success = false;
    char path[96];
    build_profile_path(profile_id, path, sizeof(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* format = flipper_format_file_alloc(storage);
    InfraredSignal* signal = infrared_signal_alloc();

    if(flipper_format_file_open_existing(format, path)) {
        if(infrared_signal_search_by_name_and_read(signal, format, signal_name) ==
           InfraredErrorCodeNone) {
            infrared_signal_transmit(signal);
            success = true;
        }
        flipper_format_file_close(format);
    }

    infrared_signal_free(signal);
    flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    return success;
}

static bool load_mitsubishi_profile_state(AcMultiRemoteState* state, uint8_t profile_id) {
    state->mitsubishi_supported = false;
    state->mitsubishi_prefix_count = 0;

    char path[96];
    build_profile_path(profile_id, path, sizeof(path));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* format = flipper_format_file_alloc(storage);
    InfraredSignal* signal = infrared_signal_alloc();
    bool decoded = false;

    if(flipper_format_file_open_existing(format, path)) {
        if(infrared_signal_search_by_name_and_read(signal, format, "Normal") ==
           InfraredErrorCodeNone) {
            decoded = ac_mitsubishi_decode_profile(
                signal,
                state->mitsubishi_frame,
                state->mitsubishi_prefix,
                &state->mitsubishi_prefix_count);
        }
        flipper_format_file_close(format);
    }

    infrared_signal_free(signal);
    flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);

    if(decoded) {
        state->mitsubishi_supported = true;
        state->power_on = (state->mitsubishi_frame[5] & 0x20U) != 0U;
        state->temperature_c =
            (uint8_t)(MITSUBISHI_TEMP_MIN + (state->mitsubishi_frame[7] & 0x0FU));
        const uint8_t fan_code = state->mitsubishi_frame[9] & 0x07U;
        if((state->mitsubishi_frame[9] & 0x80U) || fan_code == 0U) {
            state->fan_speed = 0U;
        } else {
            state->fan_speed =
                (fan_code < (uint8_t)(sizeof(mitsubishi_fan_codes) / sizeof(mitsubishi_fan_codes[0]))) ?
                    fan_code :
                    (uint8_t)(FAN_SPEED_COUNT - 1U);
        }
    }

    return decoded;
}

static bool transmit_mitsubishi_state(AcMultiRemoteState* state) {
    uint32_t timings[AC_MITSUBISHI_RAW_MAX];
    size_t timings_count = 0;
    InfraredSignal* signal = infrared_signal_alloc();

    const bool built = ac_mitsubishi_build_signal(
        state->mitsubishi_frame,
        state->mitsubishi_prefix,
        state->mitsubishi_prefix_count,
        timings,
        AC_MITSUBISHI_RAW_MAX,
        &timings_count);
    if(built) {
        infrared_signal_set_raw_signal(
            signal, timings, timings_count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
        infrared_signal_transmit(signal);
    }

    infrared_signal_free(signal);
    return built;
}

static void update_mitsubishi_power(AcMultiRemoteState* state, bool power_on) {
    if(power_on) {
        state->mitsubishi_frame[5] |= 0x20U;
    } else {
        state->mitsubishi_frame[5] &= (uint8_t)~0x20U;
    }
    ac_mitsubishi_update_checksum(state->mitsubishi_frame);
    if(transmit_mitsubishi_state(state)) state->power_on = power_on;
}

static void toggle_mitsubishi_mode(AcMultiRemoteState* state) {
    const uint8_t current_mode = (state->mitsubishi_frame[6] >> 3) & 0x07U;
    const bool switch_to_heat = current_mode == 3U;
    const uint8_t next_mode = switch_to_heat ? 1U : 3U;

    state->mitsubishi_frame[6] =
        (uint8_t)((state->mitsubishi_frame[6] & (uint8_t)~0x38U) | (next_mode << 3));
    state->mitsubishi_frame[8] =
        (uint8_t)((state->mitsubishi_frame[8] & 0xF0U) | (switch_to_heat ? 0U : 6U));
    ac_mitsubishi_update_checksum(state->mitsubishi_frame);
    transmit_mitsubishi_state(state);
}

static void start_capture(AcMultiRemoteState* state, bool capture_off) {
    if(furi_mutex_acquire(state->capture_mutex, FuriWaitForever) == FuriStatusOk) {
        state->capturing = true;
        state->capture_received = false;
        furi_mutex_release(state->capture_mutex);
    }

    state->page = capture_off ? PageListenOff : PageListenNormal;
    infrared_worker_rx_set_received_signal_callback(
        state->infrared_worker, received_signal_callback, state);
    infrared_worker_rx_enable_signal_decoding(state->infrared_worker, true);
    infrared_worker_rx_enable_blink_on_receiving(state->infrared_worker, true);
    infrared_worker_rx_start(state->infrared_worker);
    app_request_redraw(state);
}

static void stop_capture(AcMultiRemoteState* state) {
    if(state->capturing) {
        infrared_worker_rx_stop(state->infrared_worker);
        if(furi_mutex_acquire(state->capture_mutex, FuriWaitForever) == FuriStatusOk) {
            state->capturing = false;
            furi_mutex_release(state->capture_mutex);
        }
    }
}

static void capture_completed(AcMultiRemoteState* state) {
    stop_capture(state);

    if(state->page == PageListenNormal) {
        infrared_signal_set_signal(state->normal_signal, state->capture_signal);
        state->page = PageLearnOff;
    } else if(state->page == PageListenOff) {
        state->page = save_new_profile(state) ? PageLearnDone : PageLearnFailed;
        if(state->page == PageLearnDone) {
            state->profile_count = load_profiles(state);
            state->active_profile_id = state->pending_profile_id;
            if(!load_mitsubishi_profile_state(state, state->active_profile_id)) {
                state->power_on = false;
            }
        }
    }

    app_request_redraw(state);
}

static void begin_new_profile(AcMultiRemoteState* state) {
    state->pending_profile_id = find_free_profile_id();
    if(state->pending_profile_id == 0) {
        state->page = PageLearnFailed;
    } else {
        state->page = PageLearnNormal;
    }
}

static void choose_selected_unit(AcMultiRemoteState* state) {
    if(state->selected_unit_index < state->profile_count) {
        state->active_profile_id = state->profile_ids[state->selected_unit_index];
        if(!load_mitsubishi_profile_state(state, state->active_profile_id)) {
            state->power_on = false;
        }
        state->page = PageControl;
    } else {
        begin_new_profile(state);
    }
}

static void handle_single_ok(AcMultiRemoteState* state) {
    switch(state->page) {
    case PageControl:
        if(state->active_profile_id && state->mitsubishi_supported) {
            update_mitsubishi_power(state, !state->power_on);
        } else if(state->active_profile_id) {
            const char* signal_name = state->power_on ? "Off" : "Normal";
            if(transmit_profile_signal(state->active_profile_id, signal_name)) {
                state->power_on = !state->power_on;
            }
        } else {
            state->preview_power_on = !state->preview_power_on;
        }
        break;
    case PageSelectUnit:
        choose_selected_unit(state);
        break;
    case PageLearnNormal:
        start_capture(state, false);
        break;
    case PageLearnOff:
        start_capture(state, true);
        break;
    case PageLearnDone:
        state->page = PageControl;
        break;
    case PageListenNormal:
    case PageListenOff:
    case PageLearnFailed:
    default:
        break;
    }
}

static void handle_back(AcMultiRemoteState* state) {
    switch(state->page) {
    case PageControl:
        state->running = false;
        break;
    case PageSelectUnit:
        state->page = PageControl;
        break;
    case PageListenNormal:
        stop_capture(state);
        state->page = PageLearnNormal;
        break;
    case PageListenOff:
        stop_capture(state);
        state->page = PageLearnOff;
        break;
    case PageLearnNormal:
    case PageLearnOff:
    case PageLearnFailed:
        state->page = PageSelectUnit;
        break;
    case PageLearnDone:
        state->page = PageControl;
        break;
    }
}

static void handle_input(AcMultiRemoteState* state, const InputEvent* event) {
    if(event->key == InputKeyBack) {
        handle_back(state);
        return;
    }

    if(event->key == InputKeyOk) {
        handle_single_ok(state);
        return;
    }

    if(state->page == PageControl && state->mitsubishi_supported) {
        bool changed = false;
        switch(event->key) {
        case InputKeyUp:
            if(state->temperature_c < MITSUBISHI_TEMP_MAX) {
                state->temperature_c++;
                state->mitsubishi_frame[7] =
                    (uint8_t)((state->mitsubishi_frame[7] & 0xE0U) |
                              (state->temperature_c - MITSUBISHI_TEMP_MIN));
                changed = true;
            }
            break;
        case InputKeyDown:
            if(state->temperature_c > MITSUBISHI_TEMP_MIN) {
                state->temperature_c--;
                state->mitsubishi_frame[7] =
                    (uint8_t)((state->mitsubishi_frame[7] & 0xE0U) |
                              (state->temperature_c - MITSUBISHI_TEMP_MIN));
                changed = true;
            }
            break;
        case InputKeyLeft:
            if(state->fan_speed > 0U) {
                state->fan_speed--;
                changed = true;
            }
            break;
        case InputKeyRight:
            if(state->fan_speed < (uint8_t)(FAN_SPEED_COUNT - 1U)) {
                state->fan_speed++;
                changed = true;
            }
            break;
        default:
            break;
        }

        if(changed) {
            const uint8_t fan_code = mitsubishi_fan_codes[state->fan_speed];
            state->mitsubishi_frame[9] =
                (uint8_t)((state->mitsubishi_frame[9] & 0x78U) | (fan_code & 0x07U));
            if(fan_code == 0U) {
                state->mitsubishi_frame[9] |= 0x80U;
            } else {
                state->mitsubishi_frame[9] &= (uint8_t)~0x80U;
            }
            ac_mitsubishi_update_checksum(state->mitsubishi_frame);
            transmit_mitsubishi_state(state);
        }
    } else if(state->page == PageControl && !state->active_profile_id) {
        switch(event->key) {
        case InputKeyUp:
            if(state->temperature_c < 30) state->temperature_c++;
            break;
        case InputKeyDown:
            if(state->temperature_c > 16) state->temperature_c--;
            break;
        case InputKeyLeft:
            if(state->fan_speed > 0) state->fan_speed--;
            break;
        case InputKeyRight:
            if(state->fan_speed < (uint8_t)(FAN_SPEED_COUNT - 1U)) state->fan_speed++;
            break;
        default:
            break;
        }
    } else if(state->page == PageSelectUnit) {
        const size_t choices = state->profile_count + ((state->profile_count < MAX_PROFILES) ? 1U : 0U);
        if(choices > 0 && event->key == InputKeyRight) {
            state->selected_unit_index = (state->selected_unit_index + 1U) % choices;
        } else if(choices > 0 && event->key == InputKeyLeft) {
            state->selected_unit_index =
                (state->selected_unit_index == 0) ? choices - 1U : state->selected_unit_index - 1U;
        }
    }
}

static void handle_input_event(AcMultiRemoteState* state, const InputEvent* input) {
    if(input->key == InputKeyOk && input->type == InputTypeLong &&
       state->page == PageControl && state->mitsubishi_supported) {
        toggle_mitsubishi_mode(state);
        return;
    }

    if(input->key == InputKeyOk &&
       (state->page == PageControl || state->page == PageSelectUnit)) {
        return;
    }

    handle_input(state, input);
}

int32_t gbhobbytech_ac_multi_remote_app(void* p) {
    UNUSED(p);

    AcMultiRemoteState state = {
        .event_queue = furi_message_queue_alloc(EVENT_QUEUE_SIZE, sizeof(AppEvent)),
        .capture_mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .infrared_worker = infrared_worker_alloc(),
        .capture_signal = infrared_signal_alloc(),
        .normal_signal = infrared_signal_alloc(),
        .mode = KeypadModeControl,
        .page = PageControl,
        .temperature_c = 22,
        .fan_speed = 0,
        .power_on = false,
        .preview_power_on = false,
        .capturing = false,
        .capture_received = false,
        .running = true,
    };

    state.profile_count = load_profiles(&state);
    if(state.profile_count > 0) {
        state.active_profile_id = state.profile_ids[0];
        load_mitsubishi_profile_state(&state, state.active_profile_id);
    }

    ViewPort* view_port = view_port_alloc();
    state.view_port = view_port;
    view_port_draw_callback_set(view_port, draw_callback, &state);
    view_port_input_callback_set(view_port, input_callback, &state);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    view_port_update(view_port);

    bool ok_pending = false;
    uint32_t ok_tick = 0;
    const uint32_t double_click_ticks = furi_ms_to_ticks(OK_DOUBLE_CLICK_MS);

    while(state.running) {
        AppEvent event;
        uint32_t wait_ticks = FuriWaitForever;

        if(ok_pending) {
            const uint32_t elapsed = furi_get_tick() - ok_tick;
            wait_ticks = (elapsed < double_click_ticks) ? (double_click_ticks - elapsed) : 0;
        }

        const FuriStatus status = furi_message_queue_get(state.event_queue, &event, wait_ticks);
        if(status == FuriStatusOk) {
            if(event.type == AppEventTypeSignalCaptured) {
                if(state.page == PageListenNormal || state.page == PageListenOff) {
                    capture_completed(&state);
                }
            } else if(event.input.key == InputKeyOk &&
                      event.input.type == InputTypeShort &&
                      (state.page == PageControl || state.page == PageSelectUnit)) {
                const uint32_t now = furi_get_tick();
                if(ok_pending && ((now - ok_tick) < double_click_ticks)) {
                    if(state.page == PageControl) {
                        state.page = PageSelectUnit;
                        state.mode = KeypadModeSelectUnit;
                        state.selected_unit_index = 0;
                    } else {
                        state.page = PageControl;
                        state.mode = KeypadModeControl;
                    }
                    ok_pending = false;
                } else {
                    if(ok_pending) handle_single_ok(&state);
                    ok_pending = true;
                    ok_tick = now;
                }
            } else {
                if(ok_pending) {
                    handle_single_ok(&state);
                    ok_pending = false;
                }
                handle_input_event(&state, &event.input);
            }
            app_request_redraw(&state);
        } else if(ok_pending) {
            handle_single_ok(&state);
            ok_pending = false;
            app_request_redraw(&state);
        }
    }

    stop_capture(&state);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    infrared_signal_free(state.normal_signal);
    infrared_signal_free(state.capture_signal);
    infrared_worker_free(state.infrared_worker);
    furi_mutex_free(state.capture_mutex);
    furi_message_queue_free(state.event_queue);

    return 0;
}
