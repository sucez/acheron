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
| Acheron Hub #002 | Mêmes suites, plus HubTests | 23/23 suites réussies |

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

La direction technique a confirmé les huit compilations de la base `8d70603`. Sous Windows, MinSizeRel, RelWithDebInfo et Qt5 ont chacun réussi 42/42 tests et la configuration minimale 23/23. WindowsTokenStoreTests a réussi dans les quatre configurations. Ces résultats qualifient la base #001/#001-B. Le Hub demande une nouvelle exécution de la matrice.

### Test ciblé du trousseau Windows (#001-B)

Sur un runner Windows avec QtKeychain et les dépendances de compilation décrites dans BUILD, exécuter depuis un répertoire source configuré pour MinSizeRel avec les tests activés.

```powershell
cmake --build build --config MinSizeRel --target TestWindowsTokenStore
ctest --test-dir build -C MinSizeRel -R '^WindowsTokenStoreTests$' --no-tests=error --output-on-failure
```

Qt Test utilise le vrai TokenStore, deux profils temporaires nommés avec un suffixe aléatoire, le même identifiant synthétique et des valeurs fictives. Il vérifie la lecture, le remplacement et la suppression par profil. Le nettoyage tente les deux suppressions même après un échec d’assertion. CTest limite le test à 30 secondes.


## Hub #002 sous Linux

`HubTests` vérifie la découverte, les noms uniques et l’alias stable. Deux processus écrivent simultanément des préférences distinctes. Le test couvre également un registre endommagé, l’interruption d’une écriture atomique de fiche et les messages IPC valides, mal formés ou trop longs.

Sur un affichage Xvfb privé à échelle 100 %, le scénario suivant démarre le vrai binaire avec HOME, XDG et socket de session temporaires. L’identifiant de compte fictif 0 est rejeté par la session avant l’accès au trousseau ou au réseau. Pour obtenir les captures, fournir un répertoire de destination en second argument et disposer de la commande ImageMagick `import`.

```sh
. /workspace/.acheron-qt5/env.sh
export PYTHONPATH="$dev/profile-test-deps/usr/lib/python3/dist-packages"
xvfb-run -a /usr/bin/python3 tests/hub_smoke.py "$dev/build/acheron" /tmp/acheron-hub-captures
```

La vérification observe le Hub vide, un puis trois profils historiques, 19 profils avec défilement, un pseudo Unicode long, deux fenêtres et leurs verrous, un deuxième lancement du Hub, la fermeture et la redécouverte des sessions, la restauration après fermeture globale et la récupération après arrêt brutal. Le scénario exerce également le lancement groupé, son exclusion par carte, les démarrages sélectionné et par défaut, les archives et la confirmation de fermeture de deux sessions. Une copie jetable de l’exécutable sert à simuler un enfant qui se termine avant de prendre son verrou et à réessayer ensuite. Le script imprime le délai jusqu’à la première fenêtre X11 et les mesures Linux de RSS et de temps CPU sur deux secondes. La RSS additionnée peut compter plusieurs fois des pages partagées.

## Qualification Windows de la branche Hub

1. Compiler les configurations Windows Qt5/Qt6 et exécuter toute la suite CTest, en vérifiant la présence de HubTests, ProfileTests et WindowsTokenStoreTests. La configuration de tests est décrite dans [BUILD](BUILD.md).
2. Dans un compte système d’essai, lancer deux fois le Hub, créer deux profils fictifs et ouvrir les deux fenêtres. Vérifier leurs titres, l’indépendance du Hub et l’absence de doublon en rappelant une fenêtre. Ouvrir également un ancien profil multi-compte.
3. Avec un profil réservé à la qualification connectée, vérifier Login avec une entrée de trousseau valide, le parcours natif en cas de nouvelle authentification et l’actualisation du pseudo et de la miniature. Les scénarios automatisés restent hors ligne.
4. Vérifier la réduction dans la zone de notification, l’activation ou le clignotement depuis une autre application, la fermeture normale et globale, puis la restauration. Ouvrir un raccourci `--profile` avec restauration activée et vérifier que seul ce profil démarre.
5. Examiner à 100 %, 125 %, 150 % et 200 % les boutons, le défilement, le pseudo long et les fenêtres de préférences. Vérifier aussi la voix, la vidéo et les notifications dans les configurations qui les activent.

## Qualification #003

La matrice de la base #002 a réussi à `2c8d5f1` (43/43 sur MinSizeRel, RelWithDebInfo et Qt5, 24/24 sur la configuration minimale). Pour la branche #003, compléter les deux scénarios X11 précédents par le test des densités, de la recherche et du profil visible dans la messagerie.

```sh
xvfb-run -a /usr/bin/python3 tests/hub_polish_smoke.py "$dev/build/acheron" /tmp/hub-polish
```

Sur Windows, vérifier le déplacement à la souris et au clavier puis relancer le Hub. Tester les quatre échelles 100–200 %. Installer l’exécutable dans un chemin avec espaces et caractères Unicode, créer un raccourci sur le bureau réel (y compris redirigé), inspecter sa cible, ses arguments et son icône, puis l’ouvrir avec le Hub fermé. Répéter la création et vérifier que le fichier précédent est conservé. Contrôler le refus de permission dans un bureau non inscriptible.

Sous Linux Qt 5.15, les 23 suites CTest et les trois scénarios X11 ont réussi en #003. Le scénario de présentation vérifie l’ordre enregistré après un glisser-déposer, la disparition des cartes non correspondantes après saisie du pseudo, les hauteurs de 414/453/498 pixels logiques pour trois cartes et la largeur du Hub aux facteurs Qt 1,25, 1,5 et 2. Les clics sur Login utilisent uniquement les profils synthétiques. Les erreurs de passerelle sont vérifiées au niveau des états du protocole local. La création de fichiers .lnk et le comportement de la passerelle en ligne restent à qualifier sur leurs plateformes.

## Distribution #004

Avec Python 3, CTest inclut `BuildIdentity` (exécutable réel, source, canal et commit) et `DistributionTests`. Depuis la racine, `python3 tests/test_distribution.py` exerce les canaux, les noms, le contenu du ZIP, les champs invalides et la modification des octets sur un jeu de fichiers fictifs. Tester ensuite un dispatch Windows avec **publish** désactivé et contrôler les artefacts sur un poste propre. [Qualification du paquet](RELEASING.md)

Pour #004, la validation Linux Qt 5.15 a réussi sur les quatre suites ciblées BuildIdentity, DistributionTests, ProfileTests et HubTests. Les cinq tests Python utilisent des archives fictives. Un essai X11 a vérifié le contenu exact copié depuis About. Le YAML, les dépendances des jobs, les permissions déclarées et la syntaxe Bash de publication ont été contrôlés localement. L’exécution GitHub Actions sur Windows et le démarrage de la distribution autonome restent à qualifier.

CI qualification of mission #004 on the temporary branch. Public release is not authorized; packaging qualification remains separate.
