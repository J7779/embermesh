# EmberMesh soft modems — measured performance

Measured with the skipped doctest sweeps in `core/tests/modem/sweep_test.cpp`
(Release build on a desktop CPU):

```sh
./build/modem-rel/core/ember_tests -ts=modem -tc="sweep afsk"     --no-skip
./build/modem-rel/core/ember_tests -ts=modem -tc="sweep robust"   --no-skip
./build/modem-rel/core/ember_tests -ts=modem -tc="sweep fast psk" --no-skip
./build/modem-rel/core/ember_tests -ts=modem -tc="sweep cpu"      --no-skip
NOISE_MIN=10 ./build/modem-rel/core/ember_tests -ts=modem -tc="sweep noise" --no-skip
```

**SNR definition** (same everywhere, see `dsp/channel_sim.h`): signal power / noise power in a
**3000 Hz** reference bandwidth, white Gaussian noise. Every frame is a separate transmission with
its own default preamble (txdelay 300 ms). The channel also applies a 300–3000 Hz radio band-pass
unless noted. "tilt" = first-order FM de-emphasis (−) or pre-emphasis (+) with a 300 Hz corner
(~5 dB twist between 1200 and 2200 Hz). "off/ppm" = audio frequency offset (as from SSB mistuning)
and sample-clock drift.

## Frame success rate vs SNR

| modem | frame | condition | 10% | 50% | ≥95% |
|---|---|---|---|---|---|
| afsk1200 (7 slicers) | 50 B | flat | 4.0 dB | ~4.6 dB | 6 dB |
| afsk1200 (7 slicers) | 50 B | de-emphasis | ~4.5 dB | ~5.6 dB | 8 dB |
| afsk1200 (7 slicers) | 50 B | pre-emphasis | ~4.5 dB | ~5.5 dB | 8 dB |
| afsk1200 (1 slicer, reference) | 50 B | flat | 4 dB | ~5 dB | 8 dB |
| afsk1200 (1 slicer, reference) | 50 B | de-emphasis | ~8 dB | ~9.3 dB | 12 dB |
| afsk300 | 50 B | flat | −2 dB | ~−1.2 dB | 1 dB |
| psk2400 | 50 B | flat, no band-pass | 6 dB | ~7 dB | 9 dB |
| psk4800 | 50 B | flat, no band-pass | 12 dB | ~13.5 dB | 16 dB |
| robust250 | 32 B | flat | −7.5 dB | ~−7 dB | −6 dB |
| robust250 | 32 B | de-emph + 37 Hz + 200 ppm | −4.5 dB | ~−3.5 dB | — |
| robust125 | 32 B | flat | −10.5 dB | ~−9.7 dB | −9 dB |
| robust125 | 32 B | de-emph + 37 Hz + 200 ppm | −11 dB | ~−9 dB | −7 dB |
| robust62 | 32 B | flat | −13.5 dB | ~−12.7 dB | −11 dB |
| robust62 | 32 B | de-emph + 37 Hz + 200 ppm | −13 dB | ~−12.5 dB | −10 dB |
| robust31 | 32 B | flat | −16 dB | ~−15.5 dB | −14 dB |
| robust31 | 32 B | de-emph + 37 Hz + 200 ppm | −16 dB | ~−15.5 dB | −13 dB |

Notes:
* The multi-slicer AFSK demodulator gains ~3.7 dB over a classic single decoder on de-emphasised
  audio and costs nothing on flat audio. A 1.2-bit boxcar correlator measured better than
  Hann/Hamming-windowed correlators of 1–2.5 bits (de-emphasis: 50% point 5.6 dB vs 7–13 dB).
* robust: the 50% points sit within ~0.5 dB of the K=7 r=½ / 16-FSK noncoherent theory
  (Eb/N0 ≈ 4 dB), lowering the sync threshold does not help — decoding, not sync, is the limit.
  robust250 spans 625–2500 Hz, so de-emphasis leaves its upper tones ~12 dB weaker than the lower
  ones; the narrower speeds are hardly affected.
* psk2400/psk4800 are for flat cable paths; they are not meant for FM pre/de-emphasised audio.

## False frames from pure noise

10 minutes of white Gaussian noise per modem at 16 kHz, cycling −26 / −10 / −1 dBFS each second
(`sweep noise`): **0 false frames** for afsk1200, afsk300, psk2400, psk4800 and all four robust
speeds (also 0 header/CRC rejections reported as bad by the robust decoders). The default suite
also feeds every modem Gaussian noise, full-scale uniform junk with NaN/±Inf bursts, impulses, DC
and random PCM16 under ASan/UBSan and requires zero frames.

## CPU cost (× realtime, one core of a desktop CPU, device rate shown)

| modem | RX @ 8 kHz | RX @ 48 kHz | TX @ 8 kHz | TX @ 48 kHz |
|---|---|---|---|---|
| afsk1200 | 1880× | 980× | 7070× | 2020× |
| afsk300 | 1200× | 830× | 8550× | 2000× |
| psk2400 | 2790× | 1070× | 2130× | 1470× |
| psk4800 | 3020× | 960× | 3830× | 1540× |
| robust31 | 1390× | 670× | 87800× | 1690× |
| robust62 | 1460× | 700× | 79400× | 1770× |
| robust125 | 1550× | 740× | 79100× | 2070× |
| robust250 | 1490× | 700× | 76100× | 1990× |
| dtmf | 12700× | 1240× | 73400× | 1840× |
| cw | 5260× | 1110× | 25600× | 1870× |
| psk31 | 9320× | 1220× | 55900× | 1900× |
| rtty | 8900× | 1240× | 51000× | 2020× |

Old phones (Cortex-A7 class) and WASM are roughly 20–50× slower than the reference desktop, which still
leaves every modem well above realtime. RX cost is dominated by resampling at 48 kHz device rates;
run sound cards at 8–16 kHz where possible.
