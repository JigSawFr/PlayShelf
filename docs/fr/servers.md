<!-- translated from docs/servers.md, sha256 f21ddb3c4a08 -->
# Serveurs de bibliothèque

PlayShelf installe depuis un serveur de bibliothèque que vous faites tourner vous-même, sur un ordinateur ou un NAS, et qui parle [Lantern](https://lantern.boreale.dev) : il lit le catalogue du serveur, puis télécharge les paquets, chacun vérifié d'après la taille et le SHA-256 annoncés par le serveur. Jusqu'à 16 serveurs peuvent être gardés, chacun avec ses adresses.

Lantern est le seul protocole que parle PlayShelf. Un serveur qui en parle un autre (AeroFoil, Ownfoil, un index JSON, celui de Tinfoil) a besoin d'un pont qui le sert en Lantern : [lantern-bridge](https://github.com/JigSawFr/lantern-bridge).

PlayShelf n'envoie aucun identifiant de la console à un serveur.

## Ajouter un serveur

**Ajouter un serveur**, sur *Accueil* et dans le *Catalogue* tant qu'il n'y a pas de serveur, puis depuis la pastille **Serveur** du *Catalogue* :

- **Nom** : tel que le Catalogue l'affiche.
- **Sur ce réseau** et **Par Internet** : les adresses `https://` du serveur, une au moins. Seulement `https://` : le catalogue et les paquets doivent arriver par une connexion vérifiée.
- **Adresse utilisée** : **Automatique** prend l'adresse du réseau quand le serveur y répond, sinon celle d'Internet ; ou toujours l'une des deux.
- **Appairage** : si cette console est appairée avec le serveur. **Enregistrer** appaire tout de suite un nouveau serveur.

**Chercher sur ce réseau** envoie une demande de découverte sur le réseau de la console (port UDP 47115) et liste les serveurs Lantern qui répondent, pour en ajouter un avec son adresse remplie.

## L'appairage

Un serveur laisse entrer une console quand une personne a donné son accord côté serveur :

1. PlayShelf demande un code au serveur et l'affiche, avec un QR code.
2. Sur la page du serveur (l'adresse affichée), saisissez le code, ou scannez le QR code avec un téléphone, et connectez-vous pour laisser entrer cette console.
3. PlayShelf en est informé et garde ce que le serveur lui a donné : un jeton, lié à une clé que cette console a faite pour ce serveur seul. Chaque requête est signée avec cette clé : une copie du jeton seul n'ouvre rien.

Le jeton et la clé sont gardés dans `sd:/config/playshelf/lantern.json`, scellés pour cette console (chiffrés et authentifiés avec des clés dérivées de celle de la console) : une copie de la carte SD n'ouvre rien ailleurs. *Oublier l'appairage*, dans le profil du serveur, ou supprimer le serveur, les efface ; la liste des appareils du serveur reste à ranger de son côté.

## Ce qu'un serveur offre

Le catalogue, ce sont les sections du serveur et ses jeux, avec leurs tailles et leurs versions, et son message du jour. Un serveur offre plus ou moins, et PlayShelf montre ce qu'il offre. Une fois son catalogue lu, sa pastille **Serveur** dans le *Catalogue* a jusqu'à trois lignes de plus, et la fiche d'un jeu jusqu'à deux (dans le *Catalogue*, et dans la *Bibliothèque* pour un jeu de ce serveur) :

- **Cheats** : les cheats du serveur, par jeu et par build. **Installer** écrit le fichier d'une build là où Atmosphère le cherche, `sd:/atmosphere/contents/<title id>/cheats/<build id>.txt` ; **Installer toutes les builds** les écrit toutes, et Atmosphère charge celle qui correspond à la version du jeu. Les codes d'une build peuvent être affichés, et un fichier retiré.
- **Sauvegardes** : les sauvegardes d'un utilisateur (choisi d'abord quand la console en a plusieurs) et leurs copies sur le serveur. **Envoyer une copie de cette sauvegarde** envoie la sauvegarde du jeu, avec une note pour la reconnaître ; **Remettre sur cette console** remplace la sauvegarde par une copie. La sauvegarde remplacée est d'abord gardée sur la carte SD, dans `sd:/config/playshelf/saves/<title id>/`, et remise si quoi que ce soit se passe mal. Un jeu doit avoir été lancé une fois sur la console pour avoir une sauvegarde à remplacer.
- **Demandes** : les jeux que vous avez demandés au serveur et où en est chacun ; **Demander un jeu** en trouve un par son nom dans les bases de titres, ou par son title id.

Ce qu'une personne peut faire dépend des droits que le serveur a donnés à cette console à l'appairage.

## Téléchargements

Un paquet est d'abord écrit en entier sur la carte SD, puis installé depuis elle. Le client du SDK vérifie sa taille et son SHA-256 à mesure qu'il s'écrit, et un téléchargement interrompu reprend où il s'était arrêté (le fichier est d'abord haché de nouveau). Seul un fichier entier et vérifié est installé. La place qu'il prend est la taille du paquet, en plus de son installation : PlayShelf dit quand la carte en manque.

## Certificats

Toute connexion est en HTTPS avec son certificat vérifié ; aucun réglage ne désactive la vérification.

- Un serveur avec un certificat d'une autorité connue (Let's Encrypt, par exemple) est approuvé tel quel.
- Un serveur **sur votre réseau** avec un certificat à lui : l'appairage montre l'empreinte de sa clé, à côté du code. Comparez-la à celle que le serveur affiche ; une fois appairé, le serveur n'est approuvé qu'avec cette clé (épinglée).
- Réinstallé, ou doté d'un nouveau certificat, le serveur montre une autre clé : PlayShelf la refuse. Oubliez l'appairage dans le profil du serveur et appairez de nouveau, après avoir comparé la nouvelle clé.

## Où sont gardés les serveurs

`sd:/config/playshelf/servers.json` : le nom, les adresses et l'`uid` de chaque serveur. Ses identifiants sont dans `lantern.json`, par cet `uid`.

Retour au [README](../../README.md).
