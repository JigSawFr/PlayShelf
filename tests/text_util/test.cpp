// Host tests for the header-only helpers of source/util: utf8.hpp (the strict
// and the loose reader, the writer, where to cut), emoji.hpp (what counts,
// the icon a text opens on, dropping them), text.hpp (the trims, ASCII lower
// case and capital, a prefix and a suffix, lines, a URL without its scheme,
// percent-encoding, parts joined) and json_util.hpp (fields read by type,
// arrays and objects, a text that is no JSON).
#include "check.h"
#include <cstdio>
#include <string>
#include <vector>

#include "util/emoji.hpp"
#include "util/json_util.hpp"
#include "util/text.hpp"
#include "util/utf8.hpp"

static void test_utf8()
{
    uint32_t cp = 0;
    CHECK(utf8::decode("A", 0, &cp) == 1 && cp == 'A');
    CHECK(utf8::decode("\xC3\xA9", 0, &cp) == 2 && cp == 0xE9);                    // é
    CHECK(utf8::decode("\xE2\x80\x94", 0, &cp) == 3 && cp == 0x2014);              // —
    CHECK(utf8::decode("\xF0\x9F\x9A\x80", 0, &cp) == 4 && cp == 0x1F680);         // 🚀
    CHECK(utf8::decode("\x80", 0, &cp) == 0);                                       // stray continuation
    CHECK(utf8::decode("\xC0\x80", 0, &cp) == 0 && utf8::decode("\xE0\x80\x80", 0, &cp) == 0);   // overlong
    CHECK(utf8::decode("\xED\xA0\x80", 0, &cp) == 0);                               // surrogate
    CHECK(utf8::decode("\xF4\x90\x80\x80", 0, &cp) == 0);                           // beyond U+10FFFF
    CHECK(utf8::decode("\xE2\x80", 0, &cp) == 0 && utf8::decode("\xE2\x41\x41", 0, &cp) == 0);   // cut, broken
    CHECK(utf8::decode("x\xC3\xA9", 1, &cp) == 2 && cp == 0xE9);

    CHECK(utf8::decode_loose("\xF0\x9F\x9A\x80", 0, &cp) == 4 && cp == 0x1F680);
    CHECK(utf8::decode_loose("\x80", 0, &cp) == 1 && cp == 0x80);                   // its own code point
    CHECK(utf8::decode_loose("\xFF", 0, &cp) == 1 && cp == 0xFF);
    CHECK(utf8::decode_loose("\xE2\x80", 0, &cp) == 3);                             // cut: what is there

    std::string out;
    for (uint32_t c : { 0x41u, 0xE9u, 0x2014u, 0x1F680u, 0x10FFFFu }) utf8::append(out, c);
    CHECK(out == "A\xC3\xA9\xE2\x80\x94\xF0\x9F\x9A\x80\xF4\x8F\xBF\xBF");
    size_t i = 0, n = 0;
    while (i < out.size()) {
        const size_t len = utf8::decode(out, i, &cp);
        if (!CHECK(len)) break;
        i += len;
        n++;
    }
    CHECK(n == 5);

    // Where to cut: back to the start of the character there.
    const std::string s = "a\xC3\xA9\xF0\x9F\x9A\x80z";   // a, é, 🚀, z
    CHECK(utf8::boundary_before(s, 0) == 0 && utf8::boundary_before(s, 1) == 1 && utf8::boundary_before(s, 2) == 1);
    CHECK(utf8::boundary_before(s, 3) == 3 && utf8::boundary_before(s, 4) == 3 && utf8::boundary_before(s, 6) == 3);
    CHECK(utf8::boundary_before(s, 7) == 7 && utf8::boundary_before(s, 8) == 8 && utf8::boundary_before(s, 9) == 9);
    CHECK(utf8::boundary_before("\x80\x80", 1) == 0 && utf8::boundary_before("", 0) == 0);   // strays: all the way back
}

