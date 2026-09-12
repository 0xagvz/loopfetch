<div align="center">

# Loopfetch

**Animated video fetch for your terminal.**
Plays any video as looping ASCII art next to your `fastfetch`/`neofetch` output.

</div>

---

## What is this?

`loopfetch` decodes a video with FFmpeg, converts each frame to ASCII with `chafa`, and loops the animation right beside your system info (`fastfetch` or `neofetch`). Frames get cached so replays are instant.

## Requirements

| Dependency | Used for |
|---|---|
| `g++` (C++17), `make`, `pkg-config` | Building |
| `libavformat`, `libavcodec`, `libavutil`, `libswscale` (FFmpeg libs) | Decoding video |
| `ffmpeg` + `ffprobe` | Frame extraction |
| `chafa` | Video → ASCII conversion |
| `fastfetch` (or `neofetch` with `-n`) | System info display |

## Build

```bash
make          # compile
make clean    # remove build artifacts
```

## Install

```bash
sudo make install
# or manually:
sudo cp loopfetch /usr/local/bin/
```

## Usage

```bash
loopfetch -p video.mp4
```

### Options

| Flag | Description |
|---|---|
| `-p`, `--path` | Path to the video file |
| `--width` | Output width (`0` = auto; if alone, height follows aspect ratio) |
| `--height` | Output height (`0` = auto; if alone, width follows aspect ratio) |
| `-f`, `--fps` | Frames per second (`0` = original video fps) |
| `-o`, `--output` | Output dir for frames (default: hashed dir under `$XDG_CACHE_HOME/loopfetch`) |
| `-n`, `--neofetch` | Use `neofetch` instead of `fastfetch` |
| `--fetch-config` | Config file for `fastfetch`/`neofetch` |
| `--top` | Empty rows above the layout |
| `--left` | Spaces to the left of the layout |
| `--gap` | Spaces between the ASCII animation and the fetch text |
| `--loops` | Playback loops (`0` = infinite) |
| `--clean` | Delete cached frames and exit |

### Examples

```bash
loopfetch -p video.mp4                  # infinite loop, default size
loopfetch -p video.mp4 --width 80       # fixed width, height keeps aspect ratio
loopfetch -p video.mp4 -n               # use neofetch instead of fastfetch
loopfetch -p video.mp4 --loops 3        # play 3 times and stop
loopfetch --clean                       # wipe the frame cache
```