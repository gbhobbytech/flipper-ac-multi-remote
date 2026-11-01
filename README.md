# AC Multi Remote for Flipper Zero

A simple infrared controller for air conditioners, built as a Flipper Zero external app.

## Project direction

- Keep the main control screen simple: power, temperature, and fan speed.
- Use double-click OK to switch between **Control** and **Select Unit** keypad modes.
- Store reusable remote profiles by protocol family, with optional user nicknames.
- Use a guided setup flow to identify supported protocols or learn and replay verified AC states.
- When a learned protocol cannot be interpreted, offer captured states as presets instead of claiming unavailable controls.

## Current development status

The app has a guided raw-signal learning flow for a normal/on state and an off state. It saves each completed profile to the Flipper microSD card.

When the Normal capture contains a valid Mitsubishi Electric 144-bit state frame, the app recognises it and enables direct power, temperature (16–31 °C), fan (Auto/Low/Med/High/Max), and mode controls. The current Heat/Cool mode is shown on screen; hold OK to switch between Heat and Cool. This matches the Mitsubishi Electric SG17B remote used with the MSZ-AP35VGD family. The main control page puts the setpoint on the left and shows Mode, Power, and Fan as aligned label/value pairs on the right. The app preserves the learned command prefix and updates the state frame and checksum before transmitting.

### Adding a unit: identification wizard

From **Select Unit**, press Down to add a unit. The wizard offers three routes:

- **Remote model code:** pick the brand on the indoor unit, then the code printed on the back of the original remote. The app shows the protocol family and variant for that remote, then learns Normal and Off.
- **Capture remote:** press TEMP + on the original remote. The app compares the capture against the protocol matchers it implements and reports a match, no match, or an ambiguous match.
- **Learn only (replay):** skip identification and learn Normal/Off for replay.

Every list ends with **Not listed: capture**. When a capture is not recognised or is ambiguous, the wizard says so and offers the learn-only fallback. After saving, the app reports whether the learned Normal signal decoded as a supported protocol, and warns if it does not match the protocol chosen in the wizard.

Brands, remote codes and variants live in `ac_protocols.c`. They are kept as separate records (manufacturer, protocol family, protocol variant, remote entry). Only variants with a working driver are listed, and remote codes are added only after testing on a real unit. Mitsubishi Electric and Mitsubishi Heavy Industries are separate manufacturers with unrelated protocols.

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
