<!-- translated from docs/troubleshooting.md, sha256 edf5576b6ee2 -->
# Dépannage

Que faire quand un catalogue ne peut pas être lu, qu'un serveur n'est pas trouvé, qu'un téléchargement échoue ou qu'un jeu installé ne démarre pas, et comment envoyer un rapport.

## Le catalogue ne peut pas être lu

*Le catalogue n'a pas pu être lu*, avec la raison :

- **cette console n'est pas appairée avec le serveur** : appairez-la de nouveau, à la question que pose PlayShelf ou dans le profil du serveur. Voir [L'appairage](servers.md#lappairage).
- **refusé (la clé du serveur n'est pas celle épinglée)** : le serveur a été réinstallé ou a reçu un autre certificat. Oubliez l'appairage dans le profil du serveur et appairez de nouveau, après avoir comparé la nouvelle clé. Voir [Certificats](servers.md#certificats).
- **un autre serveur que celui qui a été appairé** : l'adresse répond maintenant avec un autre serveur ; vérifiez-la, ou appairez de nouveau.
- **le serveur ne parle pas Lantern** : l'adresse n'est pas celle d'un serveur Lantern. Un serveur d'un autre genre a besoin d'un [pont](https://github.com/JigSawFr/lantern-bridge).

**Relire**, sous le catalogue, le relit depuis le serveur.

## Aucun serveur trouvé sur le réseau

*Chercher sur ce réseau* liste les serveurs qui répondent à une demande de découverte sur le port UDP 47115 (comme les serveurs Lantern). Rien trouvé :

- le serveur est éteint, ou sur un autre réseau que la console (un Wi-Fi invité, un autre sous-réseau) ;
- un pare-feu sur l'ordinateur du serveur bloque le port UDP 47115 ;
- le serveur ne répond pas à la découverte : ajoutez-le à la main avec *Ajouter un serveur* et son adresse `https://`.

## Un téléchargement échoue

Le paquet dit pourquoi, et quoi faire : voir [Quand un téléchargement s'arrête](downloads.md#quand-un-téléchargement-sarrête).

## Un jeu installé ne démarre pas

- sys-patch ou les sigpatches manquent, ou sont plus anciens que le firmware de la console : mettez-les à jour.
- Une mise à jour ou un contenu additionnel demande un firmware plus récent que celui de la console.
- Le jeu de base manque : une mise à jour ou un contenu additionnel seul ne démarre pas. La fiche du jeu installe ce qui manque.

## Des noms ou des icônes manquent

Un jeu connu seulement par son nom de fichier prend son nom et son icône dans la base de titres : *Préférences › Base de titres › Télécharger*, puis **Mettre à jour** de temps en temps. Une source en échec le dit sous *Sources*, avec la raison. Une icône toujours absente vient de Nlib API quand la console est en ligne et que *Icônes et fiches de jeu depuis Nlib API* est activé ; un jeu qu'elle ne connaît pas n'est redemandé qu'au prochain lancement de PlayShelf.

## Envoyer un rapport

*À propos › Aide et diagnostic* :

- **Exporter un rapport de diagnostic** en enregistre un sur la carte SD, à joindre à un [signalement de bug](https://github.com/JigSawFr/PlayShelf/issues/new). Il ne contient jamais le numéro de série de la console.
- **Envoyer un rapport…** le met dans un gist secret de votre compte GitHub et affiche son lien avec un QR code.
- Sans réseau ni compte, le rapport peut sortir de la console en photos : en mode développeur, *Afficher le rapport de diagnostic*, puis **R** l'affiche en codes QR (quatre par page, le début du journal laissé de côté s'ils étaient trop nombreux). Photographiez chaque code, dans n'importe quel ordre, et lancez `python3 tools/decode_report.py photo1.jpg photo2.jpg …` (demande `pip install zxing-cpp pillow zstandard`) pour reconstituer le rapport ; joignez le fichier obtenu au signalement.

Un plantage est enregistré dans un fichier de plantage, que le démarrage suivant signale et propose d'envoyer.

Retour au [README](../../README.md).
