# 🎵 Video Viewer

A cross-platform **Qt/C++ audio & video analysis tool** featuring real-time spectrogram, frequency spectrum, and waveform visualizations — with full video playback support and an interactive draggable timeline.

---

## ✨ Features

### 🎬 Video Support (OpenCV)
- Loads **MP4, MOV, MKV, AVI, WebM** and any format supported by OpenCV
- **VideoPlayerWidget** displays the current video frame in sync with playback
- Frame updates on **drag-and-drop** of the playhead (seeks instantly)
- **Thumbnail strip** in the timeline — auto-generated in a background thread, one thumbnail every ~5 seconds

### 🔊 Audio Support (libsndfile + FFmpeg)
- **libsndfile** for lossless formats: WAV, FLAC, OGG, AIFF
- **FFmpeg** for compressed formats: MP3, MP4, M4A, AAC, MOV, MKV…
- Audio decoded to interleaved `float32` PCM, background thread (UI never freezes)

### 📊 Three Visualization Modes (switchable at runtime)

| Mode | Description |
|---|---|
| 🌈 **Spectrogram** | 2-D heat-map — time → X, frequency → Y, energy → colour (inferno palette) |
| 📊 **Frequency** | FFT magnitude spectrum at playhead position — gradient fill, smoothing, peak-hold dots |
| 〰 **Amplitude** | Full PCM waveform with min/max per pixel + RMS envelope overlay |

- Frequency view supports **logarithmic or linear** X axis (`Alt+L`)
- All FFT work done with **FFTW3** (single-precision, `r2c`, Hann window, 2048-pt frames)

### ⏯ Transport
- **Play / Pause / Stop** with PortAudio output
- **Draggable playhead** — click or drag anywhere on the timeline
- Cursor changes to resize arrow when hovering the playhead
- Time counter display `M:SS.cs / M:SS.cs`

---

## 🖼 Layout

```
┌────────────────────────────────────────────────────────────┐
│  top bar:  filename                [🌈 Spectro][📊 Freq][〰 Amp][Log]  │
├─────────────────────┬──────────────────────────────────────┤
│                     │                                      │
│  VideoPlayerWidget  │     SpectrogramWidget                │
│  (video frame,      │     (spectrogram / freq / amplitude) │
│   letterboxed)      │                                      │
│                     │  ┌──────────────────────────────────┐│
│                     │  │  thumbnail strip  │  time axis   ││
│                     │  └──────────────────────────────────┘│
├─────────────────────┴──────────────────────────────────────┤
│  ▶  ⏸  ■       0:12.34 / 3:45.00                          │
└────────────────────────────────────────────────────────────┘
```

> The `VideoPlayerWidget` is automatically **hidden** for audio-only files.

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         MainWindow                              │
│                                                                 │
│   File > Open                                                   │
│       │                                                         │
│       ├──► AudioDecoder ──────────────────────────────────────┐ │
│       │    (QtConcurrent thread)                               │ │
│       │    libsndfile  → WAV / FLAC / OGG                     │ │
│       │    FFmpeg      → MP3 / MP4 / AAC / …                  │ │
│       │                                                        ▼ │
│       │              AudioPlayer (PortAudio, audio thread)     │ │
│       │                    │  positionChanged(s)               │ │
│       │                    ▼                                   │ │
│       │           SpectrogramWidget ◄──── float32 samples ─────┘ │
│       │           (FFTW3, 3 modes, thumbnail strip)              │
│       │                    │  seekRequested(s)                   │
│       │                    ▼                                     │
│       └──► VideoDecoder ──────────────────────────────────────►  │
│            (OpenCV)                                              │
│            cv::VideoCapture (main)   → frameAt(s)               │
│            cv::VideoCapture (thumb)  → generateThumbnailsAsync   │
│                    │  thumbnailsReady                            │
│                    ▼                                             │
│           VideoPlayerWidget                                      │
│           (letterbox, timecode overlay)                          │
└─────────────────────────────────────────────────────────────────┘
```

### Class responsibilities

| Class | Role | Key library |
|---|---|---|
| `AudioDecoder` | Decodes audio → `float32` PCM | **libsndfile**, **FFmpeg** |
| `AudioPlayer` | Streams PCM to sound card | **PortAudio** |
| `SpectrogramWidget` | 3-mode visualizer + thumbnail timeline + drag playhead | **FFTW3**, Qt |
| `VideoDecoder` | Seeks video frames, generates thumbnails | **OpenCV** |
| `VideoPlayerWidget` | Displays video frames letterboxed | Qt |
| `MainWindow` | Orchestrates everything | Qt |

---

## 📦 Dependencies

| Library | Purpose | Min version |
|---|---|---|
| **Qt** | UI framework | 5.15 or 6.x |
| **PortAudio** | Audio playback | v19 |
| **libsndfile** | WAV / FLAC / OGG decode | 1.0 |
| **FFTW3** (float) | Fast Fourier Transform | 3.x |
| **FFmpeg** | MP3 / MP4 / AAC / … decode | 4.x or 5.x |
| **OpenCV** | Video frame seeking + thumbnails | 4.x |

---

## 🔧 Build

### Install dependencies

**Ubuntu / Debian**
```bash
sudo apt install \
  qtbase5-dev qt5-qmake \
  libportaudio2 libportaudio-dev \
  libsndfile1-dev \
  libfftw3-dev \
  libavformat-dev libavcodec-dev libavutil-dev libswresample-dev \
  libopencv-dev \
  pkg-config cmake build-essential
