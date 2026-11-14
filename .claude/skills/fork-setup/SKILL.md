---
name: fork-setup
description: First steps after forking, mirroring or rehosting EmberMesh on GitHub - enabling Actions and Pages, creating your own Android signing key and repository secrets, what works with no secrets, renaming, and keeping your own identifiers out of your fork. Use when someone forks or rehosts the project or asks how to publish their own builds.
---

# Setting up a fork or a rehosted copy

A fork is a separate publisher. Nothing secret is inherited: GitHub does not copy secrets or
variables into forks, and you must never ask for or reuse anyone else's signing key.

## 1. Before your first push

1. `git config user.name` / `git config user.email` are what your commits publish; use your
   GitHub no-reply address (GitHub -> Settings -> Emails).
2. `./build.sh doctor`, then build and test what you can (playbook build-and-test).
3. `tools/check-publishable.sh` must print OK on **your** machine (playbook privacy-check). Add
   identifiers it cannot discover: `EMBER_PUBLISH_EXTRA_IDS="yourcall yourstreet" tools/check-publishable.sh`.

## 2. Turn on CI and Pages

1. **Actions** tab: enable workflows for the fork (GitHub keeps them off in new forks until you do).
2. **Settings -> Pages -> Build and deployment -> Source: GitHub Actions.** The workflow cannot set
   this; until it is set, the Pages run fails at "Configure Pages" and its summary says so.
3. Push to `main` (or run Actions -> Web -> Run workflow). Pages deploys after a green Web run
   (`pages.yml`); then set the repository's About -> Website to the Pages URL if you like.

Everything is repository-relative, so nothing needs editing for your account or a new repository
name: README badges and links use `../../actions/…` and `../../releases/…`, and every URL in the
web app is relative (checked by `tools/ci/check-relative-urls.py`), so it works under
`https://<owner>.github.io/<repository>/`. If another renderer shows the badges broken, see the
comment above them in `README.md`.

## 3. What works without any secret

All nine workflows: builds, tests, the publishing gate, Pages and the rolling `latest`
pre-release in your fork. The release APK is then signed with a throw-away CI key and named
`…-android-release-ci-debug-signed.apk`; it cannot be installed over a later build as an update
(uninstall first). iOS is never signed on CI (no `.ipa`).

## 4. Your own Android release key (optional)

Only if you publish APKs people will update. Full detail: `docs/CI.md` "Release signing" and
`apps/android/README.md` "Signing".

```sh
keytool -genkeypair -storetype PKCS12 -keystore /path/outside/repo/release.p12 -alias embermesh \
  -keyalg RSA -keysize 4096 -validity 10000 -dname "CN=EmberMesh"
base64 < /path/outside/repo/release.p12 | tr -d '\n' | gh secret set ANDROID_KEYSTORE_BASE64
gh secret set ANDROID_KEYSTORE_PASSWORD      # prompts
gh secret set ANDROID_KEY_ALIAS --body embermesh
gh secret set ANDROID_KEY_PASSWORD           # prompts
keytool -list -v -keystore /path/outside/repo/release.p12 -storetype PKCS12 | grep 'SHA256:'
gh variable set ANDROID_CERT_SHA256 --body '<SHA256 from keytool>'
```

- Back the keystore and its passwords up **outside the repository** (two places). Losing it means
  your users can no longer update; leaking it lets anyone ship "updates" as you.
- The `-dname` is public inside every APK: keep it neutral (no real name, e-mail or place).
- All four secrets or none: some-but-not-all fails the job. With `ANDROID_CERT_SHA256` set, a
  Release run without the secrets fails instead of shipping a throw-away-key APK.
- Secrets are read only on pushes to `main` (and `v*` tags in Release), never for pull requests.

## 5. iOS

Sign with **your own** Apple team: `DEVELOPMENT_TEAM=<your team id> -allowProvisioningUpdates`
on the `xcodebuild` command line, or pick the team in Xcode and then **revert** the change Xcode
writes into `apps/ios/EmberMesh.xcodeproj/project.pbxproj` (the committed value is
`DEVELOPMENT_TEAM = ""`). If the bundle id `org.embermesh.app` is taken for your team, change it
locally the same way. See `apps/ios/README.md` "Installing on a device without the App Store".

## 6. Renaming or rebranding

- Keep `LICENSE` and the existing copyright notice (MIT requires it) and the guide attributions
  (CC BY / CC BY-SA require them; `guides/LICENSES.md`).
- Changing the app id (`org.embermesh.app`), the URI scheme `ember:`, the hub port 8437, the
  multicast group or the BLE service UUIDs makes your build unable to talk to EmberMesh nodes
  (docs/ARCHITECTURE.md §6). Change them only on purpose and document it.
- Adding your own name anywhere is a publication of your identity; the gate allows the git display
  name only in copyright lines of the files listed as `identity path` in `tools/publishable-allow.txt`.

## 7. Keep your fork from publishing you

- Never commit `local.properties`, keystores, `.env*`, tokens, logs, dumps or node data.
- Screenshots and fixtures from the demo backend only (`ui/?mock=1`).
- Run `tools/check-publishable.sh` before every push; CI's `--ci` run cannot see your identifiers.
