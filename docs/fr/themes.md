<!-- translated from docs/themes.md, sha256 882270661001 -->
# Thèmes

PlayShelf prend toutes ses couleurs dans son thème : le thème clair ou sombre, puis une **palette** par-dessus. La palette peut être celle de PlayShelf, l'une de celles qui l'accompagnent, ou un fichier de thème à vous. Un changement se voit tout de suite, sur tous les écrans, sans redémarrer.

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

- Vos propres fichiers de thème (plus bas), par leur nom.

La même liste termine *Préférences › Thème* et le bouton de thème de l'en-tête (ou **−**, puis *Thème*). Une palette faite pour un seul thème (Océan, Sable, les contrastes élevés) fait aussi passer PlayShelf à ce thème ; une palette faite pour les deux suit *Préférences › Thème*. Choisir ensuite l'autre thème change aussi la palette : pour son autre moitié quand elle en a une (Contraste élevé clair et Contraste élevé sombre), sinon pour Par défaut ; le choix du thème l'annonce à l'avance. Le choix est gardé dans [`config.json`](../config.md) (`theme_file`).

## Vos propres fichiers de thème

Un fichier de thème est un petit fichier texte dont le nom finit par `.theme`. Placez-le dans le dossier `themes` du dossier de données de PlayShelf :

- sur la console : `sd:/config/playshelf/themes/` (créez le dossier : PlayShelf ne le fait pas) ;
- dans le simulateur de bureau : `playshelf_data/themes/`, à côté de `config.json`.

Rouvrez ensuite *Préférences › Palette* : le fichier y figure sous son `name`. Un fichier que PlayShelf ne peut pas lire du tout est laissé de côté ; une ligne illisible est sautée. Dans les deux cas, ce qui n'allait pas (le fichier et la ligne) est écrit dans le journal de la session (`logs/sessions/` du dossier de données), avec un avertissement quand un texte serait difficile à lire sur son fond.

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
- `background_image = image.png` met une image, à côté du fichier, derrière tous les écrans.
- `@include autre.inc` lit à cet endroit les lignes d'un autre fichier du dossier (nommez-le `.inc` pour qu'il ne soit pas listé).
- Les lignes qui commencent par `#` sont des commentaires ; une ligne plus bas l'emporte sur une ligne plus haut.

### Les couleurs propres à PlayShelf

En plus des clés d'Aurore, un fichier peut régler les couleurs du cadran de l'écran de chargement :

| Clé | Quoi |
|---|---|
| `playshelf/dial/line` | Ses cercles et ses minutes, tracés en léger |
| `playshelf/dial/track` | La piste du jour |
| `playshelf/dial/teal` | Son halo et l'arc du jour, en léger |

L'écran de chargement prend sinon le fond et le texte du thème. Le logo de PlayShelf garde ses couleurs.

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

## Si quelque chose ne va pas

- La palette n'est pas dans la liste : vérifiez que le nom du fichier finit par `.theme`, qu'il est dans `config/playshelf/themes/` sur la carte SD, et lisez dans le journal de la session ce que PlayShelf n'a pas pu lire.
- Un texte est difficile à lire : le journal dit quelle couleur, sur quel fond, et de combien ; changez l'une des deux.
- Pour revenir aux couleurs de PlayShelf : *Préférences › Palette › Par défaut*.