```

**macOS (Homebrew)**
```bash
brew install qt portaudio libsndfile fftw ffmpeg opencv pkg-config cmake
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH"
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

---

### CMake (recommended)
```bash
git clone https://github.com/your-username/spectrogram-viewer.git
cd spectrogram-viewer

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

./SpectrogramViewer
```

### qmake
```bash
qmake spectrogram_viewer.pro
make -j$(nproc)
./SpectrogramViewer
```

---

## 🎮 Usage

### Load a file
`Fichier > Ouvrir`  or  `Ctrl+O`

Supported formats: `.wav` `.flac` `.ogg` `.aiff` `.mp3` `.mp4` `.m4a` `.aac` `.mov` `.mkv` `.avi` `.webm`

- **Audio files** → spectrogram + waveform + frequency views
- **Video files** → all of the above + video player (left panel) + thumbnail strip in timeline

### Switch visualization mode

| Shortcut | Mode |
|---|---|
| `Alt+S` | 🌈 Spectrogramme |
| `Alt+F` | 📊 Fréquence |
| `Alt+A` | 〰 Amplitude |
| `Alt+L` | Toggle log / linear freq axis |

### Playback
| Button | Action |
|---|---|
| `▶` | Play |
| `⏸` | Pause (resume with `▶`) |
| `■` | Stop + rewind |

### Seeking
- **Click** anywhere on the spectrogram / waveform / thumbnail strip to jump to that position
- **Drag** the white playhead left or right at any time (even during playback)
- The cursor changes to ↔ when hovering within 8 px of the playhead line
- The **VideoPlayerWidget** updates instantly on every drag move

---

## 📁 File Structure

```
├── AudioDecoder.h / .cpp       ← audio file decoding (libsndfile + FFmpeg)
├── AudioPlayer.h  / .cpp       ← PortAudio playback engine
├── SpectrogramWidget.h / .cpp  ← FFTW3 FFT, 3 visualization modes, thumbnail timeline
├── VideoDecoder.h / .cpp       ← OpenCV video seeking + async thumbnail generation
├── VideoPlayerWidget.h / .cpp  ← letterbox video frame display widget
├── MainWindow.h   / .cpp       ← main application window
├── main.cpp                    ← entry point
├── CMakeLists.txt              ← CMake build (recommended)
```

---

## ⚙️ Technical details

### FFT / Spectrogram
- **Engine**: FFTW3 single-precision (`fftwf_plan_dft_r2c_1d`)
- **Frame size**: 2048 samples — **Hop size**: 512 samples (75 % overlap)
- **Window**: Hann
- **Colour map**: 9-stop inferno palette (black → deep-purple → orange → yellow-white)

### Frequency view
- Exponential smoothing (`α = 0.45`) for fluid animation at 30 Hz
- Peak-hold: 20-frame hold then 1.2 dB/frame decay
- Log axis: `log10(20 Hz … Nyquist)`

### Amplitude view
- Min/max PCM per horizontal pixel → accurate rendering at any zoom level
- RMS envelope pre-computed in 512-sample blocks

### Video thumbnails
- A **separate** `cv::VideoCapture` instance is used for thumbnail extraction so it never conflicts with live frame seeking
- Seeks via `CAP_PROP_POS_MSEC` for broad codec compatibility
- Thumbnails arrive via `Qt::QueuedConnection` signal from the background thread

---

## 📄 License

MIT — see [LICENSE](LICENSE) for details.

---

## 🤝 Contributing

Pull requests are welcome! For major changes, please open an issue first to discuss what you would like to change.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request
