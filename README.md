# AC Multi Remote for Flipper Zero

A simple infrared controller for air conditioners, built as a Flipper Zero external app.

## Project direction

- Keep the main control screen simple: power, temperature, and fan speed.
- Use double-click OK to switch between **Control** and **Select Unit** keypad modes.
- Store reusable remote profiles by protocol family, with optional user nicknames.
- Use a guided setup flow to identify supported protocols or learn and replay verified AC states.
- When a learned protocol cannot be interpreted, offer captured states as presets instead of claiming unavailable controls.

## Current development status

The app now has a guided raw-signal learning flow for a normal/on state and an off state. It saves each completed profile to the Flipper microSD card and can replay those two learned states from the Control screen.

Temperature and fan adjustment are not yet encoded for learned profiles. Those controls will be enabled when a supported protocol profile or a sufficient set of learned presets is available. Without a saved profile, the screen remains in preview mode.

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
4. Add protocol-specific AC state encoders for direct temperature and fan controls.
