#!/usr/bin/env bash
# Optional release-key signing of the Android APK on CI. No key material is ever in the repository.
#
#   tools/ci/android-signing.sh prepare
#       Reads the repository secrets passed in the environment (ANDROID_KEYSTORE_BASE64,
#       ANDROID_KEYSTORE_PASSWORD, ANDROID_KEY_ALIAS, ANDROID_KEY_PASSWORD). All four set: decodes
#       the keystore into $RUNNER_TEMP/android-signing/ (never the workspace) and writes
#       present=true and keystore=<path> to $GITHUB_OUTPUT. None set: present=false (the release
#       APK is then signed with the runner's throw-away debug key). Some set: fails.
#   tools/ci/android-signing.sh verify <apk> [expected-sha256]
#       Prints the signing certificate (apksigner verify --print-certs) as Markdown, writes
#       cert_sha256=<hex> to $GITHUB_OUTPUT, and fails when expected-sha256 is given and differs.
#   tools/ci/android-signing.sh cleanup
#       Deletes the decoded keystore.
# Nothing secret is printed: only whether a key is present and the certificate (public).
set -euo pipefail

DIR="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/android-signing"
out() { [[ -n "${GITHUB_OUTPUT:-}" ]] && printf '%s\n' "$1" >> "$GITHUB_OUTPUT"; return 0; }

case "${1:-}" in
  prepare)
    n=0
    for v in ANDROID_KEYSTORE_BASE64 ANDROID_KEYSTORE_PASSWORD ANDROID_KEY_ALIAS ANDROID_KEY_PASSWORD; do
      [[ -n "${!v:-}" ]] && n=$((n + 1))
    done
    if [[ $n -eq 0 ]]; then
      echo "release signing: no release key secrets in this run; the release APK is signed with the runner's throw-away debug key"
      out present=false; exit 0
    fi
    if [[ $n -ne 4 ]]; then
      echo "::error::release signing: only $n of the 4 secrets ANDROID_KEYSTORE_BASE64, ANDROID_KEYSTORE_PASSWORD, ANDROID_KEY_ALIAS, ANDROID_KEY_PASSWORD are set"
      exit 1
    fi
    rm -rf "$DIR"; (umask 077 && mkdir -p "$DIR")
    ks="$DIR/release.keystore"
    (umask 077 && printf '%s' "$ANDROID_KEYSTORE_BASE64" | tr -d ' \r\n\t' | base64 -d > "$ks") \
      || { echo "::error::release signing: ANDROID_KEYSTORE_BASE64 is not valid base64"; exit 1; }
    [[ -s "$ks" ]] || { echo "::error::release signing: ANDROID_KEYSTORE_BASE64 decodes to an empty file"; exit 1; }
    echo "release signing: release key present (keystore decoded into the runner's temp directory)"
    out present=true; out "keystore=$ks"
    ;;
  verify)
    apk="${2:?verify needs an APK}"; want="$(printf '%s' "${3:-}" | tr -d ': ' | tr 'A-F' 'a-f')"
    sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
    apksigner="$(ls -d "$sdk"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)"
    [[ -x "$apksigner" ]] || { echo "::error::apksigner not found under $sdk/build-tools"; exit 1; }
    certs="$("$apksigner" verify --verbose --print-certs "$apk")" || { echo "::error::apksigner verify failed for $apk"; exit 1; }
    # "Signer #1 certificate ..." (older apksigner) or "V2 Signer: certificate ..." (current)
    sha="$(printf '%s\n' "$certs" | sed -n -E 's/^(Signer #1|V[0-9.]+ Signer:?) certificate SHA-256 digest: *//p' | head -1 | tr 'A-F' 'a-f')"
    dn="$(printf '%s\n' "$certs" | sed -n -E 's/^(Signer #1|V[0-9.]+ Signer:?) certificate DN: *//p' | head -1)"
    [[ -n "$sha" ]] || { echo "::error::no signing certificate found in apksigner output for $apk"; exit 1; }
    schemes="$(printf '%s\n' "$certs" | sed -n 's/^Verified using \(v[0-9.]*\) scheme.*: true$/\1/p' | tr '\n' ' ' | sed 's/ $//')"
    out "cert_sha256=$sha"
    echo "- \`$(basename "$apk")\`: certificate \`$dn\`, SHA-256 \`$sha\`, signature schemes ${schemes:-?}"
    if [[ -n "$want" && "$sha" != "$want" ]]; then
      echo "::error::$(basename "$apk") is signed with certificate $sha, but ANDROID_CERT_SHA256 expects $want"
      echo "  **certificate does not match ANDROID_CERT_SHA256 (\`$want\`)**"
      exit 1
    fi
    [[ -n "$want" ]] && echo "  matches ANDROID_CERT_SHA256"
    exit 0
    ;;
  cleanup)
    rm -rf "$DIR"; echo "release signing: temporary keystore removed"
    ;;
  *)
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
