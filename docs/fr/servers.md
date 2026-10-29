<!-- translated from docs/servers.md, sha256 890af56549e4 -->
# Serveurs de bibliothèque

PlayShelf installe depuis un serveur de bibliothèque que vous hébergez vous-même, sur un ordinateur ou un NAS : il lit le catalogue du serveur, puis y télécharge les paquets. Jusqu'à 16 serveurs peuvent être gardés, chacun avec son adresse et son compte.

## Ce que lit PlayShelf

- **AeroFoil**, et Ownfoil avant sa version 2, par leur API (`/api/remote`, sinon `/api/shop`) : les sections du serveur, ses jeux avec leur taille, leur version et leur icône, et son message du jour. PlayShelf demande le catalogue entier : sans cela, AeroFoil arrête sa section *All* à quelques centaines de jeux. Un compte gelé par AeroFoil est signalé comme tel.
- **Ownfoil 2**, par son API GraphQL (`/api/graphql`, une page de jeux à la fois) et sa poignée de main (son nom et son message du jour).
- **Un index JSON** sur n'importe quel serveur web : un objet avec `files`, `directories`, `titledb` ou `success`.

Le *Catalogue*, dans le profil d'un serveur, est **Détecté** par défaut : PlayShelf essaie d'abord l'API, puis lit l'adresse comme un index. Choisissez celui que parle votre serveur pour éviter l'essai.

PlayShelf ne lit pas les index Tinfoil chiffrés, et n'envoie ni les en-têtes d'authentification de Tinfoil ni aucun identifiant de la console.

### Un index JSON

```json
{
  "success": "Welcome to my shelf",
  "files": [
    { "url": "games/Copper Canyon Rally [0100DE0000020000][v0].nsp", "size": 104857600 },
    "games/Copper Canyon Rally [0100DE0000020800][v65536].nsp"
  ],
  "directories": [ "dlc/index.json" ]
}
```

- `files` : chaque paquet, sous forme d'adresse ou d'objet avec `url` (ou `path`, `file`, `download_url`), et éventuellement `name`, `size`, `title_id`, `version`, `app_type` et `icon_url`. Une adresse est relative à celle de l'index.
- Sans `title_id`, l'identifiant du titre et la version viennent des balises du nom ou du nom de fichier : `[0100DE0000020000][v65536]`.
- `directories` : d'autres index, lus à leur tour.
- `titledb` : des noms pour les identifiants de titre, `{ "0100DE0000020000": { "name": "Copper Canyon Rally" } }`, pour les paquets connus seulement par leur nom de fichier.
- `success` : un message affiché au-dessus du catalogue.

## Ajouter un serveur

**Ajouter un serveur**, sur l'*Accueil* et dans le *Catalogue* tant qu'il n'y a pas encore de serveur, puis depuis la pastille **Serveur** du *Catalogue* :

- **Nom** : tel que le Catalogue l'affiche.
- **Sur ce réseau** et **Par Internet** : les adresses `https://` du serveur, l'une des deux au moins. Seulement `https://` : le catalogue et les paquets doivent arriver par une connexion vérifiée.
- **Adresse utilisée** : **Automatique** prend l'adresse du réseau quand le serveur y répond, sinon celle d'Internet ; ou toujours l'une des deux.
- **Nom d'utilisateur** et **Mot de passe**, quand le serveur les demande (HTTP Basic, en HTTPS uniquement). Le mot de passe est scellé pour cette console (chiffré et authentifié avec des clés dérivées de celle de la console), si bien qu'une copie de la carte SD n'ouvre rien ailleurs.

**Chercher sur ce réseau** envoie une demande de découverte sur le réseau de la console (port UDP 8465, auquel AeroFoil et Ownfoil répondent) et liste les serveurs qui répondent, pour en ajouter un avec son adresse déjà remplie.

## Ce qu'un serveur AeroFoil offre en plus

Une fois le catalogue d'un serveur AeroFoil lu, sa pastille **Serveur** dans le *Catalogue* a trois lignes de plus, et la fiche d'un jeu deux (dans le *Catalogue*, et dans la *Bibliothèque* pour un jeu de ce serveur) :

- **Cheats** : les cheats du serveur, par jeu et par build. **Installer** écrit le fichier d'une build là où Atmosphère le cherche, `sd:/atmosphere/contents/<title id>/cheats/<build id>.txt` ; **Installer toutes les builds** les écrit toutes, et Atmosphère charge celle qui correspond à la version du jeu. Les codes d'une build peuvent être affichés, et un fichier retiré. Le compte doit avoir la permission *Cheats* d'AeroFoil.
- **Sauvegardes** : les sauvegardes d'un utilisateur (choisi d'abord quand la console en a plusieurs) et leurs copies sur le serveur. **Envoyer une copie de cette sauvegarde** envoie la sauvegarde du jeu, avec une note pour la reconnaître ; **Remettre sur cette console** remplace la sauvegarde par une copie. La sauvegarde remplacée est d'abord gardée sur la carte SD, dans `sd:/config/playshelf/saves/<title id>/`, et remise en place si quelque chose échoue. Un jeu doit avoir été lancé une fois sur la console pour avoir une sauvegarde où en remettre une. Le compte doit avoir la permission *Backup* d'AeroFoil.
- **Demandes** : les jeux que vous avez demandés au serveur, et où en est chacun ; **Demander un jeu** en trouve un par son nom dans les bases de titres, ou par son title id.

La liste des cheats et les demandes sont des pages qu'AeroFoil sert à un navigateur connecté : PlayShelf s'y connecte avec le nom d'utilisateur et le mot de passe du serveur, par la même connexion vérifiée.

## Certificats

Chaque connexion est en HTTPS avec son certificat vérifié ; aucun réglage ne désactive la vérification.

- Un serveur avec un certificat d'une autorité connue (Let's Encrypt, par exemple) est approuvé tel quel.
- Un serveur **de votre réseau** avec son propre certificat : la première connexion affiche l'empreinte de sa clé. Comparez-la avec celle qu'affiche le serveur ; **Faire confiance** la garde, et le serveur n'est plus approuvé qu'avec cette clé (épinglée).
- Réinstallé, ou doté d'un nouveau certificat, le serveur montre une autre clé : PlayShelf la refuse. Oubliez l'ancienne clé dans le profil du serveur (**Clé approuvée**, Ⓐ), puis approuvez la nouvelle.

## Où les serveurs sont gardés

`sd:/config/playshelf/servers.json`, leurs mots de passe scellés dedans. Supprimer un serveur supprime son mot de passe et sa clé approuvée.

Retour au [README](../../README.md).
