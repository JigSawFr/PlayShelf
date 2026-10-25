// ui — dialogs, confirmations, a web page's QR code, the dropdown, and keyboard prompts.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "ui/cadran_kit.hpp"
#include "ui/cadran_modal.hpp"
#include "ui/cadran_list.hpp"
#include "ui/cadran_text.hpp"
#include "ui/hooks.hpp"
#include "ui/live_prefs.hpp"
#include "ui/text_size.hpp"
#include "util/config.hpp"
#include "util/text.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <yoga/Yoga.h>

using namespace brls::literals;

namespace ui
{

void on_cancel(brls::Dialog* dialog, std::function<void()> on_cancel)
{
    dialog->setCancelable(true);
    dialog->getAppletFrame()->registerAction(
        "hints/back"_i18n, brls::BUTTON_B,
        [dialog, on_cancel](brls::View*) {
            dialog->close([on_cancel]() { if (on_cancel) on_cancel(); });
            return true;
        },
        false, false, brls::SOUND_BACK);
}

void add_cancel(brls::Dialog* dialog, std::function<void()> cancel)
{
    dialog->addButton("hints/cancel"_i18n, cancel);
    on_cancel(dialog, cancel);
}

// Height of the dialog's content above its button row (its card keeps a
// margin from the screen's top and bottom).
static float content_height()
{
    return brls::Application::contentHeight - cadran::DIALOG_BUTTONS - 2 * cadran::DIALOG_MARGIN;
}

static void scroll_with(brls::Dialog* dialog, brls::ScrollingFrame* frame, brls::View* text)
{
    for (const brls::ControllerButton button : { brls::BUTTON_NAV_UP, brls::BUTTON_NAV_DOWN }) {
        const bool down = button == brls::BUTTON_NAV_DOWN;
        dialog->getAppletFrame()->registerAction(
            "", button,
            [frame, text, down](brls::View*) {
                const float page = std::round(frame->getHeight() / 2);
                const float limit = std::max(0.0f, text->getHeight() - frame->getHeight());
                const float target = std::min(std::max(frame->getContentOffsetY() + (down ? page : -page), 0.0f), limit);
                frame->setContentOffsetY(target, true);
                return true;
            },
            true, true, brls::SOUND_NONE);
    }
}

namespace
{
// A dialog's text that scrolls: its foot fades into the card while more
// follows below the buttons' row.
class DialogScroll : public brls::ScrollingFrame
{
  public:
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
    {
        brls::ScrollingFrame::draw(vg, x, y, width, height, style, ctx);
        cadran::kit::draw_more_below(vg, this, x, y, width, height, "cadran/surf");
    }
};
}   // namespace

brls::Dialog* box_dialog(brls::Box* body)
{
    YGNodeCalculateLayout(body->getYGNode(), 720, YGUndefined, YGDirectionLTR);
    const float room = content_height();
    if (YGNodeLayoutGetHeight(body->getYGNode()) <= room) return new brls::Dialog(body);
    // In a scrolling frame, each line keeps the height of its text (borealis
    // caps a label at its parent's, which a scrolling body does not settle).
    std::function<void(brls::View*)> unbounded = [&unbounded](brls::View* view) {
        view->setShrink(0);
        if (dynamic_cast<brls::Label*>(view)) YGNodeStyleSetMaxHeight(view->getYGNode(), YGUndefined);
        if (auto* box = dynamic_cast<brls::Box*>(view))
            for (brls::View* child : box->getChildren()) unbounded(child);
    };
    for (brls::View* child : body->getChildren()) unbounded(child);
    auto* frame = new DialogScroll();
    frame->setHeight(room);
    frame->setScrollingBehavior(brls::ScrollingBehavior::NATURAL);
    frame->setContentView(body);
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::STRETCH);
    box->addView(frame);
    auto* dialog = new brls::Dialog(box);
    scroll_with(dialog, frame, body);
    return dialog;
}

// The text laid out by cadran::dialog_body, `below` under it (the card's
// width unless it aligns itself), scrolling when taller than the dialog's room.
static brls::Dialog* body_dialog(const std::string& text, brls::View* below = nullptr)
{
    brls::Box* body = cadran::dialog_body(text);
    if (below) {
        if (!body->getChildren().empty()) below->setMarginTop(18);
        body->addView(below);
    }
    return box_dialog(body);
}

