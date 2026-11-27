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

## Compatibilité après l’audit #001

Les mêmes options s’appliquent à `acheron` sous Linux. Le menu et les formes `--profile NOM` et `--profile=NOM` acceptent de 1 à 32 caractères ASCII. Les majuscules sont ramenées en minuscules après validation.

**Mise à niveau Windows depuis le prototype.** La nouvelle version demande de renseigner à nouveau le jeton des comptes déjà configurés. L’ancienne clé pouvait appartenir à plusieurs profils ou à l’installation officielle. La nouvelle version laisse cette clé en place. Les fiches SQLite et les réglages existants sont conservés.

Le verrou reste actif quand le processus est réduit dans la zone de notification. Pour libérer le profil, quitter le processus depuis son menu. Les profils d’un même utilisateur partagent les permissions du système. Voir [l’inventaire du stockage](docs/ARCHITECTURE.md) et [les tests automatisés hors connexion](docs/TESTING.md).
