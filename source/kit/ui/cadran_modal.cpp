// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/cadran_modal.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <functional>
#include <memory>
#include <vector>
#include <yoga/Yoga.h>

#include "ui/cadran_kit.hpp"
#include "ui/cadran_text.hpp"
#include "ui/ui.hpp"
#include "view/cadran_digits.hpp"
#include "view/cadran_icon.hpp"

using namespace brls::literals;

namespace
{
// The pills' looks (theme and style keys, as borealis' own button styles):
// plain on the second surface with a hairline, the confirmation filled.
const brls::ButtonStyle PLAIN = {
    .shadowType              = brls::ShadowType::NONE,
    .hideHighlightBackground = true,
    .highlightPadding        = "",
    .borderThickness         = "cadran/hairline",
    .enabledBackgroundColor  = "cadran/surf2",
    .enabledLabelColor       = "cadran/text",
    .enabledBorderColor      = "cadran/line",
    .disabledBackgroundColor = "cadran/surf2",
    .disabledLabelColor      = "cadran/text3",
    .disabledBorderColor     = "cadran/line",
};
const brls::ButtonStyle PRIMARY = {
    .shadowType              = brls::ShadowType::NONE,
    .hideHighlightBackground = true,
    .highlightPadding        = "",
    .borderThickness         = "",
    .enabledBackgroundColor  = "cadran/teal_fill",
    .enabledLabelColor       = "cadran/navy",
    .enabledBorderColor      = "",
    .disabledBackgroundColor = "cadran/surf2",
    .disabledLabelColor      = "cadran/text3",
    .disabledBorderColor     = "",
};
const brls::ButtonStyle DANGER = {
    .shadowType              = brls::ShadowType::NONE,
    .hideHighlightBackground = true,
    .highlightPadding        = "",
    .borderThickness         = "",
    .enabledBackgroundColor  = "cadran/coral_fill",
    .enabledLabelColor       = "cadran/navy",
    .enabledBorderColor      = "",
    .disabledBackgroundColor = "cadran/surf2",
    .disabledLabelColor      = "cadran/text3",
    .disabledBorderColor     = "",
};

constexpr float PILL = 56;

void pill(brls::Button* button, const brls::ButtonStyle* style)
{
    button->setStyle(style);
    button->setFontSize(21);
    button->setCornerRadius(PILL / 2);
    button->setHighlightCornerRadius(PILL / 2);
}

brls::Button* button(brls::Dialog* dialog, const char* id)
{
    auto* b = dynamic_cast<brls::Button*>(dialog->getView(id));
    return b && b->getVisibility() == brls::Visibility::VISIBLE ? b : nullptr;
}

void hide(brls::View* parent, const char* id)
{
    if (brls::View* v = parent->getView(id)) v->setVisibility(brls::Visibility::GONE);
}
}   // namespace

namespace
{
std::string trim(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \n");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \n") - first + 1);
}

std::vector<std::string> split(const std::string& text, const std::string& by)
{
    std::vector<std::string> parts;
    size_t from = 0;
    while (true) {
        const size_t at = text.find(by, from);
        const std::string part = trim(text.substr(from, at == std::string::npos ? std::string::npos : at - from));
        if (!part.empty()) parts.push_back(part);
        if (at == std::string::npos) return parts;
        from = at + by.size();
    }
}

bool starts_with(const std::string& text, const std::string& start)
{
    return !start.empty() && text.compare(0, start.size(), start) == 0;
}

// The question that opens a paragraph ("Add 30 min today?", "Bloquer tous
// les jeux ? …", "…しますか？"), with its mark; "" when it does not open with one.
std::string question_of(const std::string& paragraph)
{
    size_t end = std::string::npos;
    for (const char* mark : { "?", "\xEF\xBC\x9F" }) {   // "?" and the full-width one
        const size_t at = paragraph.find(mark);
        if (at != std::string::npos && at + std::string(mark).size() < end) end = at + std::string(mark).size();
    }
    if (end == std::string::npos || end > 220 || paragraph.substr(0, end).find('\n') != std::string::npos) return "";
    return paragraph.substr(0, end);
}

