#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#define EVENT_QUEUE_SIZE 8U
#define OK_DOUBLE_CLICK_MS 300U

typedef enum {
    KeypadModeControl,
    KeypadModeSelectUnit,
} KeypadMode;

typedef struct {
    FuriMessageQueue* event_queue;
    KeypadMode mode;
    uint8_t temperature_c;
    uint8_t fan_speed;
    bool power_on;
    bool running;
} AcMultiRemoteState;

static const char* const fan_speed_names[] = {"AUTO", "LOW", "MED", "HIGH"};
#define FAN_SPEED_COUNT (sizeof(fan_speed_names) / sizeof(fan_speed_names[0]))

static void input_callback(InputEvent* event, void* context) {
    AcMultiRemoteState* state = context;

    if(event->type == InputTypeShort) {
        furi_message_queue_put(state->event_queue, event, FuriWaitForever);
    }
}

static void draw_header(Canvas* canvas, const char* mode_name) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "AC MULTI REMOTE");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 20, mode_name);
    canvas_draw_line(canvas, 4, 23, 123, 23);
}

static void draw_control_screen(Canvas* canvas, const AcMultiRemoteState* state) {
    char temperature_text[24];
    char power_text[24];
    char fan_text[24];

    draw_header(canvas, "CONTROL");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 33, "NO REMOTE PROFILE");
    snprintf(temperature_text, sizeof(temperature_text), "TEMP  %u C", state->temperature_c);
    snprintf(power_text, sizeof(power_text), "POWER  %s", state->power_on ? "ON" : "OFF");
    snprintf(fan_text, sizeof(fan_text), "FAN  %s", fan_speed_names[state->fan_speed]);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 46, temperature_text);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 55, power_text);
    canvas_draw_str(canvas, 70, 55, fan_text);

    canvas_draw_line(canvas, 4, 58, 123, 58);
    canvas_draw_str(canvas, 4, 64, "PREVIEW ONLY  |  2xOK: UNITS");
}

static void draw_unit_screen(Canvas* canvas) {
    draw_header(canvas, "SELECT UNIT");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 36, "No remote profiles saved");
    canvas_draw_str(canvas, 4, 47, "Setup wizard is next");
    canvas_draw_line(canvas, 4, 58, 123, 58);
    canvas_draw_str(canvas, 4, 64, "OK: RETURN  |  BACK: RETURN");
}

static void draw_callback(Canvas* canvas, void* context) {
    const AcMultiRemoteState* state = context;

    canvas_clear(canvas);
    if(state->mode == KeypadModeControl) {
        draw_control_screen(canvas, state);
    } else {
        draw_unit_screen(canvas);
    }
}

static void handle_single_ok(AcMultiRemoteState* state) {
    if(state->mode == KeypadModeControl) {
        // Preview state only. IR transmission is added in the next milestone.
        state->power_on = !state->power_on;
    } else {
        state->mode = KeypadModeControl;
    }
}

static void handle_input(AcMultiRemoteState* state, const InputEvent* event) {
    if(event->key == InputKeyBack) {
        if(state->mode == KeypadModeSelectUnit) {
            state->mode = KeypadModeControl;
        } else {
            state->running = false;
        }
        return;
    }

    if(event->key == InputKeyOk) {
        return;
    }

    if(state->mode != KeypadModeControl) {
        return;
    }

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
        if(state->fan_speed + 1 < FAN_SPEED_COUNT) state->fan_speed++;
        break;
    default:
        break;
    }
}

int32_t gbhobbytech_ac_multi_remote_app(void* p) {
    UNUSED(p);

    AcMultiRemoteState state = {
        .event_queue = furi_message_queue_alloc(EVENT_QUEUE_SIZE, sizeof(InputEvent)),
        .mode = KeypadModeControl,
        .temperature_c = 22,
        .fan_speed = 0,
        .power_on = false,
        .running = true,
    };

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &state);
    view_port_input_callback_set(view_port, input_callback, &state);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    view_port_update(view_port);

    bool ok_pending = false;
    uint32_t ok_tick = 0;
    const uint32_t double_click_ticks = furi_ms_to_ticks(OK_DOUBLE_CLICK_MS);

    while(state.running) {
        InputEvent event;
        uint32_t wait_ticks = FuriWaitForever;

        if(ok_pending) {
            const uint32_t elapsed = furi_get_tick() - ok_tick;
            wait_ticks = (elapsed < double_click_ticks) ? (double_click_ticks - elapsed) : 0;
        }

        const FuriStatus status = furi_message_queue_get(state.event_queue, &event, wait_ticks);
        if(status == FuriStatusOk) {
            if(event.key == InputKeyOk) {
                const uint32_t now = furi_get_tick();
                if(ok_pending && ((now - ok_tick) < double_click_ticks)) {
                    state.mode = (state.mode == KeypadModeControl) ?
                                    KeypadModeSelectUnit :
                                    KeypadModeControl;
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
                handle_input(&state, &event);
            }
            view_port_update(view_port);
        } else if(ok_pending) {
            handle_single_ok(&state);
            ok_pending = false;
            view_port_update(view_port);
        }
    }

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(state.event_queue);

    return 0;
}
