# Utiliser plusieurs comptes avec Acheron Hub

Lancer `acheron.exe` sous Windows ou `acheron` sous Linux ouvre le Hub. Au premier lancement, **Add an account** propose un identifiant de profil libre, puis ouvre **Manage Accounts** dans la nouvelle fenêtre. Terminer l’ajout avec le parcours de connexion habituel. Annuler avant de valider le nom laisse la liste intacte. Annuler ensuite conserve un profil vide que **Login** permet de compléter.

Les profils Acheron Custom déjà utilisés apparaissent automatiquement, y compris `principal`. **Login** ouvre la messagerie et tente de reconnecter le compte enregistré. **Switch to window** rappelle une fenêtre déjà ouverte. **View → Open Acheron Hub** permet de revenir au lanceur depuis la messagerie. Par défaut, fermer le Hub laisse les fenêtres ouvertes.

Un ancien profil qui contient plusieurs comptes garde une seule carte **Multiple accounts**. Son ouverture conserve les réglages de connexion automatique de chaque compte et donne accès au gestionnaire natif. Le menu de la carte permet de personnaliser son nom et sa couleur. **Archive profile** masque un profil fermé. **Settings → Unarchive** le rend de nouveau visible.

## Raccourcis directs et compatibilité

`acheron.exe --profile principal` et `acheron --profile=travail` ouvrent directement la messagerie du profil choisi. Le nom technique accepte 1 à 32 lettres ASCII, chiffres, tirets ou traits de soulignement, avec une lettre ou un chiffre en première position. Les majuscules sont ramenées en minuscules. La géométrie, les comptes, les préférences et les clés de trousseau gardent leurs emplacements. Le verrou reste actif tant que la session tourne, y compris dans la zone de notification.

**Mise à niveau Windows depuis le prototype antérieur à #001.** L’ancienne entrée du trousseau pouvait appartenir à plusieurs profils. La correction #001 impose une nouvelle saisie dans ce cas et conserve l’ancienne entrée en place. Le passage des versions #001/#001-B au Hub conserve les clés corrigées. Les données d’Acheron officiel restent dans leur espace d’origine.

Les quatre modes de démarrage, la restauration, les erreurs et les essais Windows sont décrits dans [le guide du Hub](docs/HUB.md) et [les procédures de validation](docs/TESTING.md).