brls::Dialog* dialog(const std::string& text)
{
    return body_dialog(text);
}

brls::Dialog* dialog(const std::string& text, brls::View* below)
{
    return body_dialog(text, below);
}

static void open_confirm(const std::string& body, const std::string& confirm_label,
                         std::function<void()> on_yes, std::function<void()> on_no, bool danger)
{
    auto* dialog = body_dialog(body);
    dialog->addButton("hints/cancel"_i18n, [on_no]() { if (on_no) on_no(); });
    dialog->addButton(confirm_label, [on_yes]() { if (on_yes) on_yes(); });
    on_cancel(dialog, on_no);
    cadran::style_dialog(dialog, danger ? cadran::Emphasis::DANGER : cadran::Emphasis::PRIMARY);
    dialog->open();
}

void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no, bool danger)
{
    open_confirm(body, confirm_label, std::move(on_yes), std::move(on_no), danger);
}

void confirm_danger(const std::string& body, const std::string& confirm_label, std::function<void()> on_yes)
{
    open_confirm(body, confirm_label, std::move(on_yes), nullptr, true);
}

void show_failure(const cadran_text::Failure& f)
{
    const std::string& title = f.title;
    const std::string& rest = f.reason;
    const std::string& code = f.code;
    const bool report = f.report;
    const std::string body = title.empty() ? rest : rest.empty() ? title : title + "\n\n" + rest;
    auto* dialog = body_dialog(body.empty() ? code : body);
    if (!code.empty() && !body.empty()) {
        if (auto* box = dynamic_cast<brls::Box*>(dialog->getView("cadran_dialog_body"))) {
            brls::Label* small = cadran::kit::label(code, 15, "cadran/text3");
            small->setMarginTop(10);
            box->addView(small);
        }
    }
    if (report) {
        dialog->addButton("playshelf/notices/send_report"_i18n, []() {
            if (hooks::get().send_report) hooks::get().send_report();
        });
    }
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->setCancelable(true);
    cadran::style_dialog(dialog, cadran::Emphasis::FAILURE);
    // A closes it, as before: the report is a choice, not the default.
    if (report)
        if (brls::View* ok = dialog->getView("brls/dialog/button2")) dialog->setLastFocusedView(ok);
    dialog->open();
}

