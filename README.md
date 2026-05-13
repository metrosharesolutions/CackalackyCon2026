# CackalackyCon2026
Hack Your Guitar Tone II

What's up!  There are two parts to this solution.  One is a benchmark app written in C++, Lua, and Python.  The other is a simple C++ app that connects to a USB audio device and loads effects via Lua files.

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

g++ main.cpp -o pedal_test -lasound -llua5.4

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

## DSP Techniques

| # | DSP Technique | What It Does | Why Use It / Sound |
|---:|---|---|---|
| 1 | Gain | Changes volume with one multiply. | Basic boost/cut; clean louder or quieter signal. |
| 2 | Pan | Sends signal more left or right. | Places guitar in the stereo field. |
| 3 | Mute | Sets samples to silence. | Kill-switch style cuts or bypass logic. |
| 4 | Polarity Invert | Flips waveform positive/negative. | Phase tricks, cancellation tests, stereo weirdness. |
| 5 | Dry/Wet Mix | Blends clean and effected signal. | Lets an effect feel subtle instead of fully cooked. |
| 6 | Mono Sum | Combines left and right channels. | Tighter center sound; useful for compatibility. |
| 7 | Stereo Split | Sends left/right into separate paths. | Lets each side get different processing. |
| 8 | Fixed Attenuator | Reduces level by a fixed amount. | Prevents clipping before heavier effects. |
| 9 | Bypass Switch | Chooses clean or processed audio. | Pedal on/off behavior. |
| 10 | Output Trim | Final volume correction. | Keeps effects matched in loudness. |
| 11 | Hard Clipping | Cuts peaks sharply. | Harsh distortion; aggressive square-ish edge. |
| 12 | Soft Clipping | Rounds peaks smoothly. | Warm overdrive; less harsh than hard clipping. |
| 13 | Tremolo | Modulates volume with an LFO. | Pulsing amp-style movement. |
| 14 | Fade Ramp | Smoothly changes gain over time. | Prevents clicks when switching sounds. |
| 15 | Sample Clamp | Keeps samples inside a limit. | Safety limiter before output. |
| 16 | Half-Wave Rectify | Keeps one half of the waveform. | Gritty, broken, octave-ish texture. |
| 17 | Full-Wave Rectify | Folds negative waveform upward. | Fuzzy octave flavor. |
| 18 | DC Offset Remove | Removes signal bias. | Cleaner headroom; avoids weird speaker drift. |
| 19 | Noise Floor Cut | Zeros very tiny samples. | Cleaner silence between notes. |
| 20 | Simple Level Meter | Tracks peak level. | Shows signal strength without heavy analysis. |
| 21 | Bit Reduction | Lowers bit depth. | Crunchy lo-fi digital dirt. |
| 22 | Sample Rate Reduction | Holds samples longer. | Aliased, broken arcade texture. |
| 23 | Simple Delay | Plays audio back later. | Slapback echo or short repeat. |
| 24 | Feedback Delay | Feeds delay back into itself. | Repeating echo trail. |
| 25 | Ping-Pong Delay | Alternates delay between left/right. | Wide bouncing repeats. |
| 26 | Zero Crossing Detect | Finds where waveform crosses zero. | Basic pitch/loop sync helper. |
| 27 | Peak Hold | Holds recent loudest value. | Metering with slower visual falloff. |
| 28 | Wavefolder Lite | Folds peaks back inward. | Synthetic fuzz edge. |
| 29 | Basic Saturation | Adds mild nonlinear color. | Warmer signal without full distortion. |
| 30 | Delay Mix | Blends echo with dry signal. | Adds depth without drowning the note. |
| 31 | Basic Looper | Records and replays audio. | Repeating phrase; cheap because it is mostly buffer playback. |
| 32 | Low-Pass Filter | Reduces high frequencies. | Darker tone; removes fizz. |
| 33 | High-Pass Filter | Reduces low frequencies. | Thinner, cleaner guitar; removes mud. |
| 34 | Band-Pass Filter | Keeps a middle frequency range. | Radio tone, cocked-wah focus. |
| 35 | Notch Filter | Removes a narrow frequency range. | Cuts hum, whistle, or harsh resonance. |
| 36 | One-Pole Filter | Simple smoothing filter. | Gentle tone shaping; cheap filter base. |
| 37 | Tone Knob | Rolls off treble like a guitar tone control. | Warmer/darker pedal feel. |
| 38 | Envelope Follower | Tracks how loud the guitar is. | Lets effects react to picking strength. |
| 39 | Mid/Side Encode | Converts stereo to center/side info. | Lets you process width separately. |
| 40 | Mid/Side Decode | Converts mid/side back to stereo. | Finishes stereo-width processing. |
| 41 | Noise Gate | Closes when signal gets quiet. | Cuts hum between riffs. |
| 42 | Compressor | Reduces loud peaks. | Tighter, smoother, more even guitar. |
| 43 | Limiter | Stops signal from crossing a ceiling. | Prevents overload while staying loud. |
| 44 | Expander | Pushes quiet parts quieter. | Cleaner dynamics without hard gating. |
| 45 | Auto-Swell | Fades each note in. | Violin-like attack; removes pick transient. |
| 46 | RMS Metering | Measures average loudness. | Better loudness reading than peak only. |
| 47 | Asymmetric Clipping | Clips each side differently. | Tube-ish uneven distortion character. |
| 48 | Fuzz | Heavy nonlinear clipping. | Aggressive sustain, thick broken edge. |
| 49 | Cabinet Tone Filter | Shapes highs/lows like a speaker. | Makes distortion less fizzy and more amp-like. |
| 50 | Amplitude Modulation | Fast volume modulation. | Choppy robotic ring-like movement. |
| 51 | Biquad Filter | General EQ/filter building block. | Precise tone shaping with low CPU cost. |
| 52 | Resonant Filter | Boosts around cutoff. | Synthy sweep or wah-like bite. |
| 53 | Auto-Wah | Envelope moves a filter. | Funky vowel sound tied to picking. |
| 54 | Pedal Wah | Control moves a resonant filter. | Classic wah pedal sweep. |
| 55 | Phaser | Sweeps phase notches. | Smooth swirling motion. |
| 56 | Flanger | Uses very short moving delay. | Jet-plane sweep and metallic combing. |
| 57 | Chorus | Adds slightly delayed moving copies. | Thick doubled guitar. |
| 58 | Vibrato | Modulates pitch slightly. | Wobbly tape/amp movement. |
| 59 | Ring Modulation | Multiplies signal by oscillator. | Metallic, bell-like, alien tones. |
| 60 | Comb Filter | Delay creates repeated notches. | Hollow metallic resonance. |
| 61 | Graphic EQ | Multiple fixed EQ bands. | Quick tone sculpting across frequencies. |
| 62 | Parametric EQ | Adjustable frequency/gain/Q. | Surgical tone control. |
| 63 | Rotary Speaker | Simulates spinning speaker movement. | Organ-style swirl and motion. |
| 64 | Univibe | Uneven phasey modulation. | Psychedelic wobble. |
| 65 | Multi-Tap Delay | Several delay taps. | Rhythmic echo patterns. |
| 66 | Ducking Delay | Lowers delay while playing. | Clear notes with big repeats after. |
| 67 | Room Reverb | Simulates a small acoustic space. | Natural room around the guitar. |
| 68 | Crossfeed | Blends stereo channels slightly. | More speaker-like stereo image. |
| 69 | Stereo Widener | Boosts side information. | Bigger, wider guitar sound. |
| 70 | Basic Amp Sim | Gain stages plus tone filtering. | More amp-like drive than simple clipping. |
| 71 | Spring Reverb | Simulates spring-tank reflections. | Classic surf/amp drip. |
| 72 | Plate Reverb | Dense smooth artificial reverb. | Studio-style width and polish. |
| 73 | Hall Reverb | Larger reverb space. | Big ambient depth. |
| 74 | Gated Reverb | Reverb cuts off abruptly. | Punchy 80s-style tail. |
| 75 | Freeze Reverb | Holds the reverb tail. | Infinite pad under the guitar. |
| 76 | Tape Delay | Delay with filtering and wobble. | Warm unstable repeats. |
| 77 | Granular Delay | Cuts delay into tiny grains. | Glitchy texture and clouds. |
| 78 | Octave Down | Adds lower octave. | Thick bassy guitar layer. |
| 79 | Octave Up | Adds upper octave. | Screaming lead/fuzz tone. |
| 80 | Pitch Tracking | Detects the played note. | Needed for tuners, synths, smart pitch effects. |
| 81 | Pitch Shifting | Changes pitch without just speeding audio up. | Octaves, detune, whammy-style movement. |
| 82 | Harmonizer | Adds musical intervals. | Dual-guitar harmony sound. |
| 83 | Whammy Shift | Smooth controlled pitch bend. | Dive bombs, octave sweeps, pitch pedal effects. |
| 84 | Time Stretching | Changes loop length without changing pitch. | Keeps loops in tempo. |
| 85 | Formant Shift | Changes vocal/body tone shape. | Strange voice-like guitar morph. |
| 86 | Shimmer Reverb | Reverb with pitch-shifted tails. | Huge angelic ambient wash. |
| 87 | Shimmer Delay | Delay with pitch-up repeats. | Sparkly rising echo trails. |
| 88 | Multi-Band Compressor | Compresses frequency bands separately. | Polished, controlled guitar tone. |
| 89 | Dynamic EQ | EQ changes with signal level. | Fixes harshness only when it appears. |
| 90 | Acoustic Body Sim | Adds guitar-body resonance. | Makes electric guitar feel more acoustic. |
| 91 | Convolution | Applies an impulse response. | Realistic room, speaker, or cab character. |
| 92 | Cab IR Loader | Uses speaker-cab impulse responses. | Realistic amp/cab tone direct into PA. |
| 93 | Phase Vocoder | Frequency-domain pitch/time processing. | Cleaner advanced pitch and stretch effects. |
| 94 | FFT Spectrum Analysis | Breaks signal into frequency bins. | Smart effects, visualizers, spectral tools. |
| 95 | Spectral Gate | Gates individual frequency bins. | Surgical noise removal or weird chopped tone. |
| 96 | Spectral Freeze | Holds frequency content in place. | Frozen glassy pad from a guitar note. |
| 97 | Adaptive Noise Reduction | Learns and removes background noise. | Cleaner signal without simple gating. |
| 98 | Advanced Amp Model | Simulates amp circuit behavior. | More realistic drive, sag, tone interaction. |
| 99 | Neural Amp Model | Uses trained model behavior. | Modern amp-capture sound. |
| 100 | Real-Time Convolution Reverb | Long impulse response in realtime. | Real spaces or huge cab/room realism with heavy CPU use. |