// The notes the app adds to its dialogs (cadran::add_note).
struct NoteKind
{
    std::string key;   // its text's i18n key
    const char* icon;
    bool        warn;
};
std::vector<NoteKind>& note_kinds()
{
    static std::vector<NoteKind> kinds;
    return kinds;
}

// A note the app adds to its dialogs, and its icon (nullptr: not one).
const char* note_icon(const std::string& paragraph, bool* warn)
{
    *warn = false;
    for (const NoteKind& k : note_kinds()) {
        // Read each time: the language may have changed since (live_prefs).
        const std::string text = brls::getStr(k.key);
        if (starts_with(paragraph, text.substr(0, text.find("{}")))) {
            *warn = k.warn;
            return k.icon;
        }
    }
    return nullptr;
}
}   // namespace

void cadran::add_note(const std::string& key, const char* icon, bool warn)
{
    note_kinds().push_back({ key, icon, warn });
}

brls::Box* cadran::dialog_body(const std::string& text)
{
    namespace kit = cadran::kit;
    brls::Theme theme = brls::Application::getTheme();
    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setId("cadran_dialog_body");
    body->setAlignItems(brls::AlignItems::STRETCH);
    body->setPadding(34, 40, 26, 40);

    std::vector<std::string> paragraphs = split(text, "\n\n");
    // The title: the opening question, else a short first line before more.
    std::string title;
    if (!paragraphs.empty()) {
        title = question_of(paragraphs[0]);
        if (!title.empty()) {
            paragraphs[0] = trim(paragraphs[0].substr(title.size()));
            if (paragraphs[0].empty()) paragraphs.erase(paragraphs.begin());
        } else if (paragraphs.size() > 1 && paragraphs[0].size() <= 90 && paragraphs[0].find('\n') == std::string::npos) {
            title = paragraphs[0];
            paragraphs.erase(paragraphs.begin());
        }
    }
    if (!title.empty()) {
        auto* head = new brls::Box(brls::Axis::ROW);
        head->setId("cadran_dialog_head");
        head->setAlignItems(brls::AlignItems::CENTER);
        brls::Label* label = kit::display(title, kit::TEXT_TITLE);
        label->setGrow(1.0f);
        label->setWidth(0);
        head->addView(label);
        body->addView(head);
    }

    struct Note
    {
        std::string text;
        const char* icon;
        bool warn;
    };
    std::vector<Note> notes;
    for (const std::string& paragraph : paragraphs) {
        bool warn = false;
        if (const char* icon = note_icon(paragraph, &warn)) {
            notes.push_back({ paragraph, icon, warn });
            continue;
        }
        std::vector<std::pair<std::string, std::string>> rows;
        if (cadran_text::table_rows(paragraph, &rows)) {
            // A week of lines: the days that change alike on one line
            // ("Monday – Friday  2 h → 0 min").
            rows = cadran_text::group_rows(rows);
            auto* table = new brls::Box(brls::Axis::COLUMN);
            table->setAlignItems(brls::AlignItems::STRETCH);
            table->setBackgroundColor(theme["cadran/surf2"]);
            table->setCornerRadius(kit::RADIUS_ROW);
            table->setPadding(12, 20, 12, 20);
            table->setMarginTop(body->getChildren().empty() ? 0 : 18);
            for (size_t i = 0; i < rows.size(); i++) {
                auto* row = new brls::Box(brls::Axis::ROW);
                row->setAlignItems(brls::AlignItems::CENTER);
                row->setPaddingTop(7);
                row->setPaddingBottom(7);
                if (i + 1 < rows.size()) {
                    row->setLineBottom(1);
                    row->setLineColor(theme["cadran/line"]);
                }
                brls::Label* key = kit::label(rows[i].first, kit::TEXT_BODY, "cadran/text2");
                key->setGrow(1.0f);
                key->setWidth(0);
                key->setMarginRight(16);
                row->addView(key);
                brls::Label* value = kit::label(rows[i].second, kit::TEXT_BODY, "cadran/text");
                value->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
                value->setMaxWidth(300);
                value->setShrink(0);
                row->addView(value);
                table->addView(row);
            }
            body->addView(table);
            continue;
        }
        brls::Label* label = kit::label(paragraph, kit::TEXT_LEAD, "cadran/text2");
        label->setMarginTop(body->getChildren().empty() ? 0 : 14);
        body->addView(label);
    }
    // A warning first, in an amber card under the title (a game about to be
    // suspended matters before the details); the other notes under a line,
    // each with its icon.
    size_t at = body->getView("cadran_dialog_head") ? 1 : 0;
    for (const Note& note : notes) {
        if (!note.warn) continue;
        auto* card = new brls::Box(brls::Axis::ROW);
        card->setAlignItems(brls::AlignItems::FLEX_START);
        card->setBackgroundColor(theme["cadran/amber_surf"]);
        card->setBorderColor(theme["cadran/amber_line"]);
        card->setBorderThickness(1);
        card->setCornerRadius(kit::RADIUS_ROW);
        card->setPadding(12, 18, 12, 16);
        card->setMarginTop(at > 0 ? 14 : 0);
        CadranIcon* icon = kit::icon(note.icon, 20, "cadran/amber");
        icon->setMarginTop(2);
        icon->setMarginRight(12);
        icon->setShrink(0);
        card->addView(icon);
        brls::Label* label = kit::label(note.text, 17, "cadran/amber");
        label->setGrow(1.0f);
        label->setWidth(0);
        card->addView(label);
        body->addView(card, at++);
    }
    notes.erase(std::remove_if(notes.begin(), notes.end(), [](const Note& n) { return n.warn; }), notes.end());
    // What follows the warning, apart from it.
    if (at > 0 && at < body->getChildren().size()) body->getChildren()[at]->setMarginTop(14);
    for (size_t i = 0; i < notes.size(); i++) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setAlignItems(brls::AlignItems::FLEX_START);
        if (i == 0 && !body->getChildren().empty()) {
            row->setMarginTop(20);
            row->setPaddingTop(16);
            row->setLineTop(1);
            row->setLineColor(theme["cadran/line"]);
        } else if (i > 0) {
            row->setMarginTop(12);
        }
        CadranIcon* icon = kit::icon(notes[i].icon, 18, notes[i].warn ? "cadran/amber" : "cadran/text3");
        icon->setMarginTop(2);
        icon->setMarginRight(12);
        icon->setShrink(0);
        row->addView(icon);
        brls::Label* label = kit::label(notes[i].text, 16, notes[i].warn ? "cadran/amber" : "cadran/text2");
        label->setGrow(1.0f);
        label->setWidth(0);
        row->addView(label);
        body->addView(row);
    }
    // Taller than the dialog's room, the body scrolls (ui::dialog): its lines
    // keep their height there. A label offered less than its wrapped text
    // needs draws one cut line, and borealis caps a label at its parent's
    // height (100 %), which a scrolling body does not settle before it.
    std::function<void(brls::View*)> unbounded = [&unbounded](brls::View* view) {
        view->setShrink(0);
        if (dynamic_cast<brls::Label*>(view)) YGNodeStyleSetMaxHeight(view->getYGNode(), YGUndefined);
        if (auto* box = dynamic_cast<brls::Box*>(view))
            for (brls::View* child : box->getChildren()) unbounded(child);
    };
    for (brls::View* child : body->getChildren()) unbounded(child);
    return body;
}

