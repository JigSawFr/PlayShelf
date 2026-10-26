<!-- translated from docs/troubleshooting.md, sha256 16233cef051c -->
# Dépannage

Que faire quand un catalogue ne peut pas être lu, qu'un serveur n'est pas trouvé, qu'un téléchargement échoue ou qu'un jeu installé ne démarre pas, et comment envoyer un rapport.

## Le catalogue ne peut pas être lu

*Le catalogue n'a pas pu être lu*, avec la raison :

- **le certificat du serveur n'a pas été approuvé** : un serveur de votre réseau avec son propre certificat est approuvé une fois sa clé comparée, à la question que pose PlayShelf ; un serveur dont la clé a changé doit d'abord être oublié. Voir [Certificats](servers.md#certificats).
- **the index is not JSON**, **the server's answer is not a catalogue** : l'adresse n'est ni l'API du serveur ni un index JSON. Vérifiez-la dans un navigateur, et choisissez dans le profil du serveur le *Catalogue* qu'il parle. Voir [Ce que lit PlayShelf](servers.md#ce-que-lit-playshelf).
- Un nom d'utilisateur ou un mot de passe refusé : modifiez-les dans le profil du serveur.

**Relire**, sous le catalogue, le relit depuis le serveur.

## Aucun serveur trouvé sur le réseau

*Chercher sur ce réseau* liste les serveurs qui répondent à une demande de découverte sur le port UDP 8465 (comme AeroFoil et Ownfoil). Rien trouvé :

- le serveur est éteint, ou sur un autre réseau que la console (un Wi-Fi invité, un autre sous-réseau) ;
- un pare-feu sur l'ordinateur du serveur bloque le port UDP 8465 ;
- le serveur ne répond pas à la découverte : ajoutez-le à la main avec *Ajouter un serveur* et son adresse `https://`.

## Un téléchargement échoue

Le paquet dit pourquoi, et quoi faire : voir [Quand un téléchargement s'arrête](downloads.md#quand-un-téléchargement-sarrête).

## Un jeu installé ne démarre pas

- sys-patch ou les sigpatches manquent, ou sont plus anciens que le firmware de la console : mettez-les à jour.
- Une mise à jour ou un contenu additionnel demande un firmware plus récent que celui de la console.
- Le jeu de base manque : une mise à jour ou un contenu additionnel seul ne démarre pas. La fiche du jeu installe ce qui manque.

## Des noms ou des icônes manquent

Un jeu connu seulement par son nom de fichier prend son nom et son icône dans la base de titres : *Préférences › Base de titres › Télécharger*, puis **Mettre à jour** de temps en temps. Une source en échec le dit sous *Sources*, avec la raison.

## Envoyer un rapport

*Préférences › Aide et diagnostic* :

- **Exporter un rapport de diagnostic** en enregistre un sur la carte SD, à joindre à un [signalement de bug](https://github.com/JigSawFr/PlayShelf/issues/new). Il ne contient jamais le numéro de série de la console.
- **Envoyer un rapport…** le met dans un gist secret de votre compte GitHub et affiche son lien avec un QR code.

Un plantage est enregistré dans un fichier de plantage, que le démarrage suivant signale et propose d'envoyer.

Retour au [README](../../README.md).
