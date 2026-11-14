Encrypted node store written by the pre-v2 core (store format v1, StorageParams v1).
Passphrase: "old pass". Profile name "Old Store", callsign KD2OLD, kv notes/water = "V1-SECRET-20L",
kv notes/after = "post-encryption", a ch:public message "written by the v1 store".
Used by tests/node/node_security_test.cpp (copied to a temp dir before opening; the node migrates it).

The callsign KD2OLD inside this fixture is an arbitrary test value, not assigned by the project to
anyone. The identity key, passphrase and data in this fixture are public, throwaway test material:
never reuse them for anything real.