static void test_emoji()
{
    CHECK(emoji::is_emoji(0x1F680) && emoji::is_emoji(0x2615) && emoji::is_emoji(0x23F3) && emoji::is_emoji(0x2B50));
    CHECK(emoji::is_emoji(0x200D) && emoji::is_emoji(0xFE0F) && emoji::is_emoji(0x20E3));   // what goes with one
    CHECK(!emoji::is_emoji('A') && !emoji::is_emoji(0xE9) && !emoji::is_emoji(0x2014) && !emoji::is_emoji(0x2192));
    CHECK(emoji::is_modifier(0x20E3) && !emoji::is_modifier(0x1F680));

    CHECK(emoji::icon_at("\xF0\x9F\x9A\x80 Features") == "1f680");
    CHECK(emoji::icon_at("**\xF0\x9F\x9A\x80**", 2) == "1f680");
    CHECK(emoji::icon_at("Features").empty() && emoji::icon_at("").empty() && emoji::icon_at("ab", 5).empty());
    CHECK(emoji::icon_at("\xEF\xB8\x8F x").empty() && emoji::icon_at("\xE2\x83\xA3").empty());   // a modifier alone

    // 1️⃣ (1, U+FE0F, U+20E3): the digit stays, the keycap marks go.
    CHECK(emoji::drop("1\xEF\xB8\x8F\xE2\x83\xA3 first") == "1 first");
    CHECK(emoji::drop("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x92\xBB dev") == " dev");   // a ZWJ sequence
    CHECK(emoji::drop("caf\xC3\xA9 \xE2\x80\x94 ok") == "caf\xC3\xA9 \xE2\x80\x94 ok");
}

static void test_text()
{
    CHECK(text::trim_space(" \t\r\n x y \v\f\n") == "x y");
    CHECK(text::trim_space("   ").empty() && text::trim_space("").empty() && text::trim_space("a") == "a");
    CHECK(text::trim_blank(" \t x y \t ") == "x y");
    CHECK(text::trim_blank("x\n") == "x\n" && text::trim_blank("\r x") == "\r x");   // line ends stay
    CHECK(text::trim_blank(" \t ").empty());
    CHECK(text::trim_space_end(" x y \t\r\n") == " x y" && text::trim_space_end(" \n").empty() && text::trim_space_end("").empty());
    CHECK(text::lower_ascii("Hello WORLD \xC3\x89") == "hello world \xC3\x89");
    CHECK(text::capitalise_ascii("play time") == "Play time" && text::capitalise_ascii("Already") == "Already");
    CHECK(text::capitalise_ascii("\xC3\xA9t\xC3\xA9") == "\xC3\xA9t\xC3\xA9" && text::capitalise_ascii("2 games") == "2 games");
    CHECK(text::capitalise_ascii("").empty() && text::capitalise_ascii("`code`") == "`code`");
    CHECK(text::starts_with("## Title", "## ") && !text::starts_with("#", "## ") && text::starts_with("x", ""));
    CHECK(text::starts_with("zh-Hans", std::string("zh")) && !text::starts_with("z", std::string("zh")));
    CHECK(text::starts_with("a", std::string()) && !text::starts_with("", std::string("a")));
    CHECK(text::ends_with("sdmc:/x.nro", ".nro") && !text::ends_with("nro", ".nro") && text::ends_with("x", ""));

    using Lines = std::vector<std::string>;
    CHECK(text::lines("a\nb") == (Lines{ "a", "b" }) && text::lines("a\r\nb\r\n") == (Lines{ "a", "b", "" }));
    CHECK(text::lines("") == Lines{ "" } && text::lines("\n") == (Lines{ "", "" }));
    CHECK(text::lines("a\r\r\nb\r") == (Lines{ "a\r", "b" }) && text::lines("\rx") == Lines{ "\rx" });   // one \r, at the end

    CHECK(text::url_encode("aZ09-_.~") == "aZ09-_.~");
    CHECK(text::url_encode("https://gist.github.com/A b") == "https%3A%2F%2Fgist.github.com%2FA%20b");
    CHECK(text::url_encode("\xC3\xA8") == "%C3%A8" && text::url_encode(std::string("\0\x7F", 2)) == "%00%7F");
    CHECK(text::short_url("https://ko-fi.com/jigsawfr") == "ko-fi.com/jigsawfr");
    CHECK(text::short_url("github.com/x") == "github.com/x" && text::short_url("").empty());
    CHECK(text::join({ "a", "b", "c" }, "\n") == "a\nb\nc" && text::join({ "a" }, " · ") == "a");
    CHECK(text::join({}, "\n").empty() && text::join({ "", "b", "" }, ",") == ",b,");   // empty parts kept
}

// Each line through text::ConsoleComments: "S" shown, "-" skipped, "|" a
// break, "=x" the directive's line x.
static std::string comment_steps(const std::vector<std::string>& lines)
{
    text::ConsoleComments c;
    std::string out;
    for (const std::string& l : lines) {
        std::string inner = "untouched";
        const auto step = c.next(l, inner);
        switch (step) {
            case text::ConsoleComments::Show:      out += "S"; break;
            case text::ConsoleComments::Skip:      out += "-"; break;
            case text::ConsoleComments::Break:     out += "|"; break;
            case text::ConsoleComments::Directive: out += "=" + inner; break;
        }
        if (step != text::ConsoleComments::Directive && inner != "untouched") out += "!";   // set for a directive only
    }
    return out;
}

