// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/report_qr_flow.hpp"

#include <ctime>
#include <memory>
#include <vector>

#include <aurore.hpp>
#include <aurore/cadran/ui/cadran_kit.hpp>

#include "ui/ui.hpp"
#include "util/report_qr.hpp"

using namespace aurore::literals;

namespace
{
constexpr size_t PER_PAGE = 4;   // two by two
// The end of the report is the last lines logged (util/diagnostics): that part
// is cut first when the codes would be too many.
const char* const TAIL_MARK = "\n=== last lines logged";

using Pieces = std::shared_ptr<const std::vector<std::string>>;

void show(const Pieces& pieces, size_t page)
{
    const size_t pages = (pieces->size() + PER_PAGE - 1) / PER_PAGE;
    auto* grid = new aurore::Box(aurore::Axis::COLUMN);
    grid->setAlignItems(aurore::AlignItems::CENTER);
    for (size_t r = 0; r < 2; r++) {
        auto* row = new aurore::Box(aurore::Axis::ROW);
        row->setJustifyContent(aurore::JustifyContent::SPACE_EVENLY);
        for (size_t c = 0; c < 2; c++) {
            const size_t i = page * PER_PAGE + r * 2 + c;
            if (i >= pieces->size()) break;
            row->addView(cadran::kit::qr((*pieces)[i], 150, std::to_string(i + 1) + " / " + std::to_string(pieces->size()), "", 200));
        }
        grid->addView(row);
    }
    auto* note = new aurore::Label();
    note->setText("playshelf/dev/qr_note"_i18n);
    note->setFontSize(16);
    note->setSingleLine(false);
    grid->addView(note);

    auto* dialog = ui::dialog(aurore::getStr("playshelf/dev/qr_title", page + 1, pages), grid);
    if (page > 0)
        dialog->addButton("playshelf/dev/qr_prev"_i18n, [pieces, page]() { aurore::sync([pieces, page]() { show(pieces, page - 1); }); });
    if (page + 1 < pages)
        dialog->addButton("playshelf/dev/qr_next"_i18n, [pieces, page]() { aurore::sync([pieces, page]() { show(pieces, page + 1); }); });
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}
}   // namespace

namespace report_qr_flow
{
void open(const std::string& report)
{
    const size_t at = report.find(TAIL_MARK);
    const std::string head = at == std::string::npos ? report : report.substr(0, at);
    const std::string tail = at == std::string::npos ? std::string() : report.substr(at);
    const auto id = (uint32_t)std::time(nullptr);
    const report_qr::Encoded enc = report_qr::encode(head, tail, id);
    if (enc.pieces.empty()) {
        ui::error("playshelf/dev/qr_failed_title"_i18n, "playshelf/dev/qr_failed"_i18n);
        return;
    }
    if (enc.trimmed) ui::notify("playshelf/dev/qr_trimmed"_i18n);
    show(std::make_shared<const std::vector<std::string>>(enc.pieces), 0);
}
}   // namespace report_qr_flow
