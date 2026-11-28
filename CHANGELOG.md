# Journal d’Acheron Custom

## Mission #004 — distribution, candidate 0.4.0

- Le Hub affiche et copie la version, le canal, le commit et l’architecture.
- La préparation Windows MinSizeRel génère une archive portable, un manifeste v1 et ses empreintes. La publication demande une action explicite du mainteneur.

## Mission #003 — Organisation et ergonomie

- Les favoris occupent le haut du Hub. Les cartes peuvent être déplacées à la souris ou au clavier, recherchées par nom ou pseudo et triées depuis les paramètres.
- Les paramètres proposent trois densités. Une commande dans la barre de menus de la messagerie identifie le profil et rappelle le Hub.
- Sous Windows, chaque profil propose la création d’un raccourci bureau vers la messagerie.
- La sauvegarde des paramètres regroupe les modifications en une seule synchronisation et préserve un registre non vide dont le schéma manque.

## Mission #002 — Acheron Hub

- Le lancement normal ouvre le Hub. Les raccourcis `--profile` conservent leur accès direct à la messagerie.
- Le Hub retrouve les profils existants, ouvre les sessions, rappelle une fenêtre et permet de fermer une session ou l’ensemble. La création utilise le gestionnaire de comptes natif.
- Les cartes proposent un aperçu local, un alias, une couleur et un archivage réversible. Les paramètres permettent le lancement échelonné, la sélection d’un compte par défaut et la restauration facultative.

## Missions #001 et #001-B

- Le nom d’entrée de QtKeychain sous Windows inclut le nom du profil. Après mise à niveau depuis le prototype, renseigner de nouveau le jeton des comptes concernés dans chaque profil Windows. Les fiches de comptes SQLite sont conservées.
- Le menu et la ligne de commande appliquent la même validation ASCII stricte. La ligne de commande accepte également `--profile=NOM` et rejette les options de profil répétées.
- Ajout de tests unitaires et d’un scénario Linux hors connexion qui exerce plusieurs processus et le redémarrage.

## Prototype multi-instance — a56e09f

Les quatre commits après `08dd089` introduisent les profils nommés avec verrou, le service de trousseau par profil, le lancement depuis le menu View et la procédure de vérification manuelle. Le profil implicite se nomme `principal`.
