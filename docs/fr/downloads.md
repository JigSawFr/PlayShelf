<!-- translated from docs/downloads.md, sha256 ebe4784f0349 -->
# Téléchargements

La fiche d'un jeu dans le *Catalogue* ajoute ses paquets aux **Téléchargements** : son jeu de base, ses mises à jour et ses contenus additionnels, chacun étant un paquet. Ils sont installés l'un après l'autre, directement du serveur vers la console : rien n'est d'abord enregistré en entier sur la carte SD.

## La file

Chaque jeu montre ses paquets, avec leur progression, leur vitesse et le temps restant ; les **Téléchargements** de la barre latérale indiquent combien il en reste et où ils en sont, depuis n'importe quel onglet.

- Ⓐ sur un jeu ou un paquet : le **Mettre en pause** ou le **Reprendre**, **Réessayer**, ou le **Retirer** (ce qu'il a déjà installé est supprimé).
- Les actions de la file : **Tout mettre en pause**, **Tout reprendre**, **Retirer les installés**.
- Un paquet déjà sur la console n'est pas réinstallé.

La file est gardée sur la carte SD (`sd:/config/playshelf/queue.json`). Un téléchargement interrompu (PlayShelf fermé, la console éteinte) est en pause au démarrage suivant, et son installation reprend là où elle s'était arrêtée une fois relancée.

## Quand un téléchargement s'arrête

Une connexion perdue ou un serveur qui ne peut pas répondre pour le moment : PlayShelf réessaie de lui-même trois fois, depuis là où il s'était arrêté, puis le paquet dit pourquoi il a échoué et quoi faire :

| Message | Que faire |
|---|---|
| connexion perdue, serveur indisponible | **Réessayer** une fois le serveur revenu. Un serveur à deux adresses : **Utiliser l'autre adresse du serveur**. |
| accès refusé | Le nom d'utilisateur ou le mot de passe : **Modifier le serveur**. |
| plus sur le serveur | Le paquet a été renommé ou déplacé : **Le chercher dans le catalogue** le retrouve. |
| clé du serveur non reconnue | Le serveur montre une autre clé : oubliez l'ancienne dans son profil, puis approuvez-le à nouveau. Voir [Certificats](servers.md#certificats). |
| espace libre insuffisant | Libérez de la place sur la carte SD, puis **Réessayer**. |
| paquet endommagé | Un contenu du paquet n'est pas ce qu'il devrait être : le fichier sur le serveur est peut-être endommagé. |

Un contenu qui n'est pas signé par Nintendo arrête son paquet : il a peut-être été modifié ou endommagé. **Installer quand même** continue, à vos risques.

## Réglages

Sous la file, et dans [`config.json`](../config.md) :

- **Rester éveillé pendant les téléchargements** : la console ne se met pas en veille d'elle-même pendant les téléchargements (activé par défaut).
- **Connexions par téléchargement**, de 1 à 8 (4 par défaut) : combien de requêtes un téléchargement fait à la fois, chacune pour un morceau de 4 Mo du paquet. Plus de connexions aident avec un serveur qui sert chaque connexion lentement.
- **Télécharger à l'avance** : pendant qu'un paquet s'installe, le suivant ou les deux suivants sont récupérés sur la carte SD, quand elle a la place pour eux et pour leur installation ; leur installation part alors du fichier. Chacun occupe deux fois sa taille sur la carte SD jusqu'à son installation (désactivé par défaut).

## Avant la première installation

Les jeux installés ainsi ont besoin de sys-patch ou de sigpatches pour démarrer. Quand PlayShelf ne trouve ni l'un ni les autres sur la console, il le dit avant d'installer et demande s'il faut continuer.

Retour au [README](../../README.md).