void info(const std::string& body)
{
    auto* dialog = ui::dialog(body);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

void qr_dialog(const std::string& body, const std::string& url)
{
    brls::Box* code = cadran::kit::qr(url, 240, "", text::short_url(url));
    code->setAlignSelf(brls::AlignSelf::CENTER);
    auto* dialog = body_dialog(body, code);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

brls::Box* check_row(const CheckLine& line, float size)
{
    namespace kit = cadran::kit;
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::FLEX_START);
    const char* color = line.state == Check::Ok     ? "cadran/teal"
                        : line.state == Check::Warn ? "cadran/amber"
                        : line.state == Check::Bad  ? "cadran/coral"
                                                    : "cadran/text3";
    auto* mark = new kit::Mark(color);
    mark->setMarginTop(size * 0.3f);
    mark->setMarginRight(14);
    mark->setShrink(0);
    row->addView(mark);
    brls::Label* text = kit::label(line.text, size, line.state == Check::Info ? "cadran/text2" : "cadran/text");
    text->setGrow(1.0f);
    text->setWidth(0);
    row->addView(text);
    return row;
}

namespace
{
// A panel of check lines, as dialog_body's tables: the surface, rounded.
brls::Box* check_panel()
{
    auto* panel = new brls::Box(brls::Axis::COLUMN);
    panel->setAlignItems(brls::AlignItems::STRETCH);
    panel->setBackgroundColor(brls::Application::getTheme()["cadran/surf2"]);
    panel->setCornerRadius(14);
    panel->setPadding(12, 18, 14, 18);
    return panel;
}
}   // namespace

brls::Box* checklist_body(const std::string& title, const std::vector<CheckLine>& lines, const std::string& note,
                          const std::string& subtitle)
{
    namespace kit = cadran::kit;
    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setAlignItems(brls::AlignItems::STRETCH);
    body->setPadding(30, 40, 20, 40);
    body->addView(cadran::dialog_title(title));
    if (!subtitle.empty()) {
        brls::Label* sub = kit::label(subtitle, 17, "cadran/text2");
        sub->setMarginTop(6);
        body->addView(sub);
    }
    // A Header line starts a panel of its own, its name small above it.
    brls::Box* panel = nullptr;
    for (const CheckLine& line : lines) {
        if (line.state == Check::Header) {
            brls::Label* head = kit::label(cadran::upper(line.text), 13, "cadran/text3");
            head->setMarginTop(16);
            head->setMarginBottom(6);
            head->setMarginLeft(4);
            body->addView(head);
            panel = check_panel();
            body->addView(panel);
            continue;
        }
        if (!panel) {
            panel = check_panel();
            panel->setMarginTop(16);
            body->addView(panel);
        }
        brls::Box* row = check_row(line, 16.5f);
        row->setMarginTop(panel->getChildren().empty() ? 0 : 8);
        panel->addView(row);
    }
    if (!note.empty()) {
        auto* foot = new brls::Box(brls::Axis::ROW);
        foot->setAlignItems(brls::AlignItems::FLEX_START);
        foot->setMarginTop(14);
        CadranIcon* icon = kit::icon("info", 16, "cadran/text3");
        icon->setMarginTop(2);
        icon->setMarginRight(10);
        icon->setShrink(0);
        foot->addView(icon);
        brls::Label* small = kit::label(note, 15, "cadran/text3");
        small->setGrow(1.0f);
        small->setWidth(0);
        foot->addView(small);
        body->addView(foot);
    }
    return body;
}

void notice(const char* icon, Check tone, const std::string& title, const std::string& text)
{
    namespace kit = cadran::kit;
    const char* color = tone == Check::Ok ? "cadran/teal" : tone == Check::Bad ? "cadran/coral" : tone == Check::Warn ? "cadran/amber" : "cadran/text2";
    const char* soft = tone == Check::Ok ? "cadran/teal_soft" : tone == Check::Bad ? "cadran/coral_soft" : tone == Check::Warn ? "cadran/amber_surf" : "cadran/surf2";
    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setAlignItems(brls::AlignItems::STRETCH);
    body->setPadding(30, 40, 22, 40);
    auto* head = new brls::Box(brls::Axis::ROW);
    head->setAlignItems(brls::AlignItems::CENTER);
    brls::Box* badge = cadran::dialog_badge(icon, color, soft);
    badge->setMarginRight(16);
    head->addView(badge);
    brls::Label* label = cadran::dialog_title(title);
    label->setGrow(1.0f);
    label->setWidth(0);
    head->addView(label);
    body->addView(head);
    brls::Label* words = kit::label(text, 18, "cadran/text2");
    words->setMarginTop(14);
    body->addView(words);
    auto* dialog = box_dialog(body);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->setCancelable(true);
    cadran::style_dialog(dialog, cadran::Emphasis::NONE);
    dialog->open();
}

namespace
{
// How long A is held so far (hold_confirm); 0 once let go.
struct Held
{
    int64_t start = -1, last = 0;
    bool    done = false;
    bool    touching = false;          // a finger held on the button
    std::function<void()> full;        // held long enough
};
constexpr int64_t HOLD_US = 2000000;
// borealis repeats a held key's action after 250 ms, then every 100 ms: a
// longer gap is a key let go.
constexpr int64_t LET_GO_US = 350000;

double held_part(Held& h, int64_t now)
{
    if (h.start < 0) return 0;
    if (now - h.last > LET_GO_US) {
        h.start = -1;
        return 0;
    }
    return std::min(1.0, (double)(now - h.start) / HOLD_US);
}

// Over the button held: a lighter fill from its left edge as it is held,
// within its pill; gone when it is let go.
class HoldFill : public brls::View
{
  public:
    explicit HoldFill(std::shared_ptr<Held> hold) : held(std::move(hold))
    {
        this->setFocusable(false);
        this->setPositionType(brls::PositionType::ABSOLUTE);
        this->setPositionTop(0);
        this->setPositionLeft(0);
        this->setWidthPercentage(100);
        this->setHeightPercentage(100);
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*) override
    {
        const int64_t now = brls::getCPUTimeUsec();
        // A finger on the button: held as long as it stays (no key repeats).
        if (this->held->touching) this->held->last = now;
        const double part = held_part(*this->held, now);
        if (part >= 1 && !this->held->done && this->held->full) {
            this->held->done = true;
            // Not while drawing: closing the dialog changes the views drawn.
            brls::sync([held = this->held]() { held->full(); });
        }
        if (part <= 0) return;
        nvgSave(vg);
        nvgIntersectScissor(vg, x, y, (float)(width * part), height);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, width, height, height / 2);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 70));
        nvgFill(vg);
        nvgRestore(vg);
    }

  private:
    std::shared_ptr<Held> held;
};
}   // namespace

