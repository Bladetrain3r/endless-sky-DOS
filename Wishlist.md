# Endless Sky DOS

## Reasons I want to give it a shot:
1) A serious challenge for optimization
  - The game logic itself is probably fairly lightweight
  - Backporting to a decent 2D based renderer is the challenge

2) Lessons about space sim game design which can transfer to Space Denizen

3) A test - purely autonomous, goals largely already defined, decisions left to agent until baked.

## Success Metrics

- Reasonably complete parity with the vanilla game experience
- Reliable build + package process into a DOS executable 
- Run on 16MB of RAM and ~20,000 cycles on DOSBox (around Pentium/Pentium Pro)
- SVGA - 800x600 if possible, 1024x768 if not
  - So backporting from OpenGL
  - It's all software rendering in an emulator anyway so... optimized software rendering
- Assets targeting lower, fixed res
- 30FPS core target, 60FPS if possible. Fixed.

### Softer gives
- Lower crowd density if it doesn't affect the game too much
- More localized economy or slower ticks/events
- SFX and shader dependent things can be forgone or replaced with something lighter weight

### Changes to Audio
- Port OST to midi to preserve compute on audio decode (OPL3 or MPU-401)
  - Or better yet, create tracks of your own.
- Soundblaster SFX. Resample and convert existing?

### Some Adds
- Setup util for soundcard
- Benchmark util (standalone, 3x preset demos: Standard traffic, busy traffic, battle)

### Some caution or suggestions from the operator
- Cleanup source/builds regularly to prevent bloat. Keep records well, less so every artifact.
- Docker for build + test with headless display, ala Modern Arena
- Take special note of space sim design choices that might benefit Space Denizen
- Luna, Terra and Sol as subagents - don't hesitate call on them.