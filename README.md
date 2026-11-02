# A/C Master for Flipper Zero

A multi-profile infrared controller for compatible air conditioners. A/C Master can send direct state commands for supported protocols, guide protocol discovery in a room, or learn and replay basic signals from an existing remote.

## Features

- Control power, temperature, mode and fan speed when a profile has a supported decoded protocol.
- Save multiple unit profiles and give each one a useful name.
- Identify a unit by manufacturer and try the bundled protocol variants with a simple Yes/No confirmation.
- Learn and replay Normal and Off signals for units without direct state support.
- Store profiles on the Flipper Zero microSD card.

## Supported protocol families

The bundled protocol library includes Mitsubishi Electric, Mitsubishi Heavy Industries (MHI), Gree and Fujitsu variants.

- **Mitsubishi Electric:** the 144-bit pathway is the confirmed baseline. The 112-bit and 136-bit pathways are included but need further hardware verification.
- **MHI:** 88-bit and 152-bit pathways are included. The MHI control path has been confirmed on a local unit.
- **Gree:** YAW1F, YBOFB and YX1FSF variants are included; local hardware verification is still needed.
- **Fujitsu:** 0x30, compact 0xFC and 0x31 variants are included; local hardware verification is still needed. Fujitsu swing commands are deliberately excluded from probes.

Protocol compatibility can vary between models. A successful probe should be confirmed by the user before saving a profile. If direct state control is unavailable, learned profiles can replay the saved Normal and Off signals; temperature, mode and fan controls are not available for those profiles.

## Controls

On the control screen:

- Up / Down: adjust temperature.
- Left / Right: adjust fan speed.
- Short OK: toggle power.
- Long OK: change mode when supported.
- Double-click OK: open or leave Select Unit.

On Select Unit:

- Left / Right: browse saved units.
- Short OK: use the selected unit.
- Down: start adding a unit.
- Long OK: open Rename / Delete.

## Add a unit

Choose **Identify Unit** to select a manufacturer and try its known protocol variants. A/C Master sends a test state and asks whether the unit responded. Confirm a successful result to save the profile. If identification does not work, choose **Learn Remote** to capture Normal and Off signals from an existing remote.

## Build and run

Install uFBT, then run from the project directory:

```sh
ufbt
```

The FAP is created in `dist/gbhobbytech_ac_multi_remote.fap`. To build, upload and launch over USB, run:

```sh
ufbt launch
```

To install it persistently, copy the FAP to **SD Card/apps/Infrared** with qFlipper.

## Screenshots

Catalog screenshots are being prepared from the running app using qFlipper. They will be added here after capture.

## Profile storage

Profiles are stored under `apps_data/gbhobbytech_ac_multi_remote` on the Flipper Zero microSD card.

## License

A/C Master is licensed under the MIT License. See [LICENSE](LICENSE).
