# Emoji icons

The emoji a section heading or a bullet of the release notes opens on
(`CHANGELOG.md`, `.github/release-notes/<version>.md`) is drawn from here on
the console, whose font has none (`source/view/release_notes.cpp`). A file is
named after the emoji's code point in lowercase hex, without the U+FE0F
variation selector: `1f680.png` for 🚀, `26a0.png` for ⚠️. An emoji that is
not here shows no icon.

The set is a palette, larger than what the notes use today on purpose: a new
note can open on any of these without a new file. `tools/check_resources.py`
leaves the unused ones alone, and warns about an emoji the notes open on that
has no file here.

The flags (two code points, joined by a dash as Twemoji names them:
`1f1eb-1f1f7.png` for 🇫🇷) are the language picker's
(`source/ui/dialogs.cpp`): 🇺🇸 🇫🇷 🇨🇦 🇩🇪 🇪🇸 🇲🇽 🇮🇹 🇳🇱 🇵🇹 🇧🇷 🇷🇺 🇯🇵 🇰🇷 🇨🇳 🇹🇼,
the console's own language a 🌐; the network clock's regions and servers take
the flag of their country (`ntp::host_emoji()`, `source/util/ntp_servers.cpp`:
🇬🇧 🇧🇪 🇨🇭 🇵🇱 🇸🇪 🇦🇷 🇭🇰 🇸🇬 🇮🇳 🇦🇺 🇳🇿 🇿🇦 🇦🇪 🇮🇱 besides the ones above),
🇪🇺, or the globe turned to their continent (🌎 🌏 🌍).

To add one, take its 72×72 PNG from the same Twemoji release:

    curl -fO https://cdn.jsdelivr.net/gh/jdecked/twemoji@15.1.0/assets/72x72/<hex>.png

Graphics: [Twemoji](https://github.com/jdecked/twemoji) 15.1.0, © Twitter, Inc
and other contributors, licensed under
[CC-BY 4.0](https://creativecommons.org/licenses/by/4.0/).

| | | | | | |
|---|---|---|---|---|---|
| 🚀 `1f680` | ⚠️ `26a0` | 🐞 `1f41e` | ✨ `2728` | 🧰 `1f9f0` | 🧪 `1f9ea` |
| 📚 `1f4da` | 🌟 `1f31f` | 📊 `1f4ca` | 🔒 `1f512` | 🕒 `1f552` | 📱 `1f4f1` |
| 🌍 `1f30d` | 🛡️ `1f6e1` | ⏱️ `23f1` | ⏳ `23f3` | ⏰ `23f0` | 🎮 `1f3ae` |
| 👪 `1f46a` | 🔄 `1f504` | 🛟 `1f6df` | 🆘 `1f198` | 💾 `1f4be` | 🐛 `1f41b` |
| 🔧 `1f527` | ✅ `2705` | 🎉 `1f389` | ⭐ `2b50` | 🧭 `1f9ed` | 🔔 `1f514` |
| ⚙️ `2699` | 🗓️ `1f5d3` | 📈 `1f4c8` | 🧒 `1f9d2` | 🔑 `1f511` | 📦 `1f4e6` |
| 🌙 `1f319` | 🎨 `1f3a8` | 💡 `1f4a1` | 📝 `1f4dd` | 🚨 `1f6a8` | 🛠️ `1f6e0` |
| 🔐 `1f510` | 🌐 `1f310` | 📤 `1f4e4` | ❤️ `2764` | 🚫 `1f6ab` | ⚡ `26a1` |
| 🧹 `1f9f9` | 🗑️ `1f5d1` | 📋 `1f4cb` | 👀 `1f440` | 🎯 `1f3af` | 🆕 `1f195` |
| 🔍 `1f50d` | | | | | |
