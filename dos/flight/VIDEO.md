# Banked 800×600 indexed video backend

`video.h` and `video.c` expose a C99 DJGPP framebuffer interface. The caller owns
one 480,000-byte indexed framebuffer and supplies 256 RGB palette entries with
components in the VGA DAC range 0–63. `video_open` queries VBE mode 103h through
a 256-byte DOS transfer buffer, validates the exact 800×600×8 packed-pixel layout,
sets the mode, and writes the DAC. It requires a writable window A at A000h,
at least a 64 KiB window, and a granularity that divides 64 KiB. The buffer is
freed after the query. `video_close` restores mode 3.

`video_present` switches each of eight banks and copies one 64 KiB slice per
bank, except the final 21,248-byte slice. `video_verify` samples five positions
in each bank and reads all 768 DAC components back. This is a diagnostic check,
not a full VRAM comparison. A failed call returns zero; `video_error()` gives a
static error string. `video_text` draws an original uppercase 5×7 bitmap font;
lowercase input maps to uppercase, unsupported characters leave an empty cell,
and pixels are clipped to the framebuffer. Each function remains under 500 LOC.

Reproduction from the checkout root, using the cached toolchain and image:

```sh
mkdir -p .work/flight-video
cp .work/dos-probe/CWSDPMI.EXE .work/flight-video/CWSDPMI.EXE
docker run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/work" -w /work modern-arena-tools:0.1 \
  .work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc \
  -std=gnu99 -O2 -Wall -Wextra -Werror -march=i386 -mtune=i586 \
  -mno-mmx -mno-sse -mno-sse2 dos/flight/video.c dos/flight/video_probe.c \
  -o .work/flight-video/VIDTEST.EXE
```

The local run used `.work/flight-video/flight.conf`, derived from
`dos/probes/runtime.conf`: same S3 SVGA, 16 MiB, `pentium_slow`, fixed 20,000
cycles, no swap (`CWSDPMI -s-`); its autoexec mounts `.work/flight-video` and
executes `VIDTEST.EXE > VIDEO.TXT`. Run with the same Docker mount and
`timeout 90s xvfb-run -a dosbox -conf .work/flight-video/flight.conf`.

Observed 2026-09-27: `status=pass`; three distinct pattern frames plus the final
timed frame checked across all eight banks; 768 palette components read back.
The probe made 120 additional `video_present` calls with drawing excluded and
measured 13 BIOS ticks (~0.71 s total). This is a copy-only DOSBox observation,
not game FPS or a calibrated hardware result. Drawing and simulation timing have
not been measured here. Raw guest output and DOSBox log are ignored under
`.work/flight-video/`.
