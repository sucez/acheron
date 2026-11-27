# Valider Acheron Custom

## Suite CTest

```sh
cd /workspace/acheron
. /workspace/.acheron-qt5/env.sh
cmake --build "$dev/build" --parallel 4
xvfb-run -a ctest --test-dir "$dev/build" --no-tests=error --output-on-failure --timeout 120 -j2
```

| Révision | Périmètre Linux Qt 5.15 | Résultat |
| --- | --- | --- |
| master du fork, 08dd089 | Référence de l’environnement | 21/21 suites réussies |
| Prototype, a56e09f | Même configuration | 21/21 suites réussies |
| Mission #001 | Mêmes suites, plus ProfileTests | 22/22 suites réussies |

ProfileTests exerce la syntaxe, les bornes, la casse, les noms invalides et le format des clés de comptes selon la plateforme de compilation. L’exécution du backend de trousseau requiert une validation distincte.

## Scénario multiprocessus sous Linux

Le scénario X11 indépendant de CTest nécessite Xvfb et le module Python Xlib (`python3-xlib` sur Debian). Utiliser l’interpréteur qui fournit ce module et un affichage privé. Le script installe ses trois profils sous une racine temporaire. Utiliser le scénario du dépôt pour le client personnalisé, car la sonde de l’environnement initial cherchait une fenêtre nommée Acheron.

```sh
. /workspace/.acheron-qt5/env.sh
xvfb-run -a /usr/bin/python3 tests/profile_smoke.py "$dev/build/acheron"
```

Sur le cloud sans installation système, préparer le module dans le préfixe utilisateur et l’indiquer à Python avant la commande précédente.

```sh
mkdir -p "$dev/profile-test-deps"
(cd "$dev/profile-test-deps" && APT_CONFIG="$dev/apt/apt.conf" /usr/bin/apt-get download python3-xlib && dpkg-deb -x python3-xlib_*.deb .)
export PYTHONPATH="$dev/profile-test-deps/usr/lib/python3/dist-packages${PYTHONPATH:+:$PYTHONPATH}"
```

Le scénario confirme les fenêtres de deux profils, leurs fichiers de verrou et de journal, le refus du doublon et de cinq invocations invalides, le démarrage depuis le menu et la libération des verrous. Il redimensionne les fenêtres, les ferme normalement et vérifie leur géométrie restaurée. Les bases doivent conserver des listes différentes de comptes synthétiques, dont un même identifiant dans les deux profils, puis réussir `PRAGMA quick_check`. La reconnexion reste désactivée. Les caches SQLite de contenu et la rotation des journaux ont été examinés dans le code. Ce scénario observe les comptes au niveau SQLite et le chargement de l’application sans authentification réseau.

## Windows et fonctions connectées

La page du run historique affiche huit tâches dans la matrice et une exécution réussie à `7e93796`. Le propriétaire rapporte avoir ouvert simultanément deux fenêtres avec des comptes distincts sur Windows. Ce résultat précède les correctifs #001. Le workflow actuel s’exécute sur les pushs vers master ou ci/** et les PR visant master. [Run de référence](https://github.com/sucez/acheron/actions/runs/45939597779)

Pour qualifier la correction, utiliser un compte Windows jetable et des valeurs factices. Avec le backend Credential Store, vérifier les opérations d’écriture, de lecture et de suppression pour un même identifiant dans deux profils. Vérifier également qu’une ancienne entrée est préservée, qu’une fermeture dans la zone de notification conserve le verrou et que Quitter le libère. Effectuer les vérifications interactives complémentaires selon [CUSTOM-PROFILES](../CUSTOM-PROFILES.md).

Les tests de cette mission couvrent Linux, Qt 5 et le client hors connexion. Les appels voix/vidéo, la capture, les notifications natives, la connexion Discord, le trousseau réel et Windows demandent leurs propres environnements.
