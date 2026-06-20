# ClaudeMeterEpaper

Claude Code usage meter on a **Waveshare ESP32-S3-ePaper-1.54** (200×200 B/W,
SSD1681). An e-paper take on [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter):
shows your 5-hour and 7-day rate-limit utilization as percentages + bars, with
minutes-to-reset.

## Why a separate repo (not a fork)

- **Different hardware stack.** Clawdmeter targets AMOLED touch boards driven by
  LVGL. This board is e-paper (GxEPD2, slow full-refresh, no animation). Almost
  no firmware is shared.
- **License hygiene.** Clawdmeter bundles proprietary Anthropic fonts and the
  copyrighted Clawd mascot, and ships **no LICENSE file** (default: all rights
  reserved). Forking would pull that in.

Instead this repo contains **firmware only** and reuses Clawdmeter's Python
daemon **unmodified** by matching its BLE GATT contract — you *run* their
daemon, you don't copy its code. No redistribution, no license entanglement.

## Architecture

```
Clawdmeter daemon (their repo, run as-is)         this firmware (ESP32-S3)
  poll Anthropic rate-limit headers  ──BLE write──▶  parse JSON ─▶ draw e-paper
```

### BLE GATT contract (kept identical to Clawdmeter)

| Item | Value |
|------|-------|
| Device name | `Clawdmeter` |
| Service | `4c41555a-4465-7669-6365-000000000001` |
| RX char (daemon → device, write) | `...0002` |
| REQ char (device → daemon, notify) | `...0004` |

### Payload (written by daemon to RX char)

```json
{"s":42,"sr":118,"w":7,"wr":9320,"st":"allowed","ok":true}
```

| Key | Meaning |
|-----|---------|
| `s`  | 5h utilization % |
| `sr` | minutes until 5h reset |
| `w`  | 7d utilization % |
| `wr` | minutes until 7d reset |
| `st` | 5h status string |
| `ok` | poll succeeded |

## Pin map (same board as the button demo)

| EPD signal | GPIO | | EPD signal | GPIO |
|---|---|---|---|---|
| SCK | 12 | | DC | 10 |
| MOSI | 13 | | RST | 9 |
| CS | 11 | | BUSY | 8 |
| PWR gate | 6 (LOW=on) | | | |

## Build / flash

```bash
pio run                # build
pio run -t upload      # flash (auto-detects port)
pio device monitor -b 115200
```

## Run the daemon (drives the display)

From a clone of the Clawdmeter repo:

```bash
cd Clawdmeter/daemon
pip install -r requirements.txt   # bleak, httpx
python3 claude_usage_daemon.py
```

It reads your Claude Code token (macOS Keychain `Claude Code-credentials`, or
`~/.claude/.credentials.json`), finds the BLE device named `Clawdmeter`, and
pushes a payload every ~60 s.

## Testing

Expected serial (115200):

```
advertising as Clawdmeter
connected
payload: {"s":42,"sr":118,"w":7,"wr":9320,"st":"allowed","ok":true}
```

Expected screen: `waiting BLE...` on boot → `disconnected` if daemon drops →
two labeled blocks (`5h` / `7d`) each with a big %, a filled bar, and a reset
line, once payloads arrive.

> Hardware runtime not yet captured. Flash, run the daemon, and paste real
> serial + a photo here.

## Resource usage

Build: `pio run`, env `esp32-s3-epaper-154`, ESP32-S3-PICO-1-N8R8, Arduino.

| Segment | Used    | Total     | %     |
|---------|---------|-----------|-------|
| RAM     | 38492 B | 327680 B  | 11.7% |
| Flash   | 730157 B| 3342336 B | 21.8% |
