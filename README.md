# ⚡ HANSI ZOE AI GOGGLES

**DIY AI-Powered Smart Goggles with ESP32-S3, GPT-4o Vision, and HUD Display**

*Inspired by Hange Zoe from Attack on Titan*

---

## What Is This?

Open-source smart goggles that give you real-time AI vision, instant translation, a voice assistant, and an AR heads-up display — all for under 150 EUR.

Point the goggles at anything. The AI tells you what it sees, translates text in 12+ languages, answers your questions with full conversational memory, and projects the results onto a tiny prism display in your field of view.

## Features

**Scout Mode** — Point at objects, components, text, or scenes. GPT-4o Vision identifies everything in 2-4 seconds. Includes OCR, technical component identification, and navigation sub-modes.

**Babel Mode** — Real-time translation from 12+ languages. Works on signs, documents, menus, or spoken language. Auto-detects source language. TTS reads the translation aloud.

**Titan Mode** — Hands-free conversational AI with RAG memory. Ask questions, get spoken answers. Hansi remembers past observations and can answer "What did I see earlier?" using vector similarity search.

**Survey Mode** — Automatic life-logging. Captures and AI-describes your surroundings every 10 seconds. Builds a searchable memory archive.

**Combat Mode** — Navigation with landmark detection and HUD waypoints (experimental).

## Architecture

```
┌─────────────────┐      BLE       ┌─────────────────┐      API      ┌──────────┐
│  XIAO ESP32-S3  │◄──────────────►│   Phone App     │◄────────────►│  AI Cloud │
│  Camera + Mic   │  Image/Audio   │  (Companion)    │  Vision/STT  │  GPT-4o   │
│  VAD + BLE      │  Status/Cmd    │  AI Router      │  TTS/Embed   │  Claude   │
│                 │                │  RAG Memory     │              │  Groq     │
└────────┬────────┘                └─────────────────┘              └──────────┘
         │ BLE (HUD relay)
┌────────▼────────┐
│ T-Glass V2 HUD  │
│ 126x126 AMOLED  │
│ Prism Display   │
└─────────────────┘
```

## Hardware (~150 EUR)

| Component | Model | Cost |
|-----------|-------|------|
| Base Frame | Uvex Flex Seal OTG Goggles | 15 EUR |
| Compute | Seeed XIAO ESP32-S3 Sense | 14 EUR |
| HUD Display | LILYGO T-Glass V2 (AMOLED + Prism) | 48 EUR |
| Battery | LiPo 3.7V 1000mAh | 5 EUR |
| Misc | Wires, connectors, 3D print, paint | ~30 EUR |

## Software

**Firmware** (C/C++ Arduino):
- `hansi_goggles_v3.ino` — Camera unit with VAD, BLE server, HUD relay
- `hansi_hud_v1.ino` — T-Glass LVGL display with BLE client

**Companion App** (HTML5/JS):
- `hansi_companion_v4.html` — Multi-provider AI, Whisper STT, TTS, RAG memory, Babel translation

## Quick Start

1. Flash `hansi_goggles_v3.ino` to XIAO ESP32-S3 Sense
2. Flash `hansi_hud_v1.ino` to LILYGO T-Glass V2
3. Open `hansi_companion_v4.html` in Chrome
4. Enter your OpenAI API key
5. Click BLE → Connect to "HansiGoggles"
6. Select a mode and start exploring

No goggles yet? Use **Demo Mode** — it uses your webcam as a simulated camera.

## API Costs

| Service | Cost | Usage |
|---------|------|-------|
| GPT-4o-mini Vision | ~$0.0004/image | Scout, Babel |
| Whisper STT | ~$0.006/min | Voice input |
| Embeddings (256d) | ~$0.00002/1K tokens | RAG memory |
| Browser TTS | Free | Voice output |
| Groq Llama 3.3 | Free (30 req/min) | Titan chat |

Typical session cost: **$0.05-0.15/hour**

## Build Guide

See [docs/assembly_guide.md](docs/assembly_guide.md) for the complete step-by-step build with wiring, 3D printing, firmware flashing, and Hansi Zoe aesthetic finishing.

## Tech Stack

ESP32-S3, NimBLE, LVGL, I2S PDM, FreeRTOS, Web Bluetooth, Web Audio API, Web Speech API, OpenAI API, Anthropic API, Groq API, OpenSCAD, PlatformIO

## License

MIT — Build your own, modify freely, share with others.

---

*Built by Sola — TH Nurnberg B-AMP — AlgoSphere Quant — ExMachina Labs — 2026*

*"If you don't try, you'll never know." — Hange Zoe*