brls::Label* cadran::dialog_title(const std::string& text)
{
    return cadran::kit::display(text, cadran::kit::TEXT_TITLE);
}

brls::Box* cadran::dialog_badge(const char* icon, const char* color_key, const char* soft_key)
{
    brls::Theme theme = brls::Application::getTheme();
    auto* badge = new brls::Box(brls::Axis::ROW);
    badge->setWidth(44);
    badge->setHeight(44);
    badge->setShrink(0);
    badge->setCornerRadius(22);
    badge->setBackgroundColor(theme[soft_key]);
    badge->setAlignItems(brls::AlignItems::CENTER);
    badge->setJustifyContent(brls::JustifyContent::CENTER);
    badge->addView(cadran::kit::icon(icon, 22, color_key));
    return badge;
}

void cadran::style_dialog(brls::Dialog* dialog, Emphasis emphasis)
{
    if (!dialog || dialog->getView("cadran_dialog")) return;
    brls::Theme theme = brls::Application::getTheme();
    brls::AppletFrame* card = dialog->getAppletFrame();
    card->setCornerRadius(kit::RADIUS_MODAL);
    card->setBackgroundColor(theme["cadran/surf"]);
    card->setBorderColor(theme["cadran/line"]);
    card->setBorderThickness(1);
    auto* mark = new brls::Box();
    mark->setId("cadran_dialog");
    mark->setVisibility(brls::Visibility::GONE);
    card->addView(mark);
    // Something removed or stopped: a coral warning before the title (above
    // the text when it has none).
    auto* head = dynamic_cast<brls::Box*>(dialog->getView("cadran_dialog_head"));
    auto* body = dynamic_cast<brls::Box*>(dialog->getView("cadran_dialog_body"));
    const bool warns = emphasis == Emphasis::DANGER || emphasis == Emphasis::FAILURE;
    if (warns && !head && body) {
        head = new brls::Box(brls::Axis::ROW);
        head->setMarginBottom(14);
        body->addView(head, 0);
    }
    if (warns && head) {
        auto* badge = new brls::Box(brls::Axis::ROW);
        badge->setWidth(44);
        badge->setHeight(44);
        badge->setShrink(0);
        badge->setCornerRadius(22);
        badge->setBackgroundColor(theme["cadran/coral_soft"]);
        badge->setAlignItems(brls::AlignItems::CENTER);
        badge->setJustifyContent(brls::JustifyContent::CENTER);
        badge->setMarginRight(16);
        badge->addView(cadran::kit::icon("alert", 22, "cadran/coral"));
        head->addView(badge, 0);
    }

    // borealis' order: button3 alone above, button1 and button2 in a row.
    brls::Button* b1 = button(dialog, "brls/dialog/button1");
    brls::Button* b2 = button(dialog, "brls/dialog/button2");
    brls::Button* b3 = button(dialog, "brls/dialog/button3");
    brls::Button* last = b3 ? b3 : b2 ? b2 : b1;
    const std::string ok = "hints/ok"_i18n;
    for (brls::Button* b : { b1, b2, b3 }) {
        if (!b) continue;
        const brls::ButtonStyle* look = &PLAIN;
        if (b == last && emphasis == Emphasis::PRIMARY) look = &PRIMARY;
        else if (b == last && emphasis == Emphasis::DANGER) look = &DANGER;
        else if ((emphasis == Emphasis::NONE || emphasis == Emphasis::FAILURE) && b->getText() == ok) look = &PRIMARY;
        pill(b, look);
        // A says what the focused button does ("A Cancel"), not borealis' "OK".
        b->updateActionHint(brls::BUTTON_A, b->getText());
    }
    hide(dialog, "brls/dialog/button2/separator");
    hide(dialog, "brls/dialog/button3/separator");
    if (b2) b2->setMarginLeft(12);
    if (b3) {
        b3->setHeight(PILL);
        b3->setMargins(0, 24, 12, 24);
    }
    if (brls::Box* row = b1 ? b1->getParent() : nullptr) {
        row->setLineTop(0);
        row->setHeight(DIALOG_BUTTONS);
        row->setPadding(0, 24, DIALOG_BUTTONS - PILL, 24);
    }
}

