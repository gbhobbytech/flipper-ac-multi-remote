# AC Multi Remote for Flipper Zero

A simple infrared controller for air conditioners, built as a Flipper Zero external app.

## Project direction

- Keep the main control screen simple: power, temperature, and fan speed.
- Use double-click OK to switch between **Control** and **Select Unit** keypad modes.
- On **Select Unit**, short OK uses the selected unit, long OK opens **Rename / Delete**, and double-click OK returns to **Control**.
- Store reusable remote profiles by protocol family, with optional user nicknames.
- Use a guided setup flow to identify supported protocols or learn and replay verified AC states.
- When a learned protocol cannot be interpreted, offer captured states as presets instead of claiming unavailable controls.

## Current development status

The app has a guided raw-signal learning flow for a normal/on state and an off state. It saves each completed profile to the Flipper microSD card.

When the Normal capture contains a valid Mitsubishi Electric state frame, the app recognises it and enables direct power, temperature, fan, and mode controls. The known-working 144-bit pathway remains first in the Mitsubishi Electric trial order and was tested with an SG17B remote and an MSZ-AG35VGD-family unit. The 112-bit and 136-bit Mitsubishi Electric pathways are included after it, with direct state controls and room-probe commands; they still need physical Flipper and unit testing before being treated as confirmed. Their native temperature and fan ranges are used, including the four-level fan scale on these variants. For the 144-bit profile, the app preserves the learned command prefix and updates the state frame and checksum before transmitting.

### Adding a unit: identification wizard

From **Select Unit**, press Down to add a unit. The wizard starts with two choices:

- **Identify Unit:** choose the brand printed on the indoor unit, then try only the in-room protocols registered for that manufacturer. Before each test, the app explains the command it will send. The user confirms **Yes** if the unit responds or **No** to try the next protocol. Choosing **Not sure: try all** tests every in-room protocol currently available in the catalogue.
- **Learn Remote:** capture the unit's normal On state and its Off state, then save them for replay.

The in-room trial catalogue includes Mitsubishi Electric in this order: 144-bit, 112-bit, then 136-bit; Mitsubishi Heavy Industries (MHI) 88-bit and 152-bit; Gree in this order: YAW1F, YBOFB, then YX1FSF; and Fujitsu in this order: 16-byte protocol 0x30, compact 15-byte protocol 0xFC, then 16-byte protocol 0x31. The Mitsubishi Electric 144-bit path is the confirmed baseline, and the MHI control path has been confirmed locally. The other listed trials still need local hardware verification. Gree and Fujitsu profiles decode state frames for direct power, temperature, mode, and fan control. If all tests for a selected brand fail, the wizard offers Learn Remote as the fallback. Confirming a successful test saves the unit and returns to the control screen.

Brands, protocol families, variants, and remote-code references live in `ac_protocols.c`. Protocol families are linked to their manufacturer so the wizard can filter tests by brand. Mitsubishi Electric and Mitsubishi Heavy Industries are separate manufacturers with unrelated protocols. Fujitsu protocol variants and their probe order are included in the bundled catalogue.

Mitsubishi Heavy Industries (MHI) 88-bit and 152-bit captures can be identified from their raw signal signatures. The bundled `ac_mhi.c` module builds complete frames and raw timings for both variants, and confirmed MHI profiles use the main screen's direct power, temperature, mode, and fan controls. The MHI control path has been confirmed working on a local unit. The listed MHI remote-code mappings remain reference leads and have not been verified against a physical remote; they are not part of the current setup flow.
Fujitsu support groups six remote models into three distinct safe baselines: ARRAH2E/ARREB1E/ARRY4 share the 16-byte 0x30 probe when optional features are off; ARDB1/ARJW2 share the compact frame when swing is off; and ARREW4E uses 16-byte 0x31. The app explicitly clears swing state and does not send Fujitsu swing-step or swing-toggle commands. The tested room still needs a Yes/No response for the user to confirm which baseline controls it. For other raw profiles, the app continues to replay the learned Normal and Off signals and shows that temperature and fan are not decoded. Without a saved profile, the screen remains in preview mode.

## Build

From the project directory, with uFBT installed:

```sh
ufbt
```

The built application is written to `dist/`.

## Planned milestones

1. Confirm the main screen and keypad interaction on the Flipper.
2. Add an infrared capture and replay flow for learned states.
3. Save and select user profiles.
4. Extend protocol recognition to additional AC families and learned preset controls.