void hold_confirm(brls::Box* body, const std::string& hold_label, std::function<void()> on_yes,
                  std::function<void()> on_no)
{
    namespace kit = cadran::kit;
    auto held = std::make_shared<Held>();
    // How: the A key's bubble, then the words.
    auto* how = new brls::Box(brls::Axis::ROW);
    how->setAlignItems(brls::AlignItems::CENTER);
    how->setJustifyContent(brls::JustifyContent::CENTER);
    how->setMarginTop(16);
    brls::Box* a = kit::key("A", 22);
    a->setMarginRight(8);
    how->addView(a);
    how->addView(kit::label(brls::getStr("playshelf/common/hold_hint", hold_label), 15, "cadran/text2"));
    body->addView(how);
    auto* dialog = box_dialog(body);
    dialog->addButton("hints/cancel"_i18n, [on_no]() { if (on_no) on_no(); });
    dialog->addButton(hold_label, []() {});
    on_cancel(dialog, on_no);
    cadran::style_dialog(dialog, cadran::Emphasis::PRIMARY);
    held->full = [dialog, on_yes]() { dialog->close([on_yes]() { if (on_yes) on_yes(); }); };
    if (auto* yes = dynamic_cast<brls::Box*>(dialog->getView("brls/dialog/button2"))) {
        yes->addView(new HoldFill(held));
        yes->registerAction(hold_label, brls::BUTTON_A, [held](brls::View*) {
            const int64_t now = brls::getCPUTimeUsec();
            if (held->done) return true;
            if (held->start < 0 || now - held->last > LET_GO_US) held->start = now;
            held->last = now;
            if (now - held->start >= HOLD_US) {
                held->done = true;
                held->full();
            }
            return true;
        }, false, true, brls::SOUND_NONE);
        // A finger held on it does the same; lifted or moved away, it stops.
        yes->addGestureRecognizer(new brls::TapGestureRecognizer([held](brls::TapGestureStatus status, brls::Sound*) {
            const bool down = status.state == brls::GestureState::UNSURE || status.state == brls::GestureState::START ||
                              status.state == brls::GestureState::STAY;
            if (down && !held->touching) held->start = held->last = brls::getCPUTimeUsec();
            held->touching = down;
        }));
    }
    dialog->open();
}

void prompt_text(const std::string& header, const std::string& initial, int max_len,
                 std::function<void(std::string)> on_value)
{
    brls::Application::getImeManager()->openForText(
        [on_value](std::string text) { if (!text.empty()) on_value(text); },
        header, "", max_len, initial);
}

namespace
{
template <size_t N>
int index_of(const char* const (&list)[N], const std::string& value)
{
    for (size_t i = 0; i < N; i++)
        if (value == list[i]) return (int)i;
    return 0;
}

// Saves `value` in `field`, then applies it at once (`apply`: every screen
// made again), or refreshes when nothing changed.
void choose(std::string& field, const char* value, std::function<void()> after, void (*apply)())
{
    if (field == value) {
        if (after) after();
        return;
    }
    field = value;
    save_config();
    apply();
}
}   // namespace

// Each language's flag (config::LANGUAGES' order), from the Twemoji set of
// img/emoji/: the console's own a globe; Latin American Spanish, Mexico's.
constexpr const char* LANGUAGE_FLAGS[] = { "1f310",       "1f1fa-1f1f8", "1f1eb-1f1f7", "1f1e8-1f1e6",
                                           "1f1e9-1f1ea", "1f1ea-1f1f8", "1f1f2-1f1fd", "1f1ee-1f1f9",
                                           "1f1f3-1f1f1", "1f1f5-1f1f9", "1f1e7-1f1f7", "1f1f7-1f1fa",
                                           "1f1ef-1f1f5", "1f1f0-1f1f7", "1f1e8-1f1f3", "1f1f9-1f1fc" };