void cadran::style_button(brls::Button* button, Emphasis emphasis)
{
    pill(button, emphasis == Emphasis::PRIMARY ? &PRIMARY : emphasis == Emphasis::DANGER ? &DANGER : &PLAIN);
    button->setHeight(PILL);
}

brls::Box* cadran::wait_body(const std::string& text)
{
    auto* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner->setWidth(60);
    spinner->setHeight(60);
    spinner->setMarginBottom(24);
    brls::Label* label = kit::label(text, kit::TEXT_LEAD, "cadran/text");
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setAlignSelf(brls::AlignSelf::STRETCH);
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(40, 40, 32, 40);
    box->addView(spinner);
    box->addView(label);
    return box;
}

void cadran::Progress::show(float fraction, const std::string& text)
{
    fraction = std::min(1.0f, std::max(0.0f, fraction));
    this->bar->set(fraction);
    this->percent->setText(fmt::format("{} %", (int)(fraction * 100 + 0.001f)));
    this->detail->setText(text);
}

cadran::Progress cadran::progress_body(const std::string& heading, const std::string& title, const std::string& note)
{
    Progress p;
    p.box = new brls::Box(brls::Axis::COLUMN);
    p.box->setAlignItems(brls::AlignItems::STRETCH);
    p.box->setPadding(26, 36, 20, 36);

    auto* head = new brls::Box(brls::Axis::ROW);
    head->setAlignItems(brls::AlignItems::CENTER);
    p.image = new brls::Image();
    p.image->setScalingType(brls::ImageScalingType::FIT);
    p.image->setWidth(56);
    p.image->setHeight(56);
    p.image->setMarginRight(16);
    ui::set_visible(p.image, false);
    head->addView(p.image);
    auto* titles = new brls::Box(brls::Axis::COLUMN);
    titles->setGrow(1.0f);
    titles->setShrink(1.0f);
    p.heading = kit::label(heading, 18, "cadran/text3");
    titles->addView(p.heading);
    p.title = kit::label(title, 22, "cadran/text");
    p.title->setSingleLine(true);   // a long name is cut, not wrapped over the dialog
    p.title->setMarginTop(6);
    titles->addView(p.title);
    head->addView(titles);
    p.box->addView(head);

    p.bar = new kit::Bar();
    p.bar->setMarginTop(22);
    p.box->addView(p.bar);
    auto* numbers = new brls::Box(brls::Axis::ROW);
    numbers->setAlignItems(brls::AlignItems::CENTER);
    numbers->setMarginTop(10);
    p.percent = kit::label("", 22, "cadran/text");
    p.percent->setSingleLine(true);
    p.percent->setMarginRight(14);
    numbers->addView(p.percent);
    p.detail = kit::label("", 18, "cadran/text3");
    p.detail->setSingleLine(true);
    p.detail->setGrow(1.0f);
    p.detail->setShrink(1.0f);
    p.detail->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    numbers->addView(p.detail);
    p.box->addView(numbers);

    if (!note.empty()) {
        brls::Label* label = kit::label(note, 18, "cadran/text3");
        label->setMarginTop(16);
        p.box->addView(label);
    }
    return p;
}

