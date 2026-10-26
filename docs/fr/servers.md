<!-- translated from docs/servers.md, sha256 109cd91db828 -->
# Serveurs de bibliothèque

PlayShelf installe depuis un serveur de bibliothèque que vous hébergez vous-même, sur un ordinateur ou un NAS : il lit le catalogue du serveur, puis y télécharge les paquets. Jusqu'à 16 serveurs peuvent être gardés, chacun avec son adresse et son compte.

## Ce que lit PlayShelf

- **AeroFoil et Ownfoil**, par leur API (`/api/remote`, sinon `/api/shop`) : les sections du serveur, ses jeux avec leur taille, leur version et leur icône, et son message du jour.
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

*Bibliothèque › Ajouter un serveur*, ou dans le *Catalogue*, la ligne **Serveur** :

- **Nom** : tel que le Catalogue l'affiche.
- **Sur ce réseau** et **Par Internet** : les adresses `https://` du serveur, l'une des deux au moins. Seulement `https://` : le catalogue et les paquets doivent arriver par une connexion vérifiée.
- **Adresse utilisée** : **Automatique** prend l'adresse du réseau quand le serveur y répond, sinon celle d'Internet ; ou toujours l'une des deux.
- **Nom d'utilisateur** et **Mot de passe**, quand le serveur les demande (HTTP Basic, en HTTPS uniquement). Le mot de passe est scellé pour cette console (chiffré et authentifié avec des clés dérivées de celle de la console), si bien qu'une copie de la carte SD n'ouvre rien ailleurs.

**Chercher sur ce réseau** envoie une demande de découverte sur le réseau de la console (port UDP 8465, auquel AeroFoil et Ownfoil répondent) et liste les serveurs qui répondent, pour en ajouter un avec son adresse déjà remplie.

## Certificats

Chaque connexion est en HTTPS avec son certificat vérifié ; aucun réglage ne désactive la vérification.

- Un serveur avec un certificat d'une autorité connue (Let's Encrypt, par exemple) est approuvé tel quel.
- Un serveur **de votre réseau** avec son propre certificat : la première connexion affiche l'empreinte de sa clé. Comparez-la avec celle qu'affiche le serveur ; **Faire confiance** la garde, et le serveur n'est plus approuvé qu'avec cette clé (épinglée).
- Réinstallé, ou doté d'un nouveau certificat, le serveur montre une autre clé : PlayShelf la refuse. Oubliez l'ancienne clé dans le profil du serveur (**Clé approuvée**, Ⓐ), puis approuvez la nouvelle.

## Où les serveurs sont gardés

`sd:/config/playshelf/servers.json`, leurs mots de passe scellés dedans. Supprimer un serveur supprime son mot de passe et sa clé approuvée.

Retour au [README](../../README.md).
