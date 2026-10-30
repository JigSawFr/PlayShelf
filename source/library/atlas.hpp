// atlas — the schema of Atlas' Switch files PlayShelf reads
// (https://github.com/JigSawFr/Atlas: platforms/switch/popular.json, games.json,
// similar.json, editions.json, changes.json and
// index.json). Atlas adds fields under the same schema and publishes a new one beside the old
// while PlayShelf reads it: a file of another schema is refused, and the
// copy PlayShelf had (fetched before, or the one it came with) kept.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace library
{

constexpr int ATLAS_SCHEMA = 1;

}   // namespace library
