// libFuzzer target: source/util/markdown.cpp and safe_text.cpp, on any
// bytes (a pull request's description, a release's notes, as GitHub sends
// them). Beyond the sanitizers: the bounds markdown.hpp promises (blocks,
// code lines, table rows and columns, nesting), every string of a block valid
// UTF-8, and safe_text::clean() within its size, valid UTF-8 and stable when
// run again.
#include <string>

#include "fuzz.h"
#include "util/markdown.hpp"
#include "util/safe_text.hpp"

static bool ok(const std::string& s) { return fuzz_utf8_ok(s.data(), s.size()); }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string in(reinterpret_cast<const char*>(data), size);

    for (bool multiline : { true, false }) {
        const std::string c = safe_text::clean(in, 4096, multiline);
        FUZZ_ASSERT(c.size() <= 4096);
        FUZZ_ASSERT(ok(c));
        FUZZ_ASSERT(multiline || c.find('\n') == std::string::npos);
        FUZZ_ASSERT(safe_text::clean(c, 4096, multiline) == c);
    }
    safe_text::is_version(in);

    using markdown::Block;
    const std::vector<Block> blocks = markdown::parse(in);
    FUZZ_ASSERT(blocks.size() <= markdown::MAX_BLOCKS);
    for (const Block& b : blocks) {
        FUZZ_ASSERT(ok(b.text) && ok(b.alert) && ok(b.icon));
        FUZZ_ASSERT(b.icon.size() <= 8);
        if (b.kind == Block::Heading) FUZZ_ASSERT(b.level >= 1 && b.level <= 6);
        if (b.kind == Block::Item) FUZZ_ASSERT(b.level >= 0 && b.level <= markdown::MAX_DEPTH);
        FUZZ_ASSERT(b.task >= -1 && b.task <= 1);
        // MAX_CODE_LINES lines, then one "…" saying the rest was cut.
        FUZZ_ASSERT(b.lines.size() <= markdown::MAX_CODE_LINES + 1);
        for (const std::string& l : b.lines) FUZZ_ASSERT(ok(l));
        // The header row, then MAX_ROWS rows.
        FUZZ_ASSERT(b.rows.size() <= markdown::MAX_ROWS + 1);
        for (const auto& row : b.rows) {
            FUZZ_ASSERT(row.size() <= markdown::MAX_COLUMNS);
            for (const std::string& cell : row) FUZZ_ASSERT(ok(cell));
        }
    }
    // One line's inline Markdown (a title), on bytes that may not be clean.
    const std::string line = markdown::inline_text(in.substr(0, 1024));
    FUZZ_ASSERT(ok(markdown::inline_text(safe_text::clean(in.substr(0, 1024), 1024, false))));
    (void)line;
    return 0;
}
