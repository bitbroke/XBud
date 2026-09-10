# Ear-Brain — Edge AI Call Intelligence

> The first self-contained, pocketable edge-AI audio wearable delivering zero-cloud, real-time telephony intelligence.

## Overview

Ear-Brain is an AI-integrated True Wireless Stereo (TWS) earbud system where **100% of all acoustic transcription, PII redaction, and SLM inference occurs inside the earbud charging case**. The smartphone acts only as an unprivileged display client.

**Target Market:** India (Urban professionals, enterprise sales, independent consultants)  
**Retail Price:** ₹7,499 – ₹8,999  
**Privacy:** DPDP Act 2023 compliant by design — voice audio never leaves volatile RAM on the case.

## Architecture

```
PHONE ──(HFP/SCO)──> BES2600 ──(I2S)──> RK3576 SoC
                                            │
                                   ┌────────┴────────┐
                                   │                  │
                              TX (User)          RX (Caller)
                                   │                  │
                                   └────────┬─────────┘
                                            │
                               Silero VAD → IndicWhisper ASR
                                            │
                               DistilBERT NER PII Scrubber
                                            │
                               Gemma 3 1B (GBNF Constrained)
                                            │
                               BLE GATT → Companion App
```

## Repository Structure

```
project_xbud/
├── docs/                    # PRD, architecture, hardware tracker
├── firmware/
│   ├── buildroot/           # Buildroot external tree for embedded Linux
│   ├── orbitd/              # Main C++ daemon (the brain)
│   │   ├── src/             # Source code (core, audio, vad, asr, ner, slm, comms, storage, privacy)
│   │   ├── models/          # Quantized model binaries (not in git — use LFS or download)
│   │   ├── grammars/        # GBNF grammar files for structured JSON output
│   │   ├── tests/           # Unit, integration, and benchmark tests
│   │   └── scripts/         # Benchmark and profiling scripts
│   └── kernel/              # Stripped kernel defconfig
├── companion-app/           # Flutter mobile app (BLE client)
├── evaluation/              # ASR, NER, SLM, and system evaluation tools
├── hardware/                # Schematics, BOM, mechanical CAD
└── README.md
```

## Key Specifications

| Metric | Target |
|:-------|:-------|
| End-to-End Latency | < 600ms |
| ASR Word Error Rate (Hinglish) | < 14% |
| SLM Schema Compliance | > 95% |
| Peak RAM Usage | < 3.2 GB (of 4 GB) |
| Thermal Ceiling | < 42°C skin at 32°C ambient |
| Battery Life | > 3.5 hours continuous |
| Factory BOM | $32–$39 |

## Tech Stack

| Component | Technology |
|:----------|:----------|
| SoC | Rockchip RK3576 (6 TOPS NPU) |
| OS | Buildroot Linux (stripped, musl, BusyBox init) |
| ASR | IndicWhisper Base (INT8, whisper.cpp) |
| VAD | Silero VAD (ONNX Runtime) |
| NER | DistilBERT (INT8, ONNX Runtime) |
| SLM | Gemma 3 1B (Q4_K_M, llama.cpp/rk-llama.cpp) |
| Bluetooth | BES2600IWP (HFP+A2DP, I2S, BLE 5.3) |
| Storage | SQLCipher (AES-256) on eMMC |
| Companion App | Flutter (iOS + Android) |

## Getting Started (Prototype)

1. **Hardware**: Obtain Orange Pi Zero 4 (8GB) + USB Bluetooth dongle
2. **Build**: See `firmware/buildroot/` for Buildroot configuration
3. **Models**: Download quantized models to `firmware/orbitd/models/`
4. **Run**: Cross-compile `orbitd` and deploy to the SBC
5. **Test**: Use `evaluation/` scripts to benchmark ASR/SLM performance

## License

Proprietary. All rights reserved.
