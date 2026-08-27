# Leaf CAN logger

Battery telemetry from a 2015 Nissan Leaf (24 kWh), read directly off the CAN
bus by an ESP32. Built primarily to capture **charge sessions** — the thing
that is awkward to get out of LeafSpy, because a phone has to sit there for the
whole session.

No ELM327 dongle in the data path: the ESP32's built-in TWAI peripheral talks
to the bus through a CAN transceiver, which makes it faster, lets it poll
diagnostics and sniff broadcast frames at the same time, and — the part that
matters here — allows deep sleep with wake-on-bus-activity.

## Status

Milestone 1: bus, ISO-TP, and the validation console. **The group 1 byte
offsets are deliberately not filled in** — see [Resolving the offsets](#resolving-the-offsets).

| Layer | State |
|---|---|
| ISO-TP reassembly | written, unit tested (109 host assertions) |
| Cell voltage decode (group 2) | written, unit tested |
| Group 1 decode (SOC/SOH/AHr/V/A) | mechanism written, **offsets unresolved** |
| TWAI driver, LBC client, console | written, **not yet compiled or run on hardware** |
| Sleep/wake state machine | written, **not yet validated on hardware** |
| Charge-session logging, web UI | not started |

The pure layers are compiled and tested. Everything touching the ESP32 has not
been near a compiler yet — expect to fix a few includes on first build.

## Hardware

- ESP32 DevKit V1 (30-pin, ESP32-WROOM-32)
- CAN transceiver: SN65HVD230 / VP230 breakout, **or** SN65HVD231
- 12 V → 5 V automotive supply (a dashcam hardwire kit works well; its
  low-voltage cutoff also protects the car's 12 V battery)

### Wiring

| Transceiver | ESP32 | Note |
|---|---|---|
| 3V3 | 3V3 | |
| GND | GND | |
| CTX (D) | GPIO5 (D5) | |
| CRX (R) | GPIO4 (D4) | RTC-capable — doubles as the ext0 wake pin |
| RS | GPIO25 (D25) | standby control; see below |
| CANH | OBD pin 6 | |
| CANL | OBD pin 14 | |

Power comes from the fuse box rather than OBD pin 16, which keeps the OBD
connector free so a Veepeak dongle can be plugged in alongside for validation.

### Before connecting to the car

1. **Remove the 120 Ω termination resistor** from the transceiver breakout
   (R2 on the Waveshare board). The car's bus is already terminated at both
   ends; a third resistor over-terminates it and causes intermittent faults.
2. **Tap a constant (always-hot) fuse, not a switched one.** During charging
   the ignition is off, so a switched circuit is dead exactly when you need it.
   Orient the add-a-circuit so the original fuse stays on the feed side.
3. **Never feed 12 V into VIN.** Use a proper automotive buck; car rails see
   load-dump spikes well above 40 V.

The firmware **starts in listen-only mode**, in which the TWAI peripheral
physically cannot transmit or even acknowledge. Watch the bus with `m` first;
only press `n` to enable transmit once the traffic looks sane. That ordering
makes a wiring mistake harmless rather than a problem for the car.

### Wake on CAN

The sleep design puts the transceiver in standby (RS high), where its driver is
off but its receiver stays live, and arms an `ext0` wake on the CAN RX line
going low. The first dominant bit of the first frame wakes the ESP32 in about
100 ms, so a charge session is captured from the start.

**The common Waveshare/VP230 breakout does not break out RS** — it ties it to
ground through a slope-control resistor. Options:

- Lift that resistor and solder a wire to pin 8 of the SOIC-8.
- Use a bare SN65HVD230/231 on a breakout adapter.
- Set `kHasRsControl = false` in `include/config.h` and accept timer-only wake,
  which misses up to `kTimerWakeIntervalUs` of a session.

Note also that this DevKit's deep sleep floor is ~10–20 mA because of its
CP2102 and AMS1117, regardless of firmware. Fine for development and for a
daily-driven car; a permanent install wants a bare module and a low-quiescent
regulator.

## Build

Pure layers — no toolchain download, no hardware:

```sh
make test           # ISO-TP + decoder unit tests
make find-offsets   # builds tools/find_offsets
```

Firmware:

```sh
pio run -t upload && pio device monitor
```

## Resolving the offsets

The 2011–2015 LBC layout is the best documented of any Leaf generation, but
published tables disagree on framing — some quote offsets from the start of the
ISO-TP payload (including the `61 xx` echo), others from the first data byte —
and some are actually for later model years. A wrong offset that decodes to a
believable number is expensive to debug, so `kGroup1Unverified` in
`src/leaf/pids.h` is zeroed and `DecodeGroup1` refuses to run against it.

Resolve it empirically in one sitting:

1. Park with the car in Ready (or charging). Plug the Veepeak in alongside and
   open LeafSpy on your phone.
2. Press `1` in the console to capture the group 1 payload. It prints the bytes
   under both offset conventions, plus a flat line to copy.
3. Read a distinctive value off LeafSpy at the same moment — SOH `87.5`,
   AHr `56.34`. Distinctive beats round: `100` matches everywhere.
4. Run:

   ```sh
   ./build/find_offsets "61 01 00 00 ..." --target 87.5 --tol 0.05
   ```

   It reports every `(offset, width, signedness, scale)` that produces that
   value. Leading zero bytes alias — offset 2 width 4 and offset 4 width 2 read
   the same number — so capture a second sample at a different SOC and keep
   only the candidates that survive both.
5. Fill in `Group1Layout`, delete the UNVERIFIED banner, add a regression test
   in `test/host/test_decode.cpp` using the captured payload as a fixture.

Step 5 is what stops this from regressing later: once a real payload is pinned
in a test, the decode is protected by `make test` forever after.

## Layout

```
include/config.h            pins, timings, feature flags
src/transport/isotp.*       ISO 15765-2 reassembly    (pure, tested)
src/transport/twai_bus.*    ESP32 TWAI wrapper
src/transport/lbc_client.*  request/response to 0x79B / 0x7BB
src/leaf/pids.h             addressing + the offset table
src/leaf/decode.*           decoders                  (pure, tested)
src/power/sleep_manager.*   two-state power management
src/debug/console.*         driveway validation console
tools/find_offsets.cpp      offset solver             (host)
test/host/                  unit tests                (host)
```

The pure layers deliberately avoid Arduino and ESP-IDF headers so the decode
logic — the part that is genuinely hard to get right — can be tested on a
laptop against captured payloads, with no car and no hardware present.

## Next

1. Confirm the wiring on the bench (two ESP32s, one as a fake LBC).
2. Driveway session: resolve the offsets, pin them in a test.
3. Charge-session state machine: detect start, sample, detect end.
4. Push to MQTT over home WiFi during the session; buffer to flash when away.
5. Web UI.
