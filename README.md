# Loopfetch

Animated fetch written in c++ for `fastfetch`/`neofetch`.


## Requirements

- `g++` with C++17, `make`, `pkg-config`
- FFmpeg libs  (`libavformat`, `libavcodec`, `libavutil`, `libswscale`)
- `ffmpeg` + `ffprobe`
- `chafa` (video → ASCII)
- `fastfetch` (or `neofetch` with `-n` arg)

## Compilate

```bash
make # Compile
make clean # Clean workspace
```

## Use

```bash
./loopfetch -p video.mp4                 # infinite loop
./loopfetch -p video.mp4 --width 80     # width (or height)
./loopfetch -p video.mp4 -n             # use neofetch instead of fastfetch
```