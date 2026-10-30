<!-- translated from docs/getting-started.md, sha256 c5463646f09e -->
# Prise en main

PlayShelf parcourt un serveur de bibliothèque que vous hébergez vous-même et en installe vos propres sauvegardes et homebrews : le jeu de base, ses mises à jour et ses contenus additionnels, en NSP, NSZ, XCI ou XCZ, en HTTPS. Cette page va de la carte SD au premier jeu installé.

> [!WARNING]
> **Développement en cours.** Aucune version n'est encore publiée. Le [simulateur web](https://shelf.boreale.dev/simulator/?utm_source=github&utm_medium=docs) montre où en est PlayShelf, face à un serveur de démonstration aux jeux inventés.

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

**Accueil**, en haut du rail, dit où en sont la console et ses serveurs. En haut, **Rechercher** cherche un jeu par son nom (ou son éditeur) sur tous les serveurs à la fois, puis parmi les jeux de la console qu'aucun serveur n'a. Des tuiles comptent les jeux de la console, les mises à jour et contenus additionnels que les serveurs ont et qu'elle n'a pas (Ⓐ sur les mises à jour : **Tout mettre à jour**, les jeux les plus joués d'abord, ou la liste), les téléchargements en cours et la place libre sur la carte SD (Ⓐ : **Stockage**, plus bas). Puis des rangées : **Joués récemment** (les jeux de la console par date de dernière partie, avec leur temps de jeu), les jeux à compléter (les plus joués d'abord) en grandes bannières, **Pour vous** et **Dans vos séries** (plus bas), **Votre liste de souhaits** (Ⓧ sur la page d'un jeu l'y ajoute ou l'en retire ; ceux qu'un serveur a passent devant), **Ajoutés sur vos serveurs** (les jeux qu'un serveur a depuis moins d'un mois, à partir de la deuxième lecture de son catalogue), **Populaires sur vos serveurs** et **Tendances** (plus bas), les sorties du mois (sinon les nouveautés des serveurs) et **Bientôt disponibles** (les jeux que les bases de titres datent des deux prochains mois ; Ⓐ en met un dans la liste de souhaits). Le temps de jeu est celui de la console (tous comptes confondus), lu comme PlayGuard le lit. Le catalogue de chaque serveur est lu dès le démarrage, l'un après l'autre ; l'Accueil du rail compte les jeux de la console qui ont quelque chose à récupérer. PlayShelf s'ouvre sur l'Accueil ; *Préférences › Démarrer sur* en choisit un autre.

**Populaires** et **Tendances** viennent de Wikipédia : le nombre de Wikipédias qui ont une page sur chaque jeu et de critiques qui l'ont testé (Wikidata), et combien sa page anglaise a été lue le mois dernier par rapport aux deux précédents. [Atlas](https://github.com/JigSawFr/Atlas) refait cette liste chaque semaine, avec la note des critiques de chaque jeu, que montrent les **Détails** de sa page. PlayShelf est livré avec elle et va chercher la plus récente une fois par jour, seulement les fichiers qui ont changé (*Préférences › Base de titres › Jeux populaires depuis Wikipédia*, [`popular_online`](../config.md)) ; seuls les jeux de vos serveurs s'affichent.

