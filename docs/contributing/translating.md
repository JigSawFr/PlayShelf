# Translating PlayShelf

Part of the [contributor's guide](../../CONTRIBUTING.md).

PlayShelf is translated into every language the console offers: English, French (France and Canada), German, Spanish (Spain and Latin America), Italian, Dutch, Portuguese (Portugal and Brazil), Russian, Japanese, Korean and Chinese (simplified and traditional). It follows the console's language unless *Preferences › Language* picks another one. British English consoles get the English strings (`CMakeLists.txt` copies them under `en-GB`). Fixes from native speakers are welcome.

To add a language:

1. Copy `resources/i18n/en-US/playshelf.json` to `resources/i18n/<code>/playshelf.json` (`<code>` as the console names its language, see `brls::LOCALE_*`) and translate the values. Keep every key, and as many `{}` placeholders as the English text, in the same order.
2. The language picker's list, the language's own name and the look's own words are [Aurore](https://github.com/JigSawFr/Aurore)'s: a new language goes there first (`ui::LANGUAGES` in `library/include/borealis/cadran/ui/prefs.hpp`, its `resources/i18n/<code>/cadran.json` and `hints.json`, and its name under `"tools" › "languages"` in every `cadran.json`), then the submodule is bumped here. A regional variant (`fr-CA`, `es-419`, `pt-BR`) is a full catalog of its own: borealis falls back to English, not to the base language.
3. Run `python3 tools/check_resources.py .`: it reports missing or extra keys, placeholder mismatches, unused keys and a language missing from the picker. CI runs the same check.