static_assert(std::size(LANGUAGE_FLAGS) == std::size(config::LANGUAGES), "a flag for each language");

void pick_language(std::function<void()> after)
{
    std::vector<std::string> labels, flags;
    for (const char* l : config::LANGUAGES) labels.push_back(brls::getStr(std::string("playshelf/tools/languages/") + l));
    for (const char* f : LANGUAGE_FLAGS) flags.push_back(std::string("img/emoji/") + f + ".png");
    pick("playshelf/tools/language"_i18n, labels, index_of(config::LANGUAGES, config::get().language), [after](int i) {
        choose(config::get().language, config::LANGUAGES[i], after, live_prefs::language_changed);
    }, true, {}, flags);
}

void pick_theme(std::function<void()> after)
{
    std::vector<std::string> labels;
    for (const char* t : config::THEMES) labels.push_back(brls::getStr(std::string("playshelf/tools/themes/") + t));
    // As the header's button draws them (view/header_settings.cpp).
    const std::vector<std::string> icons = { "contrast", "sun", "moon" };
    static_assert(std::size(config::THEMES) == 3, "an icon for each theme");
    pick("playshelf/tools/theme"_i18n, labels, index_of(config::THEMES, config::get().theme), [after](int i) {
        choose(config::get().theme, config::THEMES[i], after, live_prefs::theme_changed);
    }, true, {}, icons);
}

void pick_text_size(std::function<void()> after)
{
    std::vector<std::string> labels;
    for (const char* t : config::TEXT_SIZES) labels.push_back(brls::getStr(std::string("playshelf/prefs/text_sizes/") + t));
    const std::vector<std::string> icons = { "tv", "text-normal", "text-large" };
    static_assert(std::size(config::TEXT_SIZES) == 3, "an icon for each text size");
    pick("playshelf/prefs/text_size"_i18n, labels, index_of(config::TEXT_SIZES, config::get().text_size), [after](int i) {
        // At once, no restart: every screen is laid out again.
        config::get().text_size = config::TEXT_SIZES[i];
        save_config();
        text_size::apply();
        if (after) after();
    }, true, {}, icons);
}

std::string text_size_name(bool tile)
{
    if (tile && config::get().text_size == std::string("auto")) return "playshelf/tiles/text_size_auto"_i18n;
    return brls::getStr("playshelf/prefs/text_sizes/" + std::string(config::TEXT_SIZES[index_of(config::TEXT_SIZES, config::get().text_size)]));
}

std::string language_name(bool tile)
{
    // The names of a language spoken in two places, shorter on a tile: the
    // place as its code.
    static const std::pair<const char*, const char*> SHORT[] = {
        { "fr", "Français (FR)" }, { "fr-CA", "Français (CA)" }, { "es", "Español (ES)" },
        { "es-419", "Español (LatAm)" }, { "pt", "Português (PT)" }, { "pt-BR", "Português (BR)" },
    };
    if (tile)
        for (const auto& [code, name] : SHORT)
            if (config::get().language == code) return name;
    return brls::getStr("playshelf/tools/languages/" + std::string(config::LANGUAGES[index_of(config::LANGUAGES, config::get().language)]));
}

std::string theme_name(bool tile)
{
    return brls::getStr(std::string(tile ? "playshelf/tiles/themes/" : "playshelf/tools/themes/") + std::string(config::THEMES[index_of(config::THEMES, config::get().theme)]));
}

void quick_settings(std::function<void()> after)
{
    const std::vector<std::string> labels = { "playshelf/tools/language"_i18n + " · " + language_name(),
                                              "playshelf/tools/theme"_i18n + " · " + theme_name(),
                                              "playshelf/prefs/text_size"_i18n + " · " + text_size_name() };
    // As the tiles of Preferences draw them (tab/preferences_tab.cpp).
    const std::vector<std::string> icons = { "language", "contrast", "eye" };
    pick("playshelf/quick/title_text"_i18n, labels, 0, [after](int i) {
        if (i == 0) pick_language(after);
        else if (i == 1) pick_theme(after);
        else pick_text_size(after);
    }, false, {}, icons);
}

}   // namespace ui
