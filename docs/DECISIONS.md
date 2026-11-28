# Décisions et audit #001

## Base examinée

| Référence du fork | Commit | Relation |
| --- | --- | --- |
| master, custom | 08dd089 | Même objet Git. |
| feature/multi-instance | a56e09f | Quatre commits après master. abd83c3 initialise profils et verrou, 0f96463 sépare les services de trousseau, 0437a24 ajoute le menu, a56e09f documente le prototype. |
| ci/multi-instance | 7e93796 | Un commit de plus que feature. Le seul écart est un ajout de deux lignes dans CUSTOM-PROFILES.md. |
| ouwou/acheron master | eb4c7c3 | Deux commits après la base du fork au moment de l’audit. Leur intégration relève d’une décision distincte. |

## Identifiants Windows

**Important, corrigé.** Le backend Windows Credential Store du sous-module QtKeychain est activé par défaut. Son code utilise la clé seule pour CredReadW, CredWriteW et CredDeleteW. Le prototype changeait le service mais gardait la même clé pour un même identifiant de compte. Deux profils et l’application officielle pouvaient donc viser la même entrée. Le test historique de deux comptes différents ne couvre pas ce cas.

La lecture, l’écriture et la suppression emploient désormais la clé préfixée sur Windows. Le format de clé Linux est conservé dans le service existant pour préserver les trousseaux du prototype. Une migration automatique depuis la clé Windows partagée risquerait d’attribuer le jeton au mauvais profil. La décision est de laisser cette entrée en place et de demander une nouvelle saisie dans le profil concerné. Une ancienne version du logiciel peut encore accéder à son ancienne clé.

## Identité et validation

**Important, corrigé.** Un argument `--profile=deux` était ignoré et pouvait ouvrir le profil implicite. Le parseur traite maintenant les deux syntaxes et rejette un nom manquant ou un second choix.

**Mineur, corrigé.** Le prototype normalisait en minuscules avant la validation. Certains caractères Unicode devenaient ainsi des lettres ASCII. De plus, l’ancre finale de la regex autorisait un saut de ligne final et le menu supprimait les espaces que la CLI rejetait. Une fonction commune vérifie la chaîne ASCII entière avant sa conversion en minuscules. Les noms documentés conservent leur identité. Un utilisateur ayant créé sous Linux un ancien profil au nom hors contrat devra récupérer ses données séparément.

## Concurrence et portée

Aucun défaut critique identifié. L’emplacement du verrou, l’initialisation de la session après son acquisition, le répertoire temporaire d’images par gestionnaire et les emplacements de SQLite, des journaux et des thèmes sont cohérents pour des profils locaux. Les téléchargements explicitement exportés et les ressources installées sont partagés par conception. Garder le modèle multiprocessus existant permet de réutiliser le multi-compte natif.

**Amélioration ouverte.** Le dialogue de verrou combine déjà-ouvert et erreur de droits. Distinguer ces motifs pourrait faciliter le diagnostic sur des répertoires itinérants ou indisponibles. Avant toute prise en charge de profils sur plusieurs machines, qualifier le système de fichiers, le registre et les trousseaux indépendamment.

**Important, hérité de l’amont.** Si un proxy utilise un mot de passe, AccountRepository conserve son URL sérialisée dans la base du profil. ProxyConfig encode les caractères pour une URL, ce qui est réversible. L’isolation entre profils limite les collisions mais laisse les secrets du proxy lisibles pour qui a accès à cette base. Une migration vers le trousseau préserverait la reconnexion et demanderait de gérer les écritures partielles et les anciennes bases. Une saisie à chaque connexion éviterait la migration au prix de la reconnexion automatique. Cette décision dépasse la correction du multi-instance.

**Mineur, hérité de l’amont.** En supprimant un compte, AccountRepository tente la suppression du jeton puis efface la ligne SQLite même si le trousseau signale un échec. Une entrée orpheline peut subsister dans le profil. Un retour explicite de l’échec à l’interface aiderait à réessayer cette suppression.

## #002 — association compte, carte et processus

Un répertoire de profil constitue une unité de verrouillage et de trousseau. Une carte lance exactement cette unité. Un profil historique multi-compte conserve tous ses comptes dans la même fenêtre. Un alias modifie uniquement le registre du Hub. Les journaux, les comptes, les réglages et les entrées QtKeychain conservent leur identité.

Les fenêtres peuvent survivre au Hub ou démarrer par un raccourci. Chaque session possède donc le serveur de contrôle et le marqueur de restauration de son profil. Les sessions publient séparément leurs aperçus par QSaveFile. Seul le Hub possède les préférences communes. Cette répartition évite les conflits entre la publication d’un avatar et un renommage. [Protocole et récupération](HUB.md)

## #003 — ordre, préférences et raccourcis

Le tri utilise les identifiants des répertoires de profils. Les entrées inconnues ou répétées dans l’ordre stocké sont ignorées. Un tri stable conserve l’ordre relatif en cas d’égalité et sépare les favoris. Filtrer les cartes ne réécrit pas les préférences. Les clés ajoutées sont facultatives dans le schéma 1.

Le raccourci Windows utilise IShellLinkW et IPersistFile. Le programme transmet séparément le chemin de l’exécutable et les arguments. Il enregistre un fichier temporaire sur le même volume, puis le publie avec MoveFileW, qui refuse une destination existante.

## #004 — identité et publication

Le numéro de distribution est indépendant du nom Qt utilisé par les profils et QtKeychain. La publication réutilise le job Windows complet pour conserver ses options multimédias. Le déclenchement manuel et le tag préexistant séparent l’autorisation de publication du simple push. Le manifeste décrit les octets effectivement distribués. La signature et le modèle de confiance relèvent de la mission updater. [Contrat du manifeste](RELEASING.md)
