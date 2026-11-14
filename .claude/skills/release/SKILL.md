---
name: release
description: Cut an EmberMesh version (CMake version, vX.Y.Z tag), understand the rolling "latest" pre-release, how the Android APK signing key is selected and how to verify its certificate digest and the download checksums, and what iOS needs. Use when releasing, tagging, changing release signing, or checking which key signed an APK.
---

# Releases and signing

Reference: `docs/CI.md` ("Release", "Cutting a versioned release", "Release signing"),
`apps/android/README.md` ("Signing"), `.github/workflows/release.yml`, `tools/ci/android-signing.sh`.

## 1. What is published automatically

- **Every push to `main`** builds everything and replaces the rolling pre-release **`latest`**
  (tag `latest`). Only the newest `main` commit may replace it; an older run that finishes late
  leaves it alone. Version: `project(EmberMesh VERSION …)` + `-dev.<commit>`.
- Assets: `guides.pack`, the web zip and single-file HTML (with and without the library), five
  `emberd` packages, the Android debug and release APKs, `SHA256SUMS.txt`, release notes.
- Archives are reproducible (sorted entries, commit time stamps) and path-free.

## 2. Cut a versioned release

1. Set the version in the top-level `CMakeLists.txt`: `project(EmberMesh VERSION X.Y.Z …)`
   (Android `versionName`/`versionCode` and every package follow it). Update user-facing notes if
   any (manual chapters, READMEs) in the same change.
2. Run the checks locally (`./build.sh test`, `tools/check-publishable.sh` must print OK), commit,
   push to `main`, wait until every workflow is green.
3. Tag that commit and push the tag:
   ```sh
   git tag -a vX.Y.Z -m "EmberMesh X.Y.Z"
   git push origin vX.Y.Z
   ```
4. The Release workflow fails if the tag does not match the CMake version, otherwise publishes
   release `vX.Y.Z`. `vX.Y.Z-rc.1` becomes a pre-release. Re-running for the same tag replaces the
   assets. Tag messages and release notes are published: no names, hosts or paths in them.

## 3. How the APK signing key is selected

| Situation | Release APK |
|---|---|
| push to `main` / `v*` tag, all four secrets set | signed with the repository owner's key: `embermesh-<v>-android-release.apk` |
| no secrets (forks, pull requests, a new repository) | runner's throw-away debug key: `…-android-release-ci-debug-signed.apk` (cannot update other builds) |
| some but not all four secrets | the job fails |
| variable `ANDROID_CERT_SHA256` set | a release-key build with a different certificate fails; a Release run without the secrets fails |
| local `./gradlew :app:assembleRelease` | your key if `EMBER_ANDROID_KEYSTORE`, `EMBER_ANDROID_KEYSTORE_PASSWORD`, `EMBER_ANDROID_KEY_ALIAS`, `EMBER_ANDROID_KEY_PASSWORD` are set (environment or `~/.gradle/gradle.properties`, never the repo); otherwise your machine's own debug key |

Secrets: `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`,
`ANDROID_KEY_PASSWORD` (Settings -> Secrets and variables -> Actions). They are read only in steps
guarded to `main` / `v*` tags, decoded to `$RUNNER_TEMP/android-signing/` (never the workspace),
never echoed, deleted afterwards. Creating them: playbook fork-setup §4.

## 4. Verify

```sh
keytool -list -v -keystore /path/to/release.p12 -storetype PKCS12 | grep 'SHA256:'   # your key
$ANDROID_HOME/build-tools/<version>/apksigner verify --print-certs <apk>             # an APK
sha256sum -c SHA256SUMS.txt --ignore-missing      # downloads (macOS: shasum -a 256 -c SHA256SUMS.txt)
```

The Android job summary and the release notes print the certificate SHA-256 of the APK; it must
equal `ANDROID_CERT_SHA256` (colons optional).

## 5. Keys: rules that protect you and your users

- One key per publisher. Never reuse, share, request or commit anyone's keystore, and never
  commit your own: `.gitignore` and the gate refuse `*.jks`, `*.p12`, `*.keystore`,
  `keystore.properties`.
- Back the keystore and its passwords up outside the repository. A lost key means installed copies
  can never be updated (users must uninstall, losing their data); a leaked key lets others ship
  updates as you. Rotate by publishing under a new key and telling users to uninstall first.
- The default debug key (`~/.android/debug.keystore`) is per machine: two machines produce APKs
  that do not update each other.

## 6. iOS

No `.ipa` is built or published: distribution needs the **builder's own** Apple team. CI builds
for the simulator and an unsigned device build only. Locally: `DEVELOPMENT_TEAM=<team id>
-allowProvisioningUpdates` on the `xcodebuild` command line; never commit a team id (the committed
project has `DEVELOPMENT_TEAM = ""`, and the gate refuses your local signing team ids).

## 7. Common failures

- "tag does not match project(EmberMesh VERSION …)": fix `CMakeLists.txt` or re-tag.
- Signing job fails with "some but not all": set all four secrets or none.
- Certificate mismatch: the secrets hold a different key than `ANDROID_CERT_SHA256`.
- `latest` not updated: a newer `main` commit exists; its run will update it.
