# Translating PlayShelf

Part of the [contributor's guide](../../CONTRIBUTING.md).

PlayShelf is translated into every language the console offers: English, French (France and Canada), German, Spanish (Spain and Latin America), Italian, Dutch, Portuguese (Portugal and Brazil), Russian, Japanese, Korean and Chinese (simplified and traditional). It follows the console's language unless *Preferences › Language* picks another one. British English consoles get the English strings (`CMakeLists.txt` copies them under `en-GB`). Fixes from native speakers are welcome.

To add a language:

1. Copy `resources/i18n/en-US/playshelf.json` to `resources/i18n/<code>/playshelf.json` (`<code>` as the console names its language, see `brls::LOCALE_*`) and translate the values. Keep every key, and as many `{}` placeholders as the English text, in the same order.
2. Add `<code>` to `config::LANGUAGES` in `source/util/config.hpp`.
3. Add the language's own name under `"tools" › "languages"` in **every** `playshelf.json`. A regional variant (`fr-CA`, `es-419`, `pt-BR`) is a full catalog of its own: borealis falls back to English, not to the base language.
4. Optionally add `resources/i18n/<code>/hints.json` for borealis' button hints.
5. Run `python3 tools/check_resources.py .`: it reports missing or extra keys, placeholder mismatches, unused keys and a language missing from the picker. CI runs the same check.
