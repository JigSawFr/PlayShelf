<!-- translated from docs/getting-started.md, sha256 77a392ad12b9 -->
# Prise en main

PlayShelf parcourt un serveur de bibliothèque que vous hébergez vous-même et en installe vos propres sauvegardes et homebrews : le jeu de base, ses mises à jour et ses contenus additionnels, en NSP, NSZ, XCI ou XCZ, en HTTPS. Cette page va de la carte SD au premier jeu installé.

> [!WARNING]
> **Développement en cours.** Aucune version n'est encore publiée. Le [simulateur web](https://jigsawfr.github.io/PlayShelf/simulator/) montre où en est PlayShelf, face à un serveur de démonstration aux jeux inventés.

## Ce qu'il faut

- Une Nintendo Switch avec [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere).
- [sys-patch](https://github.com/impeeza/sys-patch) ou des sigpatches, pour que les jeux installés démarrent. PlayShelf les cherche avant d'installer, et demande s'il faut continuer quand il ne trouve ni l'un ni les autres.
- Votre propre serveur de bibliothèque, accessible en HTTPS : [AeroFoil](https://github.com/luketanti/AeroFoil), Ownfoil, ou n'importe quel serveur web avec un index JSON de vos fichiers. Voir [Serveurs de bibliothèque](servers.md).

## Installer PlayShelf

1. Téléchargez `playshelf.zip` depuis la [dernière version](https://github.com/JigSawFr/PlayShelf/releases).
2. Extrayez-le à la racine de la carte SD : il met en place `switch/playshelf/playshelf.nro`.
3. Lancez PlayShelf depuis le menu homebrew. Lancé par-dessus un jeu (R maintenu en lançant un jeu), il dispose de toute la mémoire de la console ; depuis l'album, il tourne en applet, avec moins.

PlayShelf garde ses réglages et ses données dans `sd:/config/playshelf/` : ses serveurs, ses téléchargements, sa base de titres et ses rapports.

## Ajouter votre serveur

Le premier démarrage s'ouvre sur l'**Accueil**, qui vous souhaite la bienvenue : il n'y a pas encore de serveur.

- **Ajouter un serveur** : son nom, son adresse `https://` sur votre réseau, sur Internet ou les deux, et son nom d'utilisateur et son mot de passe s'il a un compte. Voir [Ajouter un serveur](servers.md#ajouter-un-serveur).
- **Chercher sur ce réseau** : liste les serveurs AeroFoil et Ownfoil qui répondent sur le réseau de la console, pour en choisir un.

## Installer un jeu

**Accueil**, en haut du rail, dit où en sont la console et ses serveurs : des tuiles comptent les jeux de la console, les mises à jour et contenus additionnels que les serveurs ont et qu'elle n'a pas, et les téléchargements en cours (Ⓐ sur l'une ouvre cette liste), puis **Joués récemment** (les jeux de la console par date de dernière partie, avec leur temps de jeu), les jeux à compléter (les plus joués d'abord) et les nouveautés des serveurs en grandes bannières, et **Pour vous** : des jeux de vos serveurs qui ressemblent à ceux de la console, d'après les genres de ses jeux, les plus joués comptant le plus. Le temps de jeu est celui de la console (tous comptes confondus), lu comme PlayGuard le lit. Le catalogue de chaque serveur y est lu, l'un après l'autre. PlayShelf s'ouvre dessus ; *Préférences › Démarrer sur* en choisit un autre.

Le **Catalogue** montre les jeux du serveur en jaquettes, avec leur taille et leurs contenus additionnels, et une marque sur la jaquette d'un jeu que la console a (en ambre quand une mise à jour est à prendre). Ses réglages sont une rangée de pastilles au-dessus : le serveur, la recherche, l'ordre, les filtres et la section, en turquoise tant qu'ils restreignent la liste. **Rechercher** et **Ordre** (par nom, les plus récents, les plus gros, par éditeur, ou par genre en rayons : une rangée de jaquettes par genre, les jeux pas encore connus sur la dernière, **Tout voir** au bout d'une rangée pleine pour tout le genre) l'affinent ; un serveur organisé en sections les montre aussi. **Filtres** garde les jeux d'un genre, d'un éditeur, à un ou plusieurs joueurs, dans votre langue, selon ce que la console en a (absents, présents, une mise à jour ou des contenus additionnels à prendre), démos masquées. Les genres, éditeurs, joueurs et langues viennent de Nlib API à mesure que les jeux s'affichent : tant qu'un filtre en a besoin, les jeux pas encore connus sont demandés en arrière-plan et rejoignent la liste à leur arrivée (la ligne sous les filtres dit combien sont connus). La **Bibliothèque** a ses propres pastilles : combien de jeux (Ⓐ les relit) et **Ordre et filtres** : par nom, par éditeur, les plus récents, ceux à compléter d'abord (les plus joués d'entre eux en premier), les plus joués, ou les joués récemment.

| Catalogue | La fiche d'un jeu |
|---|---|
| ![Le Catalogue](../../images/screenshots/catalogue.png) | ![La fiche d'un jeu](../../images/screenshots/game_sheet.png) |

Ⓐ sur un jeu ouvre sa fiche : **Tout installer**, ou son jeu de base, l'une de ses mises à jour ou l'un de ses contenus additionnels. Chacun part dans les **Téléchargements**. La fiche a des onglets (Ⓐ sur l'un d'eux, ou Ⓛ et Ⓡ) : **Aperçu**, **Mises à jour** (la version installée et la plus récente, la plus récente d'un coup, chaque mise à jour), **Contenus additionnels** (combien la console en a, les manquants d'un coup, chacun) et **Détails** (ses identifiants, ses tailles, son éditeur, son développeur, sa date de sortie, ses genres, ses joueurs, ses langues, sa classification, et sa page eShop en QR code pour un téléphone). Quand la console est en ligne, elle montre ce que [Nlib API](https://api.nlib.cc/nx/) sait du jeu dans la langue de PlayShelf : sa bannière en haut avec le nom du jeu par-dessus, son éditeur et son année, ses genres, ses joueurs et s'il est dans votre langue en pastilles, un résumé, ses captures d'écran (Ⓐ en affiche une en plein écran, ◀ ▶ les autres), sa description, puis d'autres jeux du catalogue du même éditeur et du même genre. Détails dit aussi combien de temps la console y a joué, combien de fois elle l'a lancé et quand elle y a joué pour la dernière fois. **Disponible sur** nomme les serveurs qui ont le jeu ; Détails dit ce que chacun en a (le jeu de base, combien de mises à jour et la plus récente, combien de contenus additionnels). La Bibliothèque ouvre la même fiche pour un jeu de la console.

## Suivre les téléchargements

Les **Téléchargements** installent les paquets l'un après l'autre, chacun avec sa progression, sa vitesse et le temps restant. Ⓐ sur un jeu ou un paquet propose de le mettre en pause, de le reprendre, de le réessayer ou de le retirer. Une fois installé, le jeu est dans la **Bibliothèque**. Voir [Téléchargements](downloads.md).

| Téléchargements | Bibliothèque |
|---|---|
| ![Les Téléchargements](../../images/screenshots/downloads.png) | ![La Bibliothèque](../../images/screenshots/library.png) |

## Noms et icônes

Un jeu que ni la console ni le serveur ne nomment prend son nom et son icône dans la base de titres, gardée sur la carte SD : *Préférences › Base de titres › Télécharger*. Ses sources sont CyberFoil-DB, NXMeta et Ghostland par défaut, et vous pouvez ajouter les vôtres. Voir [`title_sources`](../config.md).

Une icône qu'aucune d'elles n'a est demandée à [Nlib API](https://api.nlib.cc/nx/) par Internet, pour les seuls jeux à l'écran, puis gardée en cache. Jeux de base seulement : une mise à jour ou un contenu additionnel montre l'icône de son jeu. *Préférences › Base de titres › Icônes et fiches de jeu depuis Nlib API* la désactive ([`nlib_api`](../config.md)).

## Préférences

Les *Préférences* règlent la langue (celle de la console, ou l'une des 15), le thème et sa palette (celle de PlayShelf, une palette livrée ou [la vôtre](themes.md)), la taille du texte, l'onglet de démarrage, les mises à jour et la base de titres. Chaque réglage est aussi dans [`config.json`](../config.md).

![Les Préférences](../../images/screenshots/preferences.png)

Retour au [README](../../README.md).
