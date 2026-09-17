# Ear-Brain / XBud — Current Model Architecture Specification

## 1. System-Level Edge-AI Pipeline Architecture

The Ear-Brain system operates 100% on-device inside the smart earbud charging case (Rockchip RK3576 SoC). The system runs a single C++20 daemon (`orbitd`) orchestrated with real-time POSIX threads (`SCHED_FIFO`) and strict sandboxing (`seccomp`, zero network socket access, volatile tmpfs audio buffers).

```mermaid
flowchart TD
    subgraph Hardware_FrontEnd ["1. Hardware Front-End"]
        Phone[Phone HFP/SCO] -->|Bluetooth 5.3| BES[BES2600IWP Audio SoC]
        Earbuds[TWS Earbuds TX] -->|Bluetooth A2DP/HFP| BES
        BES -->|I2S 16kHz Stereo PCM| RK[Rockchip RK3576 SoC]
    end

    subgraph Audio_Ingest ["2. Audio Ingestion & VAD"]
        RK --> Capture[orbit-audio Thread (SCHED_FIFO 99, CPU 4)]
        Capture --> RingBuf[Lock-Free Ring Buffer (Volatile tmpfs)]
        RingBuf --> VAD_Thread[orbit-vad-asr Thread (SCHED_FIFO 90, CPU 5-6)]
        VAD_Thread --> Energy[Energy Detector (Talk-Time Analytics)]
        VAD_Thread --> Silero[Silero VAD (ONNX INT8, 30ms / 480 samples)]
    end

    subgraph ASR_Layer ["3. ASR & Acoustic Processing"]
        Silero -->|Speech Frames (p > 0.5)| WhisperEng[WhisperEngine (whisper.cpp)]
        WhisperEng -->|Audio Segments| WhisperModel[Whisper Base / IndicWhisper (74M, INT8 NEON)]
        WhisperModel -->|Auto Language Detection| LID[LID Tokens: &lt;|hi|&gt;, &lt;|en|&gt;, &lt;|ta|&gt;, &lt;|bn|&gt;]
        LID --> RawText[Raw Transcribed Tokens]
    end

    subgraph Normalization_PII ["4. Normalization & DPDP PII Scrubbing"]
        RawText --> Normalizer[Text Normalizer (RapidFuzz + Hotword Dict)]
        Normalizer --> RegexPrefilter[Regex Pre-Filter (Aadhaar, PAN, Phone, OTP)]
        RegexPrefilter --> DistilBERT[DistilBERT NER (ONNX INT8, Token Classification)]
        DistilBERT --> ScrubbedText[Sanitized & Scrubbed Utterance]
        ScrubbedText --> TranscriptBuf[Transcript Buffer]
    end

    subgraph SLM_Layer ["5. SLM Reasoning & Structured Generation"]
        TranscriptBuf -->|30s Chunks / Wake Trigger| GemmaEng[orbit-slm Thread (CPU 4-7 / RKNN NPU)]
        GemmaEng --> GemmaModel[Gemma 3 1B (Q4_K_M, llama.cpp)]
        GemmaEng --> GBNF[GBNF Grammar Constrained Decoding]
        GBNF --> OutJSON[Structured JSON: Action Items, Calendar, Summary, CRM]
    end

    subgraph Storage_Comms ["6. Storage & Egress"]
        ScrubbedText --> BLE[BLE GATT Server]
        OutJSON --> BLE
        BLE --> Companion[Companion App (Flutter iOS/Android)]
        ScrubbedText --> SQLCipher[SQLCipher DB (AES-256 on eMMC)]
        OutJSON --> SQLCipher
    end
```

---

## 2. ASR Neural Architecture (Whisper Base / IndicWhisper)

The primary speech-to-text model is a Sequence-to-Sequence Encoder-Decoder Transformer based on the Whisper Base architecture (74 Million parameters), optimized for Indic multilingual transcription with dynamic language identification.

### Architectural Breakdown

```
[Audio: 16 kHz Mono PCM]
         │
         ▼
[Log-Mel Filterbank Extraction] (80 channels, 25ms window, 10ms step)
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│                       AUDIO ENCODER                         │
│                                                             │
│  Conv1D (stride=1, kernel=3, 80 -> 512) + GELU              │
│  Conv1D (stride=2, kernel=3, 512 -> 512) + GELU  (2x down)  │
│  Conv1D (stride=2, kernel=3, 512 -> 512) + GELU  (4x total) │
│  + Sinusoidal 1D Positional Embeddings                      │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ 6x Transformer Encoder Layers:                        │  │
│  │  - LayerNorm (Pre-LN)                                 │  │
│  │  - Multi-Head Self-Attention (d_model=512, 8 heads)   │  │
│  │  - LayerNorm (Pre-LN)                                 │  │
│  │  - Feed-Forward Network (d_ffn=2048, GELU activation) │  │
│  │  - Residual Connections                               │  │
│  └───────────────────────────────────────────────────────┘  │
│  Final LayerNorm                                            │
└─────────────────────────────────────────────────────────────┘
         │
         ▼ (Encoder Hidden States: 25 representations/sec)
         │
┌────────┴────────────────────────────────────────────────────┐
│                       TEXT DECODER                          │
│                                                             │
│  Token Embedding (Vocab: 51,865 multilingual BPE tokens)    │
│  + Learned 1D Positional Embeddings                         │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ 6x Transformer Decoder Layers:                        │  │
│  │  - LayerNorm (Pre-LN)                                 │  │
│  │  - Masked Multi-Head Self-Attention (8 heads)         │  │
│  │  - LayerNorm (Pre-LN)                                 │  │
│  │  - Multi-Head Cross-Attention (to Encoder outputs)    │  │
│  │  - LayerNorm (Pre-LN)                                 │  │
│  │  - Feed-Forward Network (d_ffn=2048, GELU)            │  │
│  │  - Residual Connections                               │  │
│  └───────────────────────────────────────────────────────┘  │
│  Final LayerNorm + Linear Projection to Vocab Logits        │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
[Autoregressive Softmax & Greedy/Beam Decoding]
```

