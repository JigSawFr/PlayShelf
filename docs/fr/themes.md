<!-- translated from docs/themes.md, sha256 7f1059e5a003 -->
# Thèmes

PlayShelf prend toutes ses couleurs dans son thème : le thème clair ou sombre, puis une **palette** par-dessus. La palette peut être celle de PlayShelf, l'une de celles qui l'accompagnent, ou un thème à vous : un fichier de couleurs, ou un dossier qui apporte aussi des images (un fond, un logo, l'image d'un jeu sans jaquette, des décorations). Un changement se voit tout de suite, sur tous les écrans, sans redémarrer.

## Choisir une palette

*Préférences › Palette* propose :

- **Par défaut** : les couleurs de PlayShelf, bleu nuit ou blanc froid.
- Les palettes livrées avec PlayShelf :

  | Palette | Thème | Ce qu'elle donne |
  |---|---|---|
  | Contraste élevé sombre | sombre | Blanc sur noir, un cadre de sélection jaune, plus épais |
  | Contraste élevé clair | clair | Noir sur blanc, un cadre de sélection bleu profond, plus épais |
  | Océan | sombre | Bleu profond, un accent bleu ciel |
  | Améthyste | clair et sombre | Violet, selon le thème choisi |
  | Sable | clair | Papier chaud, un accent terre cuite |
  | Hiver, Saint-Valentin, Nouvel An lunaire, Printemps, Été, Automne, Halloween, Noël, Nouvel An | clair et sombre | Une par saison et par fête : givre et argent, rose, laque rouge et or, fleurs de cerisier, mer et coucher de soleil, érable, citrouille sur une nuit violette, sapin et or, minuit et champagne |
  | Au fil des saisons | clair et sombre | Celle du jour, selon le calendrier : Halloween à partir du 17 octobre, Noël en décembre, et ainsi de suite. Elle change d'elle-même : le jour où commence une saison ou une fête, les écrans prennent sa palette à minuit, ou dès que la console sort de veille. Les saisons sont celles de l'hémisphère nord ; le [guide des thèmes d'Aurore](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#the-seasons) (en anglais) donne les jours exacts |

- Vos propres thèmes (plus bas), par leur nom.

Chaque ligne montre un échantillon des couleurs de la palette, son fond avec sa surface en anneau et son motif dans son accent (un flocon pour Hiver, une citrouille pour Halloween, une vague pour Océan…), ou la vignette du thème quand il en a une.

Les saisons et les fêtes ont aussi quelques particules qui dérivent doucement derrière les écrans : de la neige pour Hiver et Noël, des pétales pour Printemps, des feuilles pour Automne, des braises pour Halloween, des confettis pour Nouvel An, des cœurs pour Saint-Valentin, des étincelles dorées pour Nouvel An lunaire (aucune en Été). Elles ne bougent que si les mouvements sont permis : avec *Préférences › Effets visuels* sur *Mouvements réduits*, ou sur *Automatique* avec la console en main, la palette s'affiche sans elles. Après une minute sans qu'un bouton soit pressé, elles s'estompent et s'arrêtent, et reviennent à la pression suivante.

La même liste termine *Préférences › Thème* et le bouton de thème de l'en-tête (ou **−**, puis *Thème*). Une palette faite pour un seul thème (Océan, Sable, les contrastes élevés) fait aussi passer PlayShelf à ce thème ; une palette faite pour les deux suit *Préférences › Thème*. Choisir ensuite l'autre thème change aussi la palette : pour son autre moitié quand elle en a une (Contraste élevé clair et Contraste élevé sombre), sinon pour Par défaut ; le choix du thème l'annonce à l'avance. Le choix est gardé dans [`config.json`](../config.md) (`theme_file`).

## Vos propres thèmes

Un thème est un petit fichier texte dont le nom finit par `.theme`, ou, quand il a des images, un dossier qui contient son fichier, nommé `theme.theme`, et ses images ([plus bas](#dossiers-de-thème-et-images)). Placez-le dans le dossier `themes` du dossier de données de PlayShelf :

- sur la console : `sd:/config/playshelf/themes/` (créez le dossier : PlayShelf ne le fait pas) ;
- dans le simulateur de bureau : `playshelf_data/themes/`, à côté de `config.json`.

Rouvrez ensuite *Préférences › Palette* : le thème y figure sous son `name`. Un fichier que PlayShelf ne peut pas lire du tout est laissé de côté ; une ligne illisible est sautée. Dans les deux cas, ce qui n'allait pas (le fichier et la ligne) est écrit dans le journal de la session (`logs/sessions/` du dossier de données), avec un avertissement quand un texte serait difficile à lire sur son fond.

### Le plus court

Trois lignes : un nom, le thème de départ et une couleur d'accent. PlayShelf en déduit le reste (le cadre de sélection, les interrupteurs, les boutons, l'onglet choisi) en gardant le texte lisible.

```ini
name = Mint
inherit = dark
accent = #3ddc97
```

### Ce qu'un fichier peut dire

```ini
# Affiché dans la liste
name = Dusk
author = Vous

# Son point de départ : light, dark, l'une des palettes ci-dessus
# (builtin:ocean, builtin:sand, builtin:amethyst, builtin:high-contrast-dark,
# builtin:high-contrast-light) ou un autre fichier du dossier. Sombre sans lui.
inherit = dark

# Quelques couleurs, dont toutes les autres sont déduites
background = #1b1626
surface = #251e33
text = #f1ecf8
accent = #ff8fab
warning = #ffc861
danger = #ff6b6b
```

- Les couleurs s'écrivent `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb(r, g, b)` ou `rgba(r, g, b, a)` (`a` de 0 à 1).
- Les sections `[light]` et `[dark]` contiennent ce qui vaut pour un seul thème : avec les deux, le fichier suit *Préférences › Thème*.
- `[colors]` règle n'importe quelle couleur par sa clé (la liste est dans [le guide des thèmes d'Aurore](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#every-key), en anglais), comme `cadran/text3` pour le texte le plus discret ou `brls/sidebar/background` pour la barre latérale.
- `counterpart = autre.theme` (ou une palette, `builtin:ocean`), dans un fichier fait pour un seul thème : le fichier de l'autre thème, le même style en clair ou en sombre. Choisir l'autre thème passe à lui.
- `icon = moon` est le motif que montre sa ligne dans la liste des palettes : `snowflake`, `heart`, `lantern`, `blossom`, `sun`, `wave`, `leaf`, `pumpkin`, `tree`, `firework`, `gem`, `dune`, `contrast`, `moon`, `bolt`, `sparkle` (par défaut), ou n'importe quelle autre des [icônes d'Aurore](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#the-format) (en anglais).
- `background_image = image.jpg` met une image derrière tous les écrans ; les images et les clés qui s'en servent sont [plus bas](#dossiers-de-thème-et-images).
- `@include autre.inc` lit à cet endroit les lignes d'un autre fichier du dossier (nommez-le `.inc` pour qu'il ne soit pas listé).
- Les lignes qui commencent par `#` sont des commentaires ; une ligne plus bas l'emporte sur une ligne plus haut.

### Les couleurs propres à PlayShelf

En plus des clés d'Aurore, un fichier peut régler les couleurs du cadran de l'écran de chargement :

| Clé | Quoi |
|---|---|
| `playshelf/dial/line` | Ses cercles et ses minutes, tracés en léger |
| `playshelf/dial/track` | La piste du jour |
| `playshelf/dial/teal` | Son halo et l'arc du jour, en léger |

L'écran de chargement prend sinon le fond (ou l'image de fond) et le texte du thème. Le logo de PlayShelf garde ses couleurs, sauf si le thème apporte son propre logo (plus bas).

