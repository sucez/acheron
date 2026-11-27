# Architecture des profils

Un processus Acheron héberge un profil. Dans ce processus, une seule `Core::Session` gère plusieurs `ClientInstance`, identifiés par leur identifiant de compte Discord. Deux processus peuvent donc héberger chacun plusieurs comptes. Choisir un profil ne sélectionne pas un compte Discord.

## Démarrage et concurrence

`main.cpp` détermine le nom canonique et définit l’organisation Qt `sucez` et l’application `Acheron-Custom-<nom>`. Sous Windows, ce choix précède la lecture du paramètre de moteur de polices. Après création de QApplication, une saisie invalide affiche un avertissement et termine le processus avec le code 1. Le chemin de données Qt contient `profile.lock`. Le processus doit obtenir le QLockFile avant d’initialiser le thème, le journal, SQLite ou la session. Un échec de verrouillage laisse l’instance existante en place.

Le menu View lance le même exécutable avec QProcess::startDetached et des arguments séparés. Le processus enfant acquiert lui-même le verrou, y compris lors de deux lancements concurrents. Le parent ne surveille pas l’enfant. La fermeture normale sauvegarde la géométrie. L’option de réduction dans la zone de notification peut maintenir le processus et son verrou actifs. Quitter depuis cette zone termine alors la session. Les clients sont arrêtés avant la fermeture de SQLite, du journal et la destruction du verrou.

## Emplacements audités

| Donnée | Propriétaire et emplacement |
| --- | --- |
| Préférences et géométrie | QSettings dans l’espace organisation/application. Sous Linux natif, fichier dans le répertoire XDG de configuration. Sous Windows natif, paramètres utilisateur dans le registre. |
| Comptes, options de connexion, métadonnées de session | `Storage::DatabaseManager` ouvre `acheron.sqlite` sous AppDataLocation du profil. `AccountRepository` utilise cette connexion. Au démarrage, Session relit cette liste et tente la connexion uniquement pour les comptes marqués auto-connect. |
| Données de session Discord en cache | `cache_<id>.sqlite` dans le même répertoire, ou SQLite en mémoire. La base sur disque est recréée à l’ouverture du cache de ce compte. Le verrou protège aussi les fichiers SQLite auxiliaires. |
| Thème et journaux | `theme.json`, `acheron.log` et rotations sous AppDataLocation. Les constantes sont relatives à ce répertoire. |
| Images | Le gestionnaire d’images de la session utilise la mémoire et son propre QTemporaryDir. Le système choisit le répertoire temporaire parent. |
| Jetons | QtKeychain, service égal au nom d’application. Clé Linux `account_<id>_token`. Clé Windows `Acheron-Custom-<nom>:account_<id>_token`. Voir [la décision de compatibilité](DECISIONS.md). |
| Fichiers externes | Police et certificats distribués lus à côté de l’exécutable. Les dialogues d’import et d’export accèdent au chemin choisi par l’utilisateur. L’export de médias propose le répertoire Téléchargements du système. |

Sous Linux, les racines usuelles sont `$XDG_DATA_HOME/sucez/Acheron-Custom-<nom>` et `$XDG_CONFIG_HOME/sucez`. Les deux racines peuvent être redéfinies par l’environnement. Sous Windows, Qt résout AppDataLocation dans les données itinérantes de l’utilisateur. L’application officielle conserve sa propre identité Qt.

Le verrou s’applique à un répertoire de données sur un système de fichiers qui prend en charge QLockFile. Sa durée maximale par ancienneté est désactivée pour les sessions longues. La détection d’un propriétaire disparu dépend de Qt et du système. Utiliser des racines stables pour toutes les instances d’un même utilisateur. Changer uniquement la racine de données crée un autre verrou alors que la configuration ou le trousseau peuvent encore être partagés. Les profils servent à séparer l’état applicatif. Les droits du compte système continuent de gouverner les fichiers, le trousseau, le presse-papiers et les périphériques.
