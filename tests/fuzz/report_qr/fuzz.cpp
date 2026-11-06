// libFuzzer target: util/report_qr.cpp's Reader on any text, as a photo of a
// QR code could give it (a piece per line). Beyond the sanitizers: what the
// reader says is complete unpacks to at most the size allowed, and a piece
// the encoder wrote is always taken back.
#include <string>

#include "fuzz.h"
#include "util/report_qr.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string text(reinterpret_cast<const char*>(data), size);
    report_qr::Reader reader;
    size_t            from = 0;
    while (from <= text.size()) {
        const size_t nl = text.find('\n', from);
        const std::string line = text.substr(from, nl == std::string::npos ? std::string::npos : nl - from);
        reader.add(line);
        if (nl == std::string::npos) break;
        from = nl + 1;
    }
    FUZZ_ASSERT(reader.have() <= reader.total() || reader.total() == 0);
    if (const auto out = reader.text()) FUZZ_ASSERT(reader.complete() && out->size() <= (4u << 20));

    // What the encoder writes, the reader takes back, whatever the report.
    const size_t cut = size / 2;
    report_qr::Options opt;
    opt.chunk_bytes = 64 + size % 200;
    opt.max_chunks = 20;
    const auto enc = report_qr::encode(text.substr(0, cut), text.substr(cut), (uint32_t)size, opt);
    report_qr::Reader back;
    for (const auto& p : enc.pieces) FUZZ_ASSERT(back.add(p) == report_qr::Reader::Add::Ok);
    if (!enc.pieces.empty()) {
        FUZZ_ASSERT(back.complete());
        const auto s = back.text();
        FUZZ_ASSERT(s && s->size() >= cut * (enc.trimmed ? 0 : 1));
        if (!enc.trimmed) FUZZ_ASSERT(*s == text);
    }
    return 0;
}