### Deux thèmes dans un fichier

Celui-ci est clair avec le thème clair et sombre avec le thème sombre :

```ini
name = Forest
author = Vous

[light]
background = #f1f4ee
surface = #ffffff
text = #1d2a1f
accent = #2f7d4a
playshelf/dial/line = #1d2a1f

[dark]
background = #121a14
surface = #1a251c
text = #e6efe7
accent = #6fcf8f
playshelf/dial/teal = #6fcf8f
```

## Dossiers de thème et images

Un thème avec des images est un dossier dans `themes/`, au nom du thème, qui contient son fichier `theme.theme` et ses images (PNG ou JPEG), à côté de lui ou dans des sous-dossiers :

```
config/playshelf/themes/
├── mint.theme          # un simple fichier
└── nightfall/          # un dossier de thème
    ├── theme.theme
    ├── night.jpg
    └── logo-dark.png
```

Une image se nomme par rapport au fichier du thème et doit rester dans le dossier du thème : un chemin absolu, un `..` qui en sort ou un lien vers un fichier extérieur est refusé, et la ligne est sautée (le journal de la session dit laquelle). Chaque clé ci-dessous s'écrit en haut, pour les deux thèmes, ou dans `[light]` ou `[dark]` pour un seul ; `none` retire une image héritée. Sans ces clés, rien ne change.

