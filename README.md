# CackalackyCon2026
Hack Your Guitar Tone II

## Part 1: Benchmark

The benchmark exists to answer one practical question:

> Can this machine process audio fast enough without crackles, dropouts, or weird timing problems?

Real-time audio is deadline-based. It is not enough for code to be fast on average. Every buffer has to finish before the audio device needs the next one.

At 48,000 Hz:

```text
64 frames  = 1.33 ms
128 frames = 2.67 ms
256 frames = 5.33 ms
512 frames = 10.67 ms
```

A 128-frame buffer gives about 2.67 ms for the whole audio path.

That includes:

```text
ALSA read
C++ work
Lua work
buffer copying
ALSA write
operating system scheduling
USB audio overhead
```

The benchmark helps show whether there is enough headroom.

---

## Benchmark Type 1: Raw CPU Benchmark

The raw CPU benchmark checks basic processing speed without relying on the full audio path.

It answers:

```text
How fast can this machine run DSP-style work?
How much CPU headroom is available?
Does this device look powerful enough for real-time audio?
```

Typical settings:

```text
Sample rate: 48000
Buffer size: 128 or 256
Channels: 2
Test length: a few seconds
```

What matters:

```text
Average time: useful
Worst-case time: more important
Spikes: dangerous
Stable timing: good
```

The worst-case time matters because one slow buffer can cause an audible glitch.

---

## Benchmark Type 2: Buffer Deadline Benchmark

The buffer deadline benchmark checks whether processing finishes within the available buffer time.

Example at 48,000 Hz:

```text
128 frames = 2.67 ms
```

That does not mean the DSP should take 2.66 ms.

That is too close to the edge.

A better target is:

```text
Excellent: under 20% of buffer time
Good:      under 30% of buffer time
Safe:      under 50% of buffer time
Risky:     over 70% of buffer time
Bad:       near 100% of buffer time
```

For a 128-frame buffer:

```text
Buffer time: 2.67 ms

Excellent: under ~0.53 ms
Good:      under ~0.80 ms
Safe:      under ~1.33 ms
Risky:     over ~1.87 ms
Bad:       near ~2.67 ms
```

The extra time is not wasted. It is the safety margin for scheduler jitter, ALSA behavior, USB timing, and random system weirdness.

---

## Benchmark Type 3: Real Audio Benchmark

The real audio benchmark uses the actual audio path.

It checks the full chain:

```text
USB audio input
ALSA capture
C++ processing
Lua processing
ALSA playback
USB audio output
```

This matters because a processor can look fine in isolation but still fail in the real audio path.

Typical causes:

```text
USB audio hiccups
ALSA read/write timing
OS scheduling jitter
buffer underruns
driver behavior
memory/cache spikes
```

Typical settings:

```text
Sample rate: 48000
Channels: 2
Format: float
Buffer size: 128 for low latency
Buffer size: 256 for safer testing
```

Practical guide:

```text
128 frames = low latency, harder to keep stable
256 frames = safer, often still comfortable
512 frames = very safe, more noticeable latency
```

If 128 crackles but 256 works, the system is close but needs more headroom.

That is useful information, not failure.

---

## Recommended Benchmark Settings

Start here:

```text
Sample rate: 48000
Buffer size: 256
Channels: 2
Format: float
```

Then try lower latency:

```text
Sample rate: 48000
Buffer size: 128
Channels: 2
Format: float
```

If 128 is unstable, use 256 until the DSP path is faster.

The goal is not just “it runs.”

The goal is “it runs with room to breathe.”

---

## Part 2: Simple DSP Host

The DSP host is a small C++ app that does this:

```text
USB audio input -> Lua effect -> USB audio output
```

The C++ app:

```text
Finds the first USB ALSA audio device
Opens stereo input and output
Asks for an effect name
Loads the matching Lua file
Lets Lua ask for its own setup parameters
Calls Lua for each stereo audio frame
Outputs the processed stereo audio
```

Simple. Direct. Clean.

---

## Effect File Naming

When the app asks for an effect name, enter a short name.

Example:

```text
Effect name: volume
```

The C++ app loads:

```text
effect_volume.lua
```

Naming rule:

```text
effect_<name>.lua
```

Examples:

```text
volume  -> effect_volume.lua
reverb  -> effect_reverb.lua
phaser  -> effect_phaser.lua
delay   -> effect_delay.lua
```

The C++ app does not need to know what controls each effect has.

Lua handles that.

---

## Lua Effect Contract

Each Lua effect provides two functions:

```lua
function init()
end

function processFrame(left, right)
  return left, right
end
```

### init()

`init()` runs once when the Lua file loads.

Use it to ask for setup values.

Example:

```text
Volume level (0 to 1, default 1):
```

If the user presses Enter, Lua uses the default.

### processFrame(left, right)

`processFrame(left, right)` runs for every stereo audio frame.

The C++ app sends:

```text
left
right
```

Lua returns:

```text
processedLeft
processedRight
```

That is the full DSP contract.

---

## The Volume Lua Effect

The first simple effect is:

```text
effect_volume.lua
```

It asks for one value:

```text
Volume level
```

Range:

```text
0.0 to 1.0
```

Default:

```text
1.0
```

Behavior:

```text
0.0 = silent
0.5 = half volume
1.0 = normal volume
```

The processing is intentionally simple:

```text
outputLeft  = inputLeft  * volume
outputRight = inputRight * volume
```

That makes it a perfect first test.

If volume works, then the whole path works:

```text
C++ opened USB audio
C++ loaded Lua
Lua asked for a parameter
C++ called Lua per frame
Lua returned processed audio
C++ played the result
```

Solid little ride.

---

## Creating New Lua Effects

To create a new effect, make a new file:

```text
effect_<name>.lua
```

Then start the app and enter:

```text
<name>
```

Examples:

```text
effect_tremolo.lua  -> enter tremolo
effect_reverb.lua   -> enter reverb
effect_phaser.lua   -> enter phaser
effect_delay.lua    -> enter delay
```

Each file can ask for whatever values it needs inside `init()`.

Example prompts:

```text
Rate (0.1 to 10, default 4):
Depth (0 to 1, default 0.5):
Mix (0 to 1, default 0.35):
Decay (0 to 0.95, default 0.6):
```

The C++ app stays simple.

The Lua files carry the creative part.

---

## Simple Effect Ideas

You can create effects like:

```text
Volume
Boost
Tremolo
Auto-pan
Delay
Reverb
Phaser
Distortion
Filter
Stereo widener
```

Each one follows the same pattern:

```lua
function init()
  -- ask for setup values here
end

function processFrame(left, right)
  -- process audio here
  return left, right
end
```

---

## Project Mental Model

The project is split like this:

```text
Benchmark:
Can the machine keep up?

C++ DSP Host:
Can audio move through Lua in real time?

Lua Effect:
What should the sound do?
```

That split keeps things easy to change.

C++ handles the audio plumbing.

Lua handles the effect.

No rebuild needed just to try a new sound.

---

## Practical Starting Point

Use this when testing:

```text
Sample rate: 48000
Buffer size: 256
Channels: 2
Format: float
```

If the sound is stable, try:

```text
Buffer size: 128
```

If it crackles, go back to 256.

Clean audio beats tiny buffers.

Every time.
