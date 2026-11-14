<!-- SPDX-License-Identifier: MIT -->
# Third-party components

EmberMesh's own code is released under the MIT License (see [LICENSE](LICENSE)). The repository
also contains the components below, written by other people and kept under their own licences.
Each code component is vendored **unmodified**, next to its licence file; the search model is
converted to a 4-bit format, as described in its entry. Nothing else is bundled: the shared UI
uses no third-party code, fonts or images, and the Android and iOS shells use only the platforms'
own frameworks plus ZXing core on Android.

The guide library (`guides/content/`) is **not** covered by this file or by the MIT License. Its
licensing is explained in [guides/LICENSES.md](guides/LICENSES.md).

| Component | Version | Location in the tree | Licence | Ships in |
|---|---|---|---|---|
| [Monocypher](#monocypher) | 4.0.2 | `core/third_party/monocypher/` | BSD-2-Clause **or** CC0-1.0, your choice | every app (the core's cryptography) |
| [doctest](#doctest) | 2.4.11 | `core/third_party/include/doctest.h` | MIT (includes parts under BSL-1.0) | nothing: unit tests only |
| [miniaudio](#miniaudio) | 0.11.21 | `host/third_party/miniaudio/` | Public domain (Unlicense) **or** MIT-0, your choice | desktop `emberd` (sound-card endpoint) |
| [ZXing core](#zxing-core) | 3.5.3 | `apps/android/app/libs/zxing-core-3.5.3.jar` | Apache-2.0 | Android app (QR scanning) |
| [Gradle wrapper](#gradle-wrapper) | wrapper for Gradle 9.8.2 | `apps/android/gradlew`, `gradlew.bat`, `gradle/wrapper/` | Apache-2.0 | nothing: build tool only |
| [potion-base-4M](#potion-base-4m-semantic-search-model) (search model) | upstream commit `9b3cff412d30`, converted to 4-bit weights | `models/potion-base-4M/` | MIT | every app, inside `guides.pack` |
| [QR Code generator library](#qr-code-generator-library-project-nayuki) (parts, adapted) | upstream C++ library by Project Nayuki | `core/src/tools/qr.cpp`, `ui/js/mock/qr.js` | MIT | every app (QR codes for contact cards, pairing and bundles); the UI's demo backend |

When you redistribute EmberMesh binaries, keep this file (or the licence texts it points to)
with them. The in-app **Settings → About → Licences** section shows the same list.

---

## Monocypher

- **What:** cryptographic library (X25519, EdDSA/Ed25519, XChaCha20-Poly1305, BLAKE2b, Argon2).
- **Version:** 4.0.2, unmodified. Files: `monocypher.{c,h}`, `monocypher-ed25519.{c,h}`.
- **Upstream:** <https://monocypher.org>, release tarball checksum recorded in
  [`core/third_party/monocypher/VERSION.txt`](core/third_party/monocypher/VERSION.txt).
- **Licence:** dual-licensed, BSD-2-Clause **or** CC0-1.0. EmberMesh uses it under either.
  Full text, including the complete CC0 1.0 legal code:
  [`core/third_party/monocypher/LICENCE.md`](core/third_party/monocypher/LICENCE.md); authors:
  [`AUTHORS.md`](core/third_party/monocypher/AUTHORS.md).

The BSD-2-Clause option, as it appears in `LICENCE.md`:

```text
Copyright (c) 2017-2023, Loup Vaillant
Copyright (c) 2017-2019, Michael Savage
Copyright (c) 2017-2023, Fabio Scotoni
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the
   distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## doctest

- **What:** single-header C++ unit-test framework. Used only by the test executables
  (`core/tests/`, `host/tests/`, `apps/desktop/tests/`); it is not compiled into any app.
- **Version:** 2.4.11, unmodified. Upstream: <https://github.com/doctest/doctest>.
- **Licence:** MIT. The header refers to an accompanying `LICENSE.txt` that is not vendored; its
  text is reproduced here. The header also states that some parts are taken from Catch2 and lest,
  which use the Boost Software License 1.0 (reproduced below).

```text
The MIT License (MIT)

Copyright (c) 2016-2023 Viktor Kirilov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

```text
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

## miniaudio

- **What:** single-header audio playback and capture library (David Reid). Used by the desktop
  host library's sound-card endpoint (`host/src/host/endpoints/audio_miniaudio.c`); not compiled
  for Android, iOS or the web.
- **Version:** 0.11.21 (2023-11-15), unmodified. Upstream: <https://miniaud.io>.
- **Licence:** a choice of public domain (the Unlicense) **or** MIT No Attribution (MIT-0).
  Full text: [`host/third_party/miniaudio/LICENSE`](host/third_party/miniaudio/LICENSE) (also at
  the end of `miniaudio.h`). Neither option requires attribution; it is credited here anyway.

## ZXing core

- **What:** barcode/QR decoding library, used by the Android app's QR scanner
  (`host.qr.scan`).
- **Version:** 3.5.3, the unmodified Maven Central artifact `com.google.zxing:core:3.5.3`;
  SHA-1 in [`apps/android/app/libs/zxing-core-3.5.3.jar.sha1`](apps/android/app/libs/zxing-core-3.5.3.jar.sha1).
  Upstream: <https://github.com/zxing/zxing>.
- **Licence:** Apache License 2.0. Full text:
  [`apps/android/app/libs/ZXING-LICENSE.txt`](apps/android/app/libs/ZXING-LICENSE.txt)
  (canonical copy: <https://www.apache.org/licenses/LICENSE-2.0.txt>). The artifact ships no
  NOTICE file.

## Gradle wrapper

- **What:** the scripts and bootstrap jar that download and run Gradle for the Android build
  (`apps/android/gradlew`, `apps/android/gradlew.bat`,
  `apps/android/gradle/wrapper/gradle-wrapper.jar`, `gradle-wrapper.properties`). Build tooling
  only; nothing from it ends up in the APK.
- **Version:** the wrapper generated for Gradle 9.8.2 (the distribution named in
  `gradle-wrapper.properties`), unmodified. Upstream: <https://gradle.org>, Gradle, Inc.
- **Licence:** Apache License 2.0 (`SPDX-License-Identifier: Apache-2.0` in the scripts and in
  the jar's manifest; the jar carries the full text as `META-INF/LICENSE`, identical to
  [`apps/android/app/libs/ZXING-LICENSE.txt`](apps/android/app/libs/ZXING-LICENSE.txt)).

## potion-base-4M (semantic search model)

- **What:** a Model2Vec static text-embedding model by MinishLab (one 128-dimensional vector per
  word piece; no neural network runs at search time), used for meaning-based guide search
  (docs/RPC_API.md §7). It is converted to EmberMesh's compact format with 4-bit weights by
  [`tools/embedding/convert.py`](tools/embedding/convert.py) and packed into `guides.pack`, so it
  ships with every app that carries the guide library.
- **Version:** Hugging Face repository `minishlab/potion-base-4M` at commit
  `9b3cff412d30be9ae8603fe10224c224f3401869`; the upstream files and their SHA-256 sums are pinned
  in [`tools/embedding/models.json`](tools/embedding/models.json) and
  [`models/README.md`](models/README.md).
- **In the tree:** `models/potion-base-4M/potion-base-4M-q4.emodel` (2,347,848 bytes, SHA-256
  `6c28a8ee5e2a0ad13540d140e2053c7ded08dfd4f586e33b3abd839e0ddff87d`). Converting to 4 bits is a
  modification of the model, which the MIT License permits.
- **Upstream:** <https://huggingface.co/minishlab/potion-base-4M>, Model2Vec
  <https://github.com/MinishLab/model2vec>. The model was distilled from `BAAI/bge-base-en-v1.5`,
  which is also published under the MIT licence.
- **Licence:** MIT, according to the model card's metadata at the pinned commit. The model
  repository carries no separate licence file; the MIT text and copyright line from the authors'
  Model2Vec repository are kept next to the model in
  [`models/potion-base-4M/LICENSE`](models/potion-base-4M/LICENSE), with a note explaining where
  the text comes from.


## QR Code generator library (Project Nayuki)

- **What:** `core/src/tools/qr.cpp`, the QR code encoder (and `ui/js/mock/qr.js`, the equivalent
  encoder of the UI's demo backend), is derived from (a C++17 adaptation of
  parts of) the QR Code generator library by Project Nayuki: the module-count formula, the
  Reed-Solomon divisor/remainder and GF(2^8) multiply, the block interleaving and the
  error-correction tables follow that library closely.
- **Upstream:** <https://www.nayuki.io/page/qr-code-generator-library>,
  <https://github.com/nayuki/QR-Code-generator>.
- **Licence:** MIT. The copyright and permission notice is reproduced, as the licence requires,
  at the top of both files:

  > Copyright (c) Project Nayuki. (MIT License)
  > https://www.nayuki.io/page/qr-code-generator-library
  >
  > Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  > and associated documentation files (the "Software"), to deal in the Software without
  > restriction, including without limitation the rights to use, copy, modify, merge, publish,
  > distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
  > Software is furnished to do so, subject to the following conditions:
  > - The above copyright notice and this permission notice shall be included in all copies or
  >   substantial portions of the Software.
  > - The Software is provided "as is", without warranty of any kind, express or implied,
  >   including but not limited to the warranties of merchantability, fitness for a particular
  >   purpose and noninfringement. In no event shall the authors or copyright holders be liable
  >   for any claim, damages or other liability, whether in an action of contract, tort or
  >   otherwise, arising from, out of or in connection with the Software or the use or other
  >   dealings in the Software.
---

## Interoperability notes (no third-party code)

These parts of EmberMesh implement other projects' protocols or file formats from public
specifications. No code from those projects is included.

- **Meshtastic:** `core/src/link/meshtastic_proto.h` is a hand-written encoder/decoder that uses
  the field numbers published in the Meshtastic protobuf definitions. No `.proto` files or
  generated code are included.
- **APRS / AX.25 / KISS, RNode, Mobilinkd BLE KISS, Nordic UART service:** implemented from their
  published specifications.
- **QR codes:** the encoder follows ISO/IEC 18004 and adapts parts of Project Nayuki's QR Code
  generator library (MIT, listed above); the scanner on Android is ZXing (above), on iOS the
  system's own.