**Pour vous** pèse de nombreux signaux à la fois et dit, sous chaque jaquette, celui qui a le plus compté : les jeux qui ressemblent à ceux auxquels joue la console (*Comme …*, ou *35 h de …* pour un jeu joué 10 heures ou plus : Atlas compare les séries, développeurs, éditeurs, genres et modes de chaque jeu d'après Wikidata), un jeu comptant d'autant plus qu'il a été joué, et récemment ; les jeux d'une même série (*Série : …*) ou d'un même studio (*Par …*) ; les genres des jeux de la console ; la notoriété d'un jeu, la note des critiques et, pour un jeu aussi sur Steam, l'avis de ses joueurs (*Populaire*) ; et un petit plus pour un jeu sorti cette année. La rangée est variée : chaque jeu retenu pèse contre les suivants de sa série, de son studio ou qui lui ressemblent beaucoup, pour qu'une série ne la remplisse pas. Ⓧ sur une suggestion dit qu'elle ne vous intéresse pas : le jeu n'est plus suggéré, et ceux qui lui ressemblent beaucoup le sont moins (*Préférences › Jeux à ne plus suggérer* les suggère tous de nouveau). Ⓧ fait de même sur les jaquettes de Dans vos séries, Ajoutés sur vos serveurs et Tendances, et sur les bannières de Populaires sur vos serveurs ; un jeu écarté quitte aussi ces rangées. **Dans vos séries** liste les autres jeux des serveurs dans les séries dont la console a des jeux. La page d'un jeu montre ses **Autres éditions** présentes sur vos serveurs (une édition Deluxe, Complete ou GOTY, ou le jeu lui-même depuis l'une d'elles), les jeux qui lui ressemblent le plus sur vos serveurs (**Jeux similaires**), et ses **Détails** sa série, ses avis Steam et, dès que 20 joueurs l'ont noté sur IGDB, leur **Note des joueurs**. La ressemblance entre les jeux (*Comme …*, **Jeux similaires**) n'est pas livrée avec PlayShelf : elle est téléchargée avec le reste (quelques Mo), si bien que rien ne s'affiche à ces endroits avant sa première arrivée, ni jamais avec `popular_online` désactivé. Un jeu et ses éditions comptent pour un : aucun n'est suggéré quand la console en a un, et une rangée ne montre qu'une jaquette pour eux, celle du jeu quand la rangée l'a.

**Stockage** montre la place de la carte SD en barre (les jeux, le reste, le libre), puis **À libérer**, les gros jeux pas lancés depuis trois mois, et tous les jeux par la place qu'ils occupent, avec leur dernière partie. Ⓐ sur l'un : le supprimer (ses sauvegardes restent) ou ouvrir sa page.

**La cloche**, à droite de l'en-tête, compte ce qui est à faire ; Ⓨ (depuis un onglet sans Ⓨ à lui) ou un appui dessus ouvre la liste. **À faire** : les mises à jour et contenus additionnels à récupérer, un jeu de la liste de souhaits qu'un serveur a désormais, la carte SD presque pleine (moins d'un dixième ou de 4 Go libres), un téléchargement qui a échoué ; Ⓐ le fait ou ouvre l'écran où le faire. **Pour information** : un jeu de la liste de souhaits sorti le jour même (d'après ce qu'a changé le dernier passage quotidien d'Atlas, jusqu'au suivant), un jeu nouveau sur Switch d'une série dont vous avez des jeux ou en souhaitez, ou proche de l'un d'eux, un jeu de la liste de souhaits ou d'une série de vos jeux bien plus lu que d'habitude sur Wikipédia (du même passage, trois de chaque au plus, chaque jeu annoncé une fois), les jeux arrivés sur les serveurs, les mises à jour que les bases de titres connaissent et qu'aucun serveur n'a encore, les téléchargements installés, une nouvelle version de PlayShelf ; Ⓧ en range une (Ⓨ : toutes), jusqu'à ce que ce qu'elle dit change.

**Les autres homebrews Boréale** (PlayGuard…) ont chacun leur section en fin de liste, à leur nom, avec ce qu'ils ont à dire ; ce qui est à faire ou en échec chez eux est compté sur la cloche aussi, relu chaque minute. Ⓐ sur une entrée lance ce homebrew à l'écran qu'elle indique, une fois PlayShelf fermé (quand PlayShelf a été lancé depuis le menu des homebrews ; sinon un message invite à l'ouvrir depuis ce menu) ; rien de l'autre homebrew ne s'exécute dans PlayShelf. *Préférences › Les notifications des autres homebrews Boréale* les masque. En retour, les entrées de PlayShelf s'affichent dans leurs cloches, et en ouvrir une lance PlayShelf sur l'onglet où elle se règle.

*À propos › Homebrews Boréale* liste les homebrews de la famille Boréale installés sur la console, tels que chacun s'ajoute à son démarrage (`config/boreale/apps/`), PlayShelf en premier, chacun avec sa version ; Ⓐ sur un autre le lance une fois PlayShelf fermé (depuis le menu des homebrews, comme ci-dessus). Un homebrew supprimé de la carte SD quitte la liste.

Quand le même paquet est sur plusieurs serveurs lus, le téléchargement vient du meilleur : un serveur du réseau de la console d'abord, puis celui d'où les téléchargements précédents allaient le plus vite.

Le **Catalogue** montre les jeux du serveur en jaquettes, avec leur taille et leurs contenus additionnels, et une marque sur la jaquette d'un jeu que la console a (en ambre quand une mise à jour est à prendre). Ses réglages sont une rangée de pastilles au-dessus : le serveur, la recherche, l'ordre, les filtres et la section, en turquoise tant qu'ils restreignent la liste. **Rechercher** et **Ordre** (par nom, les plus récents, les plus gros, par éditeur, ou par genre en rayons : une rangée de jaquettes par genre, les jeux pas encore connus sur la dernière, **Tout voir** au bout d'une rangée pleine pour tout le genre) l'affinent ; un serveur organisé en sections les montre aussi. **Filtres** garde les jeux d'un genre, d'un éditeur, à un ou plusieurs joueurs, dans votre langue, selon ce que la console en a (absents, présents, une mise à jour ou des contenus additionnels à prendre), démos masquées. Les genres, éditeurs, joueurs et langues viennent de Nlib API à mesure que les jeux s'affichent : tant qu'un filtre en a besoin, les jeux pas encore connus sont demandés en arrière-plan et rejoignent la liste à leur arrivée (la ligne sous les filtres dit combien sont connus). La **Bibliothèque** a ses propres pastilles : combien de jeux (Ⓐ les relit) et **Ordre et filtres** : par nom, par éditeur, les plus récents, ceux à compléter d'abord (les plus joués d'entre eux en premier), les plus joués, ou les joués récemment ; ses filtres gardent aussi les jeux jamais lancés.

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

Les *Préférences* règlent la langue (celle de la console, ou l'une des 15), le thème et sa palette (celle de PlayShelf, une palette livrée ou [la vôtre](themes.md)), la taille du texte, l'onglet de démarrage, les mises à jour et la base de titres. *Même apparence que les autres homebrews Boréale* (activé par défaut) partage avec eux la langue, le thème, la taille du texte et les effets visuels (`config/boreale/prefs.conf`) : PlayShelf reprend au lancement ceux qu'un autre homebrew a choisis, et un changement fait ici gagne les autres à leur prochain lancement. Chaque réglage est aussi dans [`config.json`](../config.md).

![Les Préférences](../../images/screenshots/preferences.png)

Retour au [README](../../README.md).