**Le fond**

| Clé | Valeurs | Quoi |
|---|---|---|
| `background_image` | une image, ou `none` | Derrière tous les écrans, écran de chargement compris |
| `background_fit` | `cover` (par défaut), `contain`, `tile`, `center` | `cover` remplit l'écran, coupée sur ses bords ; `contain` la montre entière ; `tile` la répète ; `center` la pose au milieu telle quelle |
| `background_dim` | 0 à 1 (0,35 par défaut) | Un voile de la couleur de fond du thème sur l'image, pour que le texte se lise : 0 montre l'image telle quelle |
| `surface_opacity` | 0 à 1 (1 par défaut) | L'opacité des cartes, tuiles, pastilles, de la barre latérale et des volets sur l'image : sous 1, l'image transparaît un peu |

Une image chargée ou claire demande un voile plus fort (0,5 à 0,7). Le journal de la session prévient quand le texte se lirait mal sur l'image sous son voile.

**Le logo, les icônes et les images de PlayShelf**

| Clé | Quoi |
|---|---|
| `logo` | La marque de PlayShelf dans l'en-tête et dans À propos, et son icône sur l'écran de chargement, ajustées à leur carré |
| `icon.<nom>` | L'une des icônes des écrans (`settings`, `info`, `gamepad`… : [les noms](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#logos-and-images), en anglais) ; `icon.<nom>.tint = true` dessine une forme blanche (ou noire) dans la couleur du moment de l'icône |
| `image.playshelf/cover/placeholder` | L'image d'un jeu sans jaquette ([les images propres à PlayShelf](#les-images-propres-à-playshelf)) |
| `preview` | La vignette du thème dans la liste des palettes, à la place de l'échantillon de ses couleurs : une image carrée, de 72 × 72 au moins |

**Les décorations**

| Clé | Quoi |
|---|---|
| `backdrop.header`, `backdrop.sidebar`, `backdrop.dialog` | Une image sur l'en-tête, la barre latérale, ou le panneau des dialogues et des volets, sous leur texte : gardez-la discrète |
| `decor.top`, `decor.bottom` | Une bande le long du bord haut ou bas de l'écran, derrière les écrans (des stalactites, une guirlande, une congère) : un PNG avec transparence, dans les 30 premières unités de l'en-tête ou les 20 dernières du pied |
| `<clé>_fit`, `<clé>_opacity`, `decor.top_height`, `decor.bottom_height` | Comment chacune remplit sa zone (comme `background_fit`), son opacité, la hauteur d'une bande |
| `particles` | `snow`, `leaves`, `petals`, `embers`, `confetti`, `sparkles`, `hearts`, `bubbles`, ou `none` |
| `particles_density` | `low` (par défaut), `medium`, `high`, ou de 0 à 1 |
| `particles_color` | Jusqu'à cinq couleurs, séparées par des virgules (tirées de la palette par défaut) |
| `particles_speed`, `particles_layer` | De 0,25 à 4 fois plus vite ; `back` (par défaut : derrière les écrans, entre les cartes) ou `front` (par-dessus, plus pâles) |

Les particules suivent les mêmes règles que celles des palettes de saison (plus haut) : elles ne bougent que si les mouvements sont permis, jamais avec *Mouvements réduits* ni en main sur *Automatique*, et s'arrêtent après une minute sans qu'un bouton soit pressé. Le [guide des thèmes d'Aurore](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#theme-folders-and-pictures) (en anglais) a tous les détails.

### Les images propres à PlayShelf

Un dossier de thème (plus haut) peut remplacer ces images de PlayShelf (`image.<clé> = fichier.png`, dans le dossier du thème, en PNG ou JPEG) :

| Clé | Quoi | Sans elle |
|---|---|---|
| `playshelf/cover/placeholder` | Ce que montre un jeu sans jaquette, dans la Bibliothèque, le Catalogue, l'onglet Accueil, les Téléchargements et sur sa page : l'image remplit le carré de la jaquette (coupée sur ses bords, centrée, ses coins arrondis) ; ramenée à 512 × 512 au plus | Une manette sur le creux de la jaquette |

Le `logo` du thème prend la place de la marque de PlayShelf dans l'en-tête et dans À propos, et de son icône sur l'écran de chargement (sur le même halo).

### Un exemple : Nightfall

Un dossier de thème complet, un ciel au clair de lune la nuit et à l'aube, est dans le dépôt de PlayShelf : [`docs/examples/themes/nightfall`](https://github.com/JigSawFr/PlayShelf/tree/main/docs/examples/themes/nightfall). Ses images sont dessinées par `tools/make_example_theme.py`, quelques dizaines de Ko chacune :

```
nightfall/
├── theme.theme
├── night.jpg, day.jpg             # le fond, la nuit et à l'aube
├── logo-dark.png, logo-light.png  # le logo, 256 × 256
├── cover.png                      # un jeu sans jaquette, 512 × 512
├── preview.png                    # sa vignette dans la liste des palettes, 128 × 128
└── stars-night.png, stars-day.png # le motif discret des dialogues, répété
```

```ini
# Nightfall: the example theme folder of docs/themes.md. Its pictures are
# drawn by tools/make_example_theme.py. Copy the whole folder into
# config/playshelf/themes/ on the SD card.
name = Nightfall
author = PlayShelf
version = 1.0
description = A moonlit sky behind every screen, at night or at dawn.
icon = moon

# The picker's thumbnail, and the picture of a game without a cover
preview = preview.png
image.playshelf/cover/placeholder = cover.png

# Behind every screen: the picture fills the window, the cards let it show a little
background_fit = cover
surface_opacity = 0.88

# A faint pattern of stars on the dialogs, and a few sparkles drifting
# (never with Preferences › Visual effects at Reduced motion)
backdrop.dialog_fit = tile
backdrop.dialog_opacity = 0.45
particles = sparkles
particles_density = low

[light]
background = #f4f0fa
surface = #ffffff
text = #241f38
accent = #5c4cbe
background_image = day.jpg
background_dim = 0.5
logo = logo-light.png
backdrop.dialog = stars-day.png

[dark]
background = #0e1430
surface = #182042
text = #eef0ff
accent = #8e9cff
background_image = night.jpg
background_dim = 0.3
logo = logo-dark.png
backdrop.dialog = stars-night.png
```

Pour l'installer : copiez tout le dossier `nightfall` dans `config/playshelf/themes/` sur la carte SD (`sd:/config/playshelf/themes/nightfall/theme.theme`), puis choisissez *Nightfall* dans *Préférences › Palette*. Il a les deux thèmes : *Préférences › Thème* choisit la nuit ou l'aube. Pour partir de lui, copiez-le sous un autre nom de dossier, changez son `name` et remplacez les images.

## Si quelque chose ne va pas

- La palette n'est pas dans la liste : vérifiez que le nom du fichier finit par `.theme`, qu'il est dans `config/playshelf/themes/` sur la carte SD, et lisez dans le journal de la session ce que PlayShelf n'a pas pu lire.
- Un dossier de thème n'est pas dans la liste : son fichier doit s'appeler `theme.theme`, directement dans le dossier ; une image qu'il nomme et qui n'est pas là l'écarte aussi (le journal dit laquelle).
- Une image ne s'affiche pas : elle doit être en PNG ou en JPEG (pas en WebP), dans le dossier du thème, nommée comme elle est écrite (majuscules comprises).
- Pas de particules : elles ne bougent qu'avec *Préférences › Effets visuels* sur *Toujours complets*, ou sur *Automatique* sur la TV, et s'arrêtent après une minute sans qu'un bouton soit pressé.
- Un texte est difficile à lire : le journal dit quelle couleur, sur quel fond, et de combien ; changez l'une des deux.
- Pour revenir aux couleurs de PlayShelf : *Préférences › Palette › Par défaut*.