### Whisper Base Dimensional Specifications

| Hyperparameter | Value | Description |
|:---|:---|:---|
| **Total Parameters** | 74,057,472 (~74M) | Lightweight enough for edge memory budget |
| **Model Dimension ($d_{model}$)** | 512 | Embedding and hidden state size |
| **Attention Heads ($n_{heads}$)** | 8 | 64 dimensions per head |
| **Encoder Layers ($n_{enc\_layers}$)** | 6 | Audio representation feature depth |
| **Decoder Layers ($n_{dec\_layers}$)** | 6 | Autoregressive language generation depth |
| **FFN Intermediate Dimension ($d_{ffn}$)** | 2048 | 4x expansion in feed-forward blocks |
| **Input Audio Representation** | 80 log-mel bins | Computed every 10ms over 25ms window |
| **Acoustic Downsampling Factor** | 4x | 100 Hz input downsampled to 25 Hz encoder states |
| **Vocabulary Size** | 51,865 | Multilingual byte-pair encoding (BPE) |
| **Max Context Window** | 30.0s (448 text tokens) | Segment-level chunking with Local Agreement Policy |
| **Quantization Format** | INT8 (Q8_0 / GGML) | Vectorized using ARM NEON on Cortex-A72 |
| **Memory Footprint** | ~140 MB (INT8) / ~290 MB (FP16) | Fits comfortably in 4GB SoC budget |

---

## 3. Supporting Neural Models & Components

| Component | Model / Engine | Parameters | Quantization | Hardware Execution Target | Purpose |
|:---|:---|:---|:---|:---|:---|
| **VAD** | Silero VAD v4 | ~2M | ONNX INT8 | Cortex-A72 (CPU 5) | Detects speech vs silence with 30ms window, saves >60% ASR compute |
| **ASR** | Whisper Base / IndicWhisper | 74M | INT8 (`whisper.cpp`) | Cortex-A72 (CPU 5-6, NEON) | Dual-channel speech-to-text with auto language detection |
| **Normalizer** | Rule + RapidFuzz | N/A | C++ / Python | Cortex-A72 | Hotword injection, proper noun fuzzy correction (threshold 80) |
| **PII / NER** | DistilBERT Multilingual + Regex | 66M | ONNX INT8 | Cortex-A72 / RKNN | Masks Indian PII (Aadhaar, PAN, phone, names) for DPDP Act 2023 |
| **SLM** | Gemma 3 1B | 1.0B | Q4_K_M (`llama.cpp`) | RKNN NPU (6 TOPS) / CPU 4-7 | Post-call & real-time JSON action items, meeting summary, CRM sync |

---

## 4. Hardware Resource & Thread Allocation (RK3576 SoC)

| Core / Subsystem | Thread Name | Priority | Role | Latency SLA |
|:---|:---|:---|:---|:---|
| **CPU 4 (Cortex-A72 @ 2.2GHz)** | `orbit-audio` | `SCHED_FIFO 99` | I2S PCM capture, ring buffer writes | < 10ms frame |
| **CPU 5-6 (Cortex-A72 @ 2.2GHz)** | `orbit-vad-asr` | `SCHED_FIFO 90` | Silero VAD inference + Whisper INT8 ASR | < 300ms per 3s chunk |
| **CPU 4-7 / RKNN NPU (6 TOPS)** | `orbit-slm` | `SCHED_OTHER 0` | Gemma 3 1B batched inference (every 30s) | < 1500ms response |
| **CPU 0-3 (Cortex-A53 @ 1.8GHz)** | System / Comms | Normal | BLE GATT server, SQLCipher I/O, watchdog, thermal governor | Non-blocking |

### Memory Budget Allocation (4.0 GB LPDDR4x Total)

- **Linux Kernel & Musl OS**: ~150 MB
- **Orbitd Core & Volatile Ring Buffer (`tmpfs`)**: ~128 MB (strictly capped)
- **Silero VAD (ONNX)**: ~30 MB
- **Whisper Base (INT8)**: ~140 MB
- **DistilBERT NER (INT8)**: ~120 MB
- **Gemma 3 1B (Q4_K_M)**: ~850 MB
- **KV Cache (2048 tokens)**: ~250 MB
- **SQLCipher & System Headroom**: ~2,332 MB
- **Total In-Use Under Full Load**: **~1,668 MB (< 42% of 4GB capacity)**
