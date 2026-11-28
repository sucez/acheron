# Distribuer Acheron Custom

## Version et provenance

`VERSION` est la source de vérité. Pour le canal stable, choisir trois entiers sans zéro initial, par exemple `0.4.0`. Pour une preview, utiliser une version distincte telle que `0.4.0-preview.1`. Ces formes constituent le sous-ensemble SemVer accepté. Les trois composantes numériques sont limitées à 65535 pour la ressource Windows. Une version et son tag désignent une seule livraison.

Le canal CMake par défaut est `development`. Les builds de publication demandent explicitement `stable` ou `preview`. Le numéro, le canal, le commit de compilation et l’architecture sont visibles dans **Hub settings → About Acheron Custom**, copiables avec **Copy technical information**, et lisibles via `acheron --build-info`. Une archive source sans Git indique `unknown` en développement. Le workflow compile un checkout propre du SHA de l’événement.

## Préparer et qualifier

1. Choisir la version, modifier `VERSION` et le changelog, faire relire et intégrer le commit sur la branche de distribution. Utiliser les protections de branche et de tags du dépôt pour réserver ces opérations aux mainteneurs.
2. Pour que GitHub propose le déclenchement manuel, le workflow doit exister sur la branche par défaut. Le workflow accepte un dispatch sur `feature/multi-instance` ou `master` du fork. Configurer l’environnement GitHub **release** avec ces seules branches autorisées et des approbateurs. Ces règles doivent être réglées dans GitHub.
3. Dans **Actions → Acheron Custom release → Run workflow**, sélectionner la branche revue, saisir le contenu exact de `VERSION` et choisir le canal. Laisser **publish** désactivé. Le parcours par défaut construit uniquement l’artefact Actions `release-assets`.
4. Le job Windows complet partagé avec la CI configure Qt 6 x64 en MinSizeRel, construit avec les fonctions voix/vidéo habituelles et exécute CTest. Le script de staging crée un répertoire neuf, déploie Qt, ajoute les runtimes, certificats, police et licences, et lance le binaire avec un PATH réduit. La version PE et l’identité retournée doivent correspondre à la demande. Les graphismes applicatifs sont incorporés par Qt.
5. Télécharger et déballer l’artefact Actions pour obtenir le ZIP de distribution, `SHA256SUMS` et `manifest.json`. Sur un PC Windows propre sans SDK Qt, extraire le ZIP dans un dossier neuf. Vérifier le lancement du Hub, About, la messagerie, les plugins SQLite/TLS, la voix et la vidéo. Contrôler les notices et les obligations de mise à disposition du code source correspondant aux bibliothèques effectivement distribuées. Conserver le SHA, les références des dépendances et les résultats de qualification.

Depuis un checkout Windows préparé selon BUILD, avec la configuration MinSizeRel correspondante et le shell de compilation MSVC, les commandes de packaging sont les suivantes. Les chemins de travail sont relatifs à la racine du dépôt. Utiliser des dossiers `stage` et `dist` neufs.

```powershell
$version = (Get-Content VERSION -Raw).Trim()
$channel = "stable"
$commit = git rev-parse HEAD
python tools/release/distribution.py check $version $channel $commit
python tools/release/stage_windows.py $version $channel $commit
python tools/release/distribution.py create $version $channel $commit
python tools/release/distribution.py verify $version $channel $commit
Get-FileHash "dist/Acheron-Custom-$version-windows-x64.zip" -Algorithm SHA256
```

## Publication autorisée

Après qualification et accord de la direction technique, un mainteneur prépare le tag dédié `v<VERSION>` sur le SHA revu. Le tag peut être annoté ou léger. Il ne déclenche aucune publication par lui-même. Relancer le workflow sur ce même commit avec **publish** activé. Si la branche a avancé, choisir une nouvelle livraison ou réexaminer la préparation. Le job de publication vérifie à nouveau le ZIP téléchargé, ses métadonnées et le commit pointé par le tag. Il prépare un brouillon avec les notes générées par GitHub et les trois fichiers, puis rend la release publique. Le canal preview produit une préversion GitHub.

Le run de publication recompile et teste ses propres octets. Les empreintes du run de qualification peuvent différer. Contrôler l’artefact du run courant avant d’approuver son environnement. Seul le job de publication demande `contents: write`. En cas d’échec d’upload, inspecter le brouillon et ses fichiers avant toute nouvelle tentative. Une livraison publique est immuable par convention. Une correction reçoit une autre version.

## Contrat du manifeste v1

Tous les champs ci-dessous sont obligatoires. Le JSON distribué est encodé en UTF-8, avec les clés triées et une fin de ligne LF. Le validateur régénère les octets canoniques à partir du ZIP et de l’identité attendue et compare aussi `SHA256SUMS`.

| Champ | Valeur v1 |
| --- | --- |
| `schema_version` | Entier 1. |
| `version`, `channel` | Version exacte de VERSION et canal compatible stable ou preview. |
| `commit` | SHA Git complet en 40 chiffres hexadécimaux minuscules. |
| `release_notes` | URL HTTPS de la release du dépôt pour le tag `v<VERSION>`. |
| `packages` | Tableau contenant le paquet Windows x64. |
| `packages[].os`, `architecture` | Chaînes `windows`, `x64`. |
| `filename` | `Acheron-Custom-<VERSION>-windows-x64.zip`. |
| `size`, `sha256` | Taille en octets du ZIP et SHA-256 hexadécimal minuscule de son contenu complet. |
| `url` | URL HTTPS de cet asset sous `github.com/sucez/acheron/releases/download/v<VERSION>/`. |

La publication porte sur l’intégrité et la traçabilité. Une personne capable de remplacer le manifeste et le ZIP peut recalculer leurs empreintes. L’updater devra authentifier le manifeste, par exemple avec une signature détachée de ses octets et des clés publiques de confiance embarquées. Définir aussi rotation, révocation et protection contre le rejeu ou le retour à une version vulnérable. Garder les clés privées hors du dépôt.

## Installation et future mise à jour

L’archive contient uniquement le logiciel. Le Hub garde ses métadonnées dans l’espace Qt `sucez/Acheron-Hub`, chaque profil ses réglages et ses données sous l’identité `Acheron-Custom-<id>`. Les identifiants sont gérés dans le trousseau du système. Extraire les nouvelles versions dans des répertoires de programme distincts de ces espaces.

Sous Windows, un programme en cours d’exécution peut verrouiller son EXE et ses DLL. Le futur updater devra authentifier le manifeste, vérifier le paquet, contrôler les chemins, liens et tailles avant extraction, puis préparer une nouvelle installation séparée. Il devra coordonner le Hub et toutes les sessions, obtenir leur fermeture propre et attendre la libération des fichiers. Un processus auxiliaire pourra alors basculer entre répertoires versionnés avec journal transactionnel, conserver la version précédente et restaurer celle-ci en cas de panne. La restauration des fenêtres devra respecter les fermetures volontaires et les préférences existantes. Prévoir la compatibilité des données avec le retour arrière avant toute migration.