static void test_console_comments()
{
    // Plain lines; a one-line comment, a comment over several lines.
    CHECK(comment_steps({ "a", "", "<!-- note -->", "b" }) == "SS|S");
    CHECK(comment_steps({ "<!-- open", "inside", "<!-- still -->", "after" }) == "|--S");
    CHECK(comment_steps({ "<!--", "x -->", "y" }) == "|-S");
    // The console's line, trimmed; an empty one.
    CHECK(comment_steps({ "<!-- console:  here only  -->", "a" }) == "=here onlyS");
    CHECK(comment_steps({ "<!-- console:-->" }) == "=");
    // console:only: the lines between shown, a comment inside them too, up to "-->".
    CHECK(comment_steps({ "<!-- console:only", "a", "<!-- x -->", "-->", "b" }) == "|SS|S");
    // console:hide up to <!-- /console --> only (a "-->" inside does not end it).
    CHECK(comment_steps({ "<!-- console:hide -->", "a", "-->", "<!-- /console -->", "b" }) == "|---S");
    // Not the console's: an unclosed "<!-- console: x" is a comment, a stray
    // "-->" or "<!-- /console -->" outside a block an ordinary line or a comment.
    CHECK(comment_steps({ "<!-- console: x", "y -->", "z" }) == "|-S");
    CHECK(comment_steps({ "-->", "<!-- /console -->" }) == "S|");
    CHECK(comment_steps({ "<!-- console:hide", "a -->", "b" }) == "|-S");   // not the exact marker: a comment
}

static void test_json()
{
    nlohmann::json j;
    CHECK(json_util::parse(R"({"s": "v", "b": true, "f": false, "n": 42, "x": 1.5, "t": "true"})", &j));
    CHECK(json_util::string_at(j, "s") == "v" && json_util::string_at(j, "n").empty() && json_util::string_at(j, "-").empty());
    CHECK(json_util::flag_at(j, "b") && !json_util::flag_at(j, "f") && !json_util::flag_at(j, "t") && !json_util::flag_at(j, "-"));
    int64_t v = -1;
    CHECK(json_util::integer_at(j, "n", &v) && v == 42);
    v = -1;
    CHECK(!json_util::integer_at(j, "x", &v) && !json_util::integer_at(j, "s", &v) && v == -1);
    CHECK(json_util::integer_or(j, "n") == 42 && json_util::integer_or(j, "x") == 0 && json_util::integer_or(j, "-", 7) == 7);

    CHECK(json_util::parse(R"({"a": [1], "o": {"k": 1}, "s": "[]", "e": []})", &j));
    const nlohmann::json* a = json_util::array_at(j, "a");
    CHECK(a && a->size() == 1 && json_util::array_at(j, "e") && !json_util::array_at(j, "o") && !json_util::array_at(j, "s"));
    const nlohmann::json* o = json_util::object_at(j, "o");
    CHECK(o && json_util::integer_or(*o, "k") == 1 && !json_util::object_at(j, "a") && !json_util::object_at(j, "-"));

    // Not an object: no field, no exception.
    CHECK(json_util::parse("[1, 2]", &j) && json_util::string_at(j, "s").empty() && !json_util::flag_at(j, "b"));
    CHECK(!json_util::array_at(j, "s") && !json_util::object_at(j, "s"));
    CHECK(json_util::parse("\"text\"", &j) && !json_util::integer_at(j, "n", &v));
    CHECK(!json_util::parse("{\"s\": ", &j) && !json_util::parse("", &j) && !json_util::parse("nope", &j));
    CHECK(json_util::integer_or(j, "n", 3) == 3);

    // parse_with: the reader's answer, false for no JSON or a reader that throws.
    auto is_object = [](const nlohmann::json& d) { return d.is_object(); };
    CHECK(json_util::parse_with("{}", is_object) && !json_util::parse_with("[]", is_object));
    CHECK(!json_util::parse_with("{", is_object) && !json_util::parse_with("", is_object));
    CHECK(!json_util::parse_with(R"({"b": "yes"})", [](const nlohmann::json& d) { return d.value("b", false); }));
    CHECK(json_util::parse_with(R"({"b": true})", [](const nlohmann::json& d) { return d.value("b", false); }));
}

int main()
{
    test_utf8();
    test_emoji();
    test_text();
    test_console_comments();
    test_json();
    CHECK_DONE("utf8, emoji, text and json_util assertions passed");
}
