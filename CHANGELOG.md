# Journal d’Acheron Custom

## En revue — mission #001

- Le nom d’entrée de QtKeychain sous Windows inclut le nom du profil. Après mise à niveau depuis le prototype, renseigner de nouveau le jeton des comptes concernés dans chaque profil Windows. Les fiches de comptes SQLite sont conservées.
- Le menu et la ligne de commande appliquent la même validation ASCII stricte. La ligne de commande accepte également `--profile=NOM` et rejette les options de profil répétées.
- Ajout de tests unitaires et d’un scénario Linux hors connexion qui exerce plusieurs processus et le redémarrage.

## Prototype multi-instance — a56e09f

Les quatre commits après `08dd089` introduisent les profils nommés avec verrou, le service de trousseau par profil, le lancement depuis le menu View et la procédure de vérification manuelle. Le profil implicite se nomme `principal`.
