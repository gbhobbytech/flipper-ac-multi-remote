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

The current in-room trials include Mitsubishi Electric in this order: 144-bit, 112-bit, then 136-bit; and Mitsubishi Heavy Industries (MHI) 88-bit and 152-bit. The Mitsubishi Electric 144-bit path is the confirmed baseline. The 112-bit and 136-bit paths need local hardware verification. Fujitsu and Gree are listed as brands, but do not yet have in-room test protocols; selecting either guides the user to Learn Remote. If all tests for a selected brand fail, the wizard offers Learn Remote as the fallback. Confirming a successful test saves the unit and returns to the control screen.

Brands, protocol families, variants, and remote-code references live in `ac_protocols.c`. Protocol families are linked to their manufacturer so the wizard can filter tests by brand. Mitsubishi Electric and Mitsubishi Heavy Industries are separate manufacturers with unrelated protocols. Fujitsu and Gree are present in the brand list while their protocol support is still being researched and added.

Mitsubishi Heavy Industries (MHI) 88-bit and 152-bit captures can be identified from their raw signal signatures. The bundled `ac_mhi.c` module builds complete frames and raw timings for both variants, and confirmed MHI profiles use the main screen's direct power, temperature, mode, and fan controls. The MHI control path has been confirmed working on a local unit. The listed MHI remote-code mappings remain reference leads and have not been verified against a physical remote; they are not part of the current setup flow.
For other raw profiles, the app continues to replay the learned Normal and Off signals and shows that temperature and fan are not decoded. Without a saved profile, the screen remains in preview mode.

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
