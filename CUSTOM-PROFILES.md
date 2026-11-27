# Acheron Custom — profiles (experimental)

Launch `acheron.exe` for the default `principal` profile, or `acheron.exe --profile secondary` for a separate profile. In View, select Open another profile and enter a name. Names are case-insensitive, 1–32 ASCII letters/digits/hyphens/underscores, starting with a letter or digit.

Each named profile uses a distinct application settings/data namespace and keychain service. The original Acheron account database is not migrated. Sign in separately and enable Connect on startup if desired. Use different Discord accounts in different profiles. Never commit local data or credentials.

## Manual Windows smoke test

1. Start the custom executable alongside the original installation; confirm the custom profile initially has no accounts.
2. Open `secondary` from the View menu. Confirm a separate window and profile title.
3. Sign in with distinct accounts, enable Connect on startup, and confirm each window shows its own messages and account list.
4. Change a theme setting in one profile; confirm the other does not change. Close both and reopen; confirm settings and sessions persist.
5. Try opening `secondary` a second time (also `SECONDARY`); confirm a warning and no second window for that profile.
6. Remove an account in a test profile and verify the other profile and original installation are unaffected.

This is an experimental change. Compilation alone does not validate session isolation or voice/notification behavior.

CI branch: Windows validation build of the experimental profile implementation.
