# Compiler Acheron Custom

Les commandes partent de la racine du dépôt. CMake construit `acheron` et les exécutables Qt Test. Le workflow du dépôt fait foi pour les versions, les archives, les sommes de contrôle et les variantes de distribution. [Workflow Build](../.github/workflows/build.yml)

## Linux cloud Debian 13

Dans l’environnement préparé, le préfixe utilisateur fournit Qt 5.15, QtKeychain, CMake et Ninja. Le compilateur est GCC 14. Deux sous-modules fournissent le segmenteur d’emoji et le générateur de QR codes. La configuration locale de CMake est conservée hors du dépôt.

```sh
cd /workspace/acheron
. /workspace/.acheron-qt5/env.sh
cmake --build "$dev/build" --parallel 4
xvfb-run -a ctest --test-dir "$dev/build" --no-tests=error --output-on-failure --timeout 120 -j2
```

Pour reconfigurer ce même répertoire après un changement d’options, charger le préfixe comme ci-dessus puis exécuter les commandes suivantes.

```sh
git submodule update --init --depth 1 -- vendor/emoji-segmenter vendor/qrcodegen
cmake -S . -B "$dev/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DUSE_VCPKG=OFF -DBUILD_TESTS=ON -DALLOW_PLAIN_CURL=ON \
  -DCURL_LIBRARY=/usr/lib/x86_64-linux-gnu/libcurl.so \
  -DENABLE_VOICE=OFF -DENABLE_RNNOISE=OFF -DENABLE_VIDEO=OFF \
  -DENABLE_FFMPEG=OFF -DENABLE_LOTTIE=OFF
cmake --build "$dev/build" --parallel 4
```

Ce parcours nécessite aussi OpenSSL, zlib, le pilote Qt SQLite et Xvfb pour les tests d’interface. Qt 6.8 fourni par Debian échoue sur les indicateurs de permissions 64 bits du projet. Qt 5.15 emprunte le chemin de compatibilité du dépôt. Sur un bureau avec affichage, lancer `"$dev/build/acheron" --profile travail`.

Pour Linux avec les fonctions multimédias, suivre le job `build-linux` avec ses sous-modules récursifs, Qt 6.12, vcpkg, curl-impersonate et FFmpeg. Sa matrice vérifie les deux valeurs de `ENABLE_VIDEO`. Les paquets et la préparation de la bibliothèque curl statique figurent dans ce job.

## Windows x64

Utiliser un shell de développement Visual Studio 2022 x64, CMake, Git et le kit Qt MSVC x64. Pour la variante Qt 6 du workflow, installer Qt 6.12.0 et qtimageformats. Préparer le répertoire `curl-impersonate` et le préfixe FFmpeg selon les étapes « Install curl-impersonate » et « Install ffmpeg » du job `build-windows`, y compris le contrôle SHA-256 FFmpeg. Positionner `QT_ROOT_DIR` sur le kit Qt et ajouter ses outils au PATH.

```powershell
git submodule update --init --recursive
.\vendor\vcpkg\bootstrap-vcpkg.bat
$curlDir = Join-Path $PWD "curl-impersonate"
$ffmpegDir = Join-Path $PWD "ffmpeg"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md `
  "-DCMAKE_PREFIX_PATH=$env:QT_ROOT_DIR;$ffmpegDir" `
  "-DCURL_INCLUDE_DIR=$curlDir/include" `
  "-DCURL_LIBRARY=$curlDir/lib/libcurl-impersonate_imp.lib" -DBUILD_TESTS=ON
cmake --build build --config MinSizeRel --parallel
$env:PATH = "$curlDir/lib;$ffmpegDir/bin;$env:QT_ROOT_DIR/bin;$env:PATH"
$env:QT_FORCE_STDERR_LOGGING = "1"
ctest --test-dir build -C MinSizeRel --output-on-failure --timeout 600
.\build\MinSizeRel\acheron.exe --profile travail
```

Le répertoire `ffmpeg` correspond au dossier extrait tel que renommé par le workflow. Pour distribuer l’application, suivre également les étapes windeployqt et de copie des DLL et ressources. Les variantes RelWithDebInfo, minimale et Qt 5.15.2 ont leurs propres étapes de CI. Les valeurs Windows par défaut, notamment voix et vidéo, sont conservées.
