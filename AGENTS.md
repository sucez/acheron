# Travailler sur Acheron Custom

Le client dérive d’ouwou/acheron. Conserver la licence GPL, les crédits et les parcours de compilation de l’amont. La branche `master` du fork sert de référence. Pour une mission, partir de la branche indiquée, travailler sur une branche dédiée et proposer les changements à la revue avant intégration.

- Consulter [l’architecture](docs/ARCHITECTURE.md) avant de modifier le stockage, le démarrage ou la gestion des comptes. Conserver les espaces existants des profils valides.
- Compiler selon [BUILD](docs/BUILD.md) et vérifier selon [TESTING](docs/TESTING.md). Les options du cloud sont locales. Les valeurs par défaut et les variantes du workflow Windows font partie des configurations prises en charge.
- Utiliser des profils jetables, des lignes de comptes synthétiques et une reconnexion désactivée pour les tests automatisés. Garder les bases, journaux, trousseaux et jetons hors de Git.
- Consigner les changements visibles dans [CHANGELOG](CHANGELOG.md), les choix de compatibilité dans [DECISIONS](docs/DECISIONS.md) et les évolutions envisagées dans [ROADMAP](ROADMAP.md).
