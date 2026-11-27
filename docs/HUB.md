# Acheron Hub

## Profils et préférences

Le répertoire de données Qt `sucez/Acheron-Hub` contient `hub.lock` et `registry.ini`. Le Hub énumère ses répertoires frères `Acheron-Custom-<nom canonique>` et ignore les liens symboliques. Chaque version à profils crée ce répertoire pour le verrou avant d’accéder à QSettings ou aux comptes. Ce point commun fonctionne aussi quand les paramètres Qt se trouvent dans le registre Windows. Refaire la découverte ne déplace aucune donnée.

Le répertoire du profil établit son identité. Le registre du Hub utilise un INI QSettings de schéma 1 avec synchronisation atomique et verrouillage des écritures. Il associe les alias, couleurs, archives et choix de lancement au nom technique. Une erreur de format ou une version inconnue laisse les profils accessibles avec leurs libellés de base et empêche d’écraser les préférences. Pour repartir avec des préférences neuves, quitter le Hub et déplacer son seul fichier `registry.ini` vers une sauvegarde.

Une session publie sa propre fiche `hub-card.json` par remplacement atomique. Elle contient la légende et, si le client a déjà récupéré son avatar par son fonctionnement normal, une miniature PNG. Le Hub limite la fiche à 32 Kio et le décodage à 64 × 64 pixels. Une photo indisponible donne une initiale. Pour un ancien profil sans fiche utilisable, le Hub ouvre brièvement `acheron.sqlite` en lecture seule et lit au plus deux lignes, limitées aux noms de compte. La fiche et sa miniature restent en cache en mémoire jusqu’à une modification. Les messages, les bases de conversation et le trousseau ne participent pas à la découverte.

## Fenêtres et lancement

| Bouton | Signification |
| --- | --- |
| Login | Le profil est disponible. Le lancement démarre une session détachée avec le nom technique validé. |
| Launching… | Un lancement est en cours. Le Hub attend son verrou et son serveur local. |
| Connecting… | La session a signalé au moins un client en cours de connexion. |
| Switch to window | La session répond et peut montrer sa fenêtre, y compris après réduction dans la zone de notification. |
| Reconnect | Le compte unique a signalé un échec du trousseau ou un refus d’authentification. Ouvre le gestionnaire natif pour se reconnecter. |
| In use | Un verrou existe et la session ne répond pas encore au protocole. Le Hub conserve le verrou et évite une seconde instance. |
| Retry | Le lancement a échoué ou aucun verrou n’est apparu dans le délai de 15 secondes. |

**Settings** permet de démarrer le Hub seul (valeur initiale), restaurer les sessions précédentes, lancer une sélection ou ouvrir le compte par défaut. Ce dernier choix réduit le Hub. **Launch all accounts** échelonne les démarrages toutes les 450 ms et saute les profils occupés, archivés ou exclus de ce bouton. Le choix explicite d’un compte dans les préférences est indépendant de cette exclusion. Toutes les options automatiques ignorent les profils archivés. Elles s’appliquent uniquement au démarrage d’un nouveau processus Hub.

Une session crée `restore-session` dans son répertoire. Quitter normalement cette session enlève le marqueur. Une fermeture globale demandée par le Hub le conserve pour la restauration, comme un arrêt brutal. Fermer uniquement le Hub laisse les sessions fonctionner. Un Hub relancé les retrouve par leurs verrous et leur serveur local. Si le bureau fournit une zone de notification, les paramètres proposent de masquer le Hub à sa fermeture. **Quit all sessions** demande confirmation, attend la fermeture normale et signale les fenêtres qui ne répondent pas.

## Protocole local, version 1

Une requête ASCII et sa réponse occupent chacune une ligne terminée par LF, de la forme `1 <mot>`, sur une nouvelle connexion QLocalSocket. Le serveur accepte une ligne de 64 octets maximum et coupe un client inactif au bout de 1,5 seconde. Le client asynchrone attend au plus 1,2 seconde. Le nom de l’endpoint est dérivé par SHA-256 du répertoire de données et du répertoire personnel. Le serveur demande l’accès utilisateur de Qt. Sous Linux, le socket réside dans le répertoire d’exécution de l’utilisateur lorsque celui-ci est disponible.

| Destinataire | Requête | Réponse ou effet |
| --- | --- | --- |
| Hub ou session | `activate` | `ok`. Montre, restaure et demande d’activer la fenêtre. Une demande d’attention accompagne l’activation. |
| Session | `status` | `window`, `connecting` ou `needs-auth`. |
| Session | `login` | `ok`. Ouvre la gestion des comptes dans la fenêtre concernée. |
| Session | `close` | `ok`. Quitte normalement et retire le marqueur de restauration. |
| Session | `suspend` | `ok`. Quitte normalement et conserve le marqueur. |
| Tous | Version ou requête inconnue | `error`. Une trame trop longue provoque la déconnexion. |

Les verrous Qt du Hub et des profils sont indépendants. Seul leur propriétaire démarre le serveur correspondant. Le Hub surveille les répertoires avec QFileSystemWatcher et réinterroge les sessions actives ou en démarrage toutes les deux secondes. Une perte de réponse laisse le profil occupé tant que son verrou existe. Une session ancienne sans serveur local reste accessible par la barre des tâches. La présence d’une fenêtre n’atteste pas une authentification réussie.
