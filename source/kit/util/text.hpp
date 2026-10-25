// text — the small string helpers the parsers share. The two trims differ on
// purpose, and each says what it removes: Markdown keeps a line end that
// matters to it, the other readers drop every kind of space. Header-only,
// plain C++ (ASCII only: no locale).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstring>
#include <string>
#include <vector>

namespace text
{

// `s` without the white space (std::isspace in the C locale: space, \t, \n,
// \v, \f, \r) at either end.
inline std::string trim_space(const std::string& s)
{
    const char* ws = " \t\n\v\f\r";
    const size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(ws) - a + 1);
}

// `s` without the white space (as trim_space) at its end; its start as it is.
inline std::string trim_space_end(std::string s)
{
    const size_t last = s.find_last_not_of(" \t\n\v\f\r");
    s.erase(last == std::string::npos ? 0 : last + 1);
    return s;
}

// `s` without the spaces and tabs at either end (what Markdown calls blank);
// line ends stay.
inline std::string trim_blank(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

// A..Z as a..z, every other byte as it is.
inline std::string lower_ascii(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}

// The first byte a..z as A..Z ("play time" -> "Play time"); a text that
// starts with anything else (a digit, a UTF-8 character) as it is.
inline std::string capitalise_ascii(std::string s)
{
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
    return s;
}

inline bool starts_with(const std::string& s, const char* prefix)
{
    return s.compare(0, std::strlen(prefix), prefix) == 0;
}

inline bool starts_with(const std::string& s, const std::string& prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

inline bool ends_with(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The lines of `s`, split at each \n, a \r before it dropped (CRLF read as
// LF). The empty ones too: a text that ends with \n ends with an empty line,
// and "" is one empty line.
inline std::vector<std::string> lines(const std::string& s)
{
    std::vector<std::string> out;
    for (size_t start = 0; start <= s.size();) {
        size_t end = s.find('\n', start);
        if (end == std::string::npos) end = s.size();
        out.push_back(s.substr(start, end - start));
        if (!out.back().empty() && out.back().back() == '\r') out.back().pop_back();
        start = end + 1;
    }
    return out;
}

// "https://ko-fi.com/jigsawfr" -> "ko-fi.com/jigsawfr": what to type by
// hand. A text without a scheme as it is.
inline std::string short_url(const std::string& url)
{
    const size_t scheme = url.find("://");
    return scheme == std::string::npos ? url : url.substr(scheme + 3);
}

// `s` percent-encoded (RFC 3986) for a URL's query: the unreserved
// characters (A..Z, a..z, 0..9, -_.~) as they are, every other byte as %XX
// ("a b/é" -> "a%20b%2F%C3%A9").
inline std::string url_encode(const std::string& s)
{
    static const char HEX[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += HEX[c >> 4];
            out += HEX[c & 0xF];
        }
    }
    return out;
}

// The parts one after the other, `sep` between two of them: {"a", "b"} with
// "\n" -> "a\nb"; none -> "".
inline std::string join(const std::vector<std::string>& parts, const std::string& sep)
{
    std::string out;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

// The HTML comments of a Markdown text, invisible on GitHub, read line by
// line (each trimmed of white space, as trim_space): skipped, except the
// console's own (the changelog and PlayShelf's release notes)
//   <!-- console: a line -->              that line, read here only;
//   <!-- console:only  …  -->             the lines between, here only;
//   <!-- console:hide --> … <!-- /console -->   the lines between, not here.
class ConsoleComments
{
public:
    enum Step
    {
        Show,        // an ordinary line (one inside console:only too)
        Skip,        // inside a comment or a console:hide block
        Break,       // a comment or a block starts or a console:only ends: no line
        Directive,   // <!-- console: … -->: `inner` is its line
    };

    Step next(const std::string& t, std::string& inner)
    {
        if (block_ == Comment) {
            if (t.find("-->") != std::string::npos) block_ = Plain;
            return Skip;
        }
        if (block_ == Hide) {
            if (t == "<!-- /console -->") block_ = Plain;
            return Skip;
        }
        if (block_ == Only && t == "-->") {
            block_ = Plain;
            return Break;
        }
        if (block_ == Plain && starts_with(t, "<!--")) {
            const bool closed = t.size() >= 7 && t.compare(t.size() - 3, 3, "-->") == 0;
            if (t == "<!-- console:hide -->") {
                block_ = Hide;
            } else if (t == "<!-- console:only") {
                block_ = Only;
            } else if (closed && starts_with(t, "<!-- console:")) {
                inner = trim_space(t.substr(13, t.size() - 16));
                return Directive;
            } else if (!closed) {
                block_ = Comment;
            }
            return Break;
        }
        return Show;
    }

private:
    enum { Plain, Comment, Hide, Only } block_ = Plain;
};

}   // namespace text
