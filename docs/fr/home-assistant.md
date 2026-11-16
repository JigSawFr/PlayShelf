<!-- translated from docs/home-assistant.md, sha256 27be09dda2a3 -->
# Home Assistant et l'agent Boréale

PlayShelf peut dire à [Home Assistant](https://www.home-assistant.io/) ce qu'il fait : les téléchargements restants, celui en cours, l'espace libre de la carte SD. Il le fait par l'**agent Boréale**, un petit programme qui tourne en arrière-plan sur la console et garde une seule connexion à votre broker MQTT pour tous les homebrews Boréale (PlayGuard et PlayShelf aujourd'hui). Chaque homebrew y ajoute ses propres informations et ses propres ordres ; l'agent publie seul ceux de la console.

Il vous faut un broker MQTT (le module Mosquitto de Home Assistant convient), l'[intégration Boréale](https://github.com/JigSawFr/boreale-ha) dans Home Assistant, et Atmosphère sur la console (l'agent est un sysmodule).

## Ce qui apparaît dans Home Assistant

- **Par l'agent seul**, sans aucun homebrew ouvert : le matériel, l'alimentation, le réseau et le stockage de la console, si elle est en ligne et quand elle a été vue pour la dernière fois, et les notifications des homebrews Boréale.
- **Avec PlayShelf** : *Téléchargements PlayShelf restants*, *Progression du téléchargement PlayShelf*, *Dernière installation PlayShelf*, *Espace libre de la carte SD (PlayShelf)*, *PlayShelf télécharge* et *Problème de téléchargement PlayShelf*, et le bouton *Mettre en pause les téléchargements PlayShelf*. Ils apparaissent quand PlayShelf est ouvert et deviennent indisponibles quand il se ferme. Le bouton met la file en pause ; pour reprendre, utilisez la console.
- **Avec PlayGuard** : le contrôle parental, le temps de jeu, les jeux et les ordres de Home Assistant, comme le dit sa propre documentation.

## Le mettre en place depuis PlayShelf

1. **Préférences › Lien Home Assistant** : activez *Se connecter au broker* et indiquez l'adresse et le port du broker, *Connexion sécurisée (TLS)* s'il en utilise une, et l'utilisateur et le mot de passe qu'il attend. Ces réglages sont partagés : PlayGuard et l'agent lisent le même fichier (`sd:/config/boreale/link.conf`), vous les réglez donc une seule fois.
2. **Préférences › Agent Boréale** : choisissez **Installer**. PlayShelf porte une copie de l'agent, la met sur la carte SD, la démarre et vérifie qu'elle répond ; si la nouvelle ne répond pas, la précédente est remise. **Démarrer avec la console** le garde en marche après un redémarrage.
3. Dans Home Assistant, ajoutez l'intégration Boréale : elle trouve seule la console sur le broker.

Le même écran propose ensuite **Mettre à jour** quand PlayShelf porte un agent plus récent, **Arrêter maintenant** ou **Démarrer maintenant**, et **Retirer**. Arrêter ou mettre à jour fait quitter proprement le broker à l'agent : Home Assistant voit la console passer hors ligne au lieu d'attendre un délai.

## Publier l'état de PlayShelf

*Préférences › Publier l'état de PlayShelf (agent Boréale)* décide si PlayShelf parle à l'agent. Désactivé, il se tait : ni téléchargements, ni espace libre, ni bouton, et ses entités deviennent indisponibles. L'agent et les autres homebrews ne sont pas touchés.

## Bon à savoir

- Ce que l'agent et PlayShelf publient va uniquement vers *votre* broker.
- Le mot de passe du broker est gardé dans `link.conf` sur la carte SD, en clair, comme les réglages des autres homebrews. Utilisez un utilisateur qui n'accède qu'aux topics de cette console, et TLS quand le broker le permet.
- L'agent est installé par PlayShelf ou par PlayGuard, celui que vous utilisez en premier ; les deux gèrent le même.