void cadran::prompt_time(const std::string& header, std::vector<int> times, int initial, TimeButton off,
                         TimeButton other, std::function<void(int)> on_value)
{
    namespace kit = cadran::kit;
    if (times.empty()) return;
    brls::Theme theme = brls::Application::getTheme();
    // The time shown: its place in `times`, the first at or after `initial`.
    size_t first = 0;
    while (first + 1 < times.size() && times[first] < initial) first++;
    auto at = std::make_shared<size_t>(first);
    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setAlignItems(brls::AlignItems::CENTER);
    body->setPadding(30, 40, 22, 40);
    brls::Label* title = kit::display(header, kit::TEXT_TITLE);
    title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    body->addView(title);
    CadranIcon* up = kit::icon("caret-up", 30, "cadran/teal");
    up->setMarginTop(18);
    body->addView(up);
    auto* value = new CadranDigits();
    value->setFontSize(56);
    value->setTextColor(theme["cadran/text"]);
    const auto text = [](int t) { return fmt::format("{:02d}:{:02d}", t / 60, t % 60); };
    value->setText(text(times[first]));
    body->addView(value);
    CadranIcon* down = kit::icon("caret-down", 30, "cadran/teal");
    body->addView(down);
    // The keys: the arrows a step, L and R an hour, Y the other way to
    // choose.
    auto* keys = new brls::Box(brls::Axis::ROW);
    keys->setAlignItems(brls::AlignItems::CENTER);
    keys->setMarginTop(16);
    const auto key = [](const char* letter) {
        brls::Box* bubble = kit::key(letter);
        bubble->setMarginRight(4);
        return bubble;
    };
    keys->addView(kit::icon("caret-up", 18, "cadran/text3"));
    keys->addView(kit::icon("caret-down", 18, "cadran/text3"));
    const int step_min = times.size() > 1 ? std::max(1, times[1] - times[0]) : 15;
    brls::Label* step_text = kit::label(ui::fmt_minutes((uint32_t)step_min), 16, "cadran/text3");
    step_text->setMarginLeft(6);
    step_text->setMarginRight(22);
    keys->addView(step_text);
    keys->addView(key("L"));
    keys->addView(key("R"));
    keys->addView(kit::label(ui::fmt_minutes(60), 16, "cadran/text3"));
    brls::Box* other_key = nullptr;
    if (other.action) {
        other_key = new brls::Box(brls::Axis::ROW);
        other_key->setAlignItems(brls::AlignItems::CENTER);
        other_key->setMarginLeft(22);
        other_key->addView(key("Y"));
        other_key->addView(kit::label(other.label, 16, "cadran/text3"));
        keys->addView(other_key);
    }
    body->addView(keys);

    auto* dialog = new brls::Dialog(body);
    const auto move = [at, value, times, text](int delta) {
        *at = (size_t)std::max(0, std::min((int)times.size() - 1, (int)*at + delta));
        value->setText(text(times[*at]));
    };
    const int per_hour = std::max(1, 60 / step_min);
    brls::View* frame = dialog->getAppletFrame();
    for (const auto& [button, delta] : { std::pair<brls::ControllerButton, int>{ brls::BUTTON_NAV_UP, 1 },
                                         { brls::BUTTON_NAV_DOWN, -1 }, { brls::BUTTON_RB, per_hour },
                                         { brls::BUTTON_LB, -per_hour } })
        frame->registerAction("", button, [move, delta = delta](brls::View*) {
            move(delta);
            return true;
        }, true, true, brls::SOUND_FOCUS_CHANGE);
    up->addGestureRecognizer(new brls::TapGestureRecognizer(up, [move]() { move(1); }));
    down->addGestureRecognizer(new brls::TapGestureRecognizer(down, [move]() { move(-1); }));
    if (other.action) {
        const auto open_other = [dialog, action = other.action]() { dialog->close(action); };
        frame->registerAction(other.label, brls::BUTTON_Y, [open_other](brls::View*) {
            open_other();
            return true;
        });
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
        // The desktop build: the keyboard has no Ⓨ, so the Y key (as the main screen).
        frame->registerAction(brls::BrlsKeyCombination(brls::BRLS_KBD_KEY_Y), [open_other](brls::View*) {
            open_other();
            return true;
        });
#endif
        other_key->addGestureRecognizer(new brls::TapGestureRecognizer(other_key, open_other));
    }
    if (off.action) dialog->addButton(off.label, off.action);
    else dialog->addButton("hints/cancel"_i18n, []() {});
    dialog->addButton("hints/ok"_i18n, [at, times, on_value]() {
        if (on_value) on_value(times[*at]);
    });
    dialog->setCancelable(true);
    style_dialog(dialog, Emphasis::PRIMARY);
    // A time is set, then confirmed: A right after the arrows is OK.
    if (brls::View* ok = dialog->getView("brls/dialog/button2")) dialog->setLastFocusedView(ok);
    dialog->open();
}

brls::View* cadran::toast_icon(bool done)
{
    CadranIcon* icon = cadran::kit::icon(done ? "check" : "alert", 22, done ? "cadran/teal_fill" : "cadran/amber_fill");
    icon->setMarginRight(14);
    icon->setShrink(0);
    return icon;
}

