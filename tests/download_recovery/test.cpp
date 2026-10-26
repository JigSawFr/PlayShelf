// download/recovery.cpp: a failed job's cause read from its error, the
// tries PlayShelf makes by itself and when, what the user is offered for
// each cause, a package's address moved to its server's other one, a
// package the console has by now, a package found again in a catalogue.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "download/recovery.hpp"

#include <algorithm>

using namespace download;

namespace
{
bool offers(const std::vector<Remedy>& all, Remedy r)
{
    return std::find(all.begin(), all.end(), r) != all.end();
}

void test_causes()
{
    // What the transfer says (libcurl's words, http_logic's).
    CHECK(cause_of("Could not resolve host: shelf.example") == Cause::Network);
    CHECK(cause_of("Failed to connect to 192.168.1.20 port 8443 after 3001 ms: Couldn't connect to server") == Cause::Network);
    CHECK(cause_of("Operation too slow. Less than 1 bytes/sec transferred the last 30 seconds") == Cause::Network);
    CHECK(cause_of("Recv failure: Connection reset by peer") == Cause::Network);
    CHECK(cause_of("the range came out short") == Cause::Network);
    CHECK(cause_of("HTTP 503") == Cause::Server && cause_of("HTTP 500") == Cause::Server && cause_of("HTTP 429") == Cause::Server);
    CHECK(cause_of("HTTP 401") == Cause::Auth && cause_of("HTTP 403") == Cause::Auth);
    CHECK(cause_of("HTTP 404") == Cause::Gone && cause_of("HTTP 410") == Cause::Gone);
    CHECK(cause_of("HTTP 416") == Cause::Other);
    CHECK(cause_of("SSL: public key does not match pinned public key") == Cause::Certificate);
    CHECK(cause_of("SSL certificate problem: self-signed certificate") == Cause::Certificate);
    // What the install says.
    CHECK(cause_of("not enough free space") == Cause::NoSpace);
    CHECK(cause_of("0123.nca: the content's SHA-256 differs from the content meta's") == Cause::Corrupt);
    CHECK(cause_of("the NCZ's zstd stream cut short") == Cause::Corrupt);
    CHECK(cause_of("a.nca: not signed by Nintendo", true) == Cause::Unsigned);
    CHECK(cause_of("no content meta in the package") == Cause::Other);
    CHECK(cause_of("") == Cause::Other);
    CHECK(cause_of(SERVER_GONE) == Cause::ServerGone);
    CHECK(std::string(cause_name(Cause::NoSpace)) == "no_space" && std::string(cause_name(Cause::Other)) == "other");
}

void test_retries()
{
    // A cut, a busy server: 10 s, 30 s, 2 min, then the user's call.
    CHECK(retry_delay(Cause::Network, 0) == 10 && retry_delay(Cause::Network, 1) == 30 && retry_delay(Cause::Network, 2) == 120);
    CHECK(!retry_delay(Cause::Network, RETRIES));
    CHECK(retry_delay(Cause::Server, 0) == 10);
    // What needs the user.
    for (Cause c : { Cause::Corrupt, Cause::Auth, Cause::Gone, Cause::Certificate, Cause::NoSpace, Cause::Unsigned, Cause::ServerGone, Cause::Other })
        CHECK(!retry_delay(c, 0));
}

void test_remedies()
{
    Situation known;
    known.server_known = true;
    Situation both = known;
    both.other_address = true;
    CHECK(offers(remedies(Cause::Network, both), Remedy::OtherAddress) && offers(remedies(Cause::Network, both), Remedy::Retry));
    CHECK(!offers(remedies(Cause::Network, known), Remedy::OtherAddress));
    CHECK(remedies(Cause::Auth, known).front() == Remedy::EditServer);
    CHECK(remedies(Cause::Certificate, known).front() == Remedy::EditServer);
    CHECK(!offers(remedies(Cause::Auth, Situation{}), Remedy::EditServer));
    CHECK(remedies(Cause::Gone, known).front() == Remedy::FindAgain);
    CHECK(!offers(remedies(Cause::Gone, Situation{}), Remedy::FindAgain));
    CHECK(remedies(Cause::Corrupt, known) == std::vector<Remedy>{ Remedy::Retry });
    CHECK(remedies(Cause::Unsigned, known) == std::vector<Remedy>{ Remedy::InstallAnyway });
    CHECK(remedies(Cause::ServerGone, known).empty());   // only removed
    CHECK(remedies(Cause::NoSpace, known) == std::vector<Remedy>{ Remedy::Retry });
}

void test_rebase()
{
    const std::string lan = "https://192.168.1.20:8443", wan = "https://shelf.example.org/library/";
    CHECK(rebase("https://192.168.1.20:8443/api/get_game/12", lan, wan) == "https://shelf.example.org/library/api/get_game/12");
    CHECK(rebase("https://shelf.example.org/library/api/get_game/12", wan, lan) == "https://192.168.1.20:8443/api/get_game/12");
    // The host's case does not matter; another host is not moved.
    CHECK(rebase("https://SHELF.example.org/library/x.nsp", wan, lan) == "https://192.168.1.20:8443/x.nsp");
    CHECK(rebase("https://other.example/x.nsp", wan, lan).empty());
    // Not under the address's path, on its host: the origin swapped.
    CHECK(rebase("https://shelf.example.org/files/x.nsp", wan, lan) == "https://192.168.1.20:8443/files/x.nsp");
    // A prefix of the host is not the host.
    CHECK(rebase("https://192.168.1.200:8443/x.nsp", "https://192.168.1.20", wan).empty());
    CHECK(rebase("https://a.example/x", "", wan).empty() && rebase("https://a.example/x", "https://a.example", "").empty());
}

void test_installed()
{
    Job j;
    j.title_id = 0x0100000000010800ull;
    j.version = 131072;
    std::map<uint64_t, uint32_t> have;
    CHECK(!installed_already(j, have));
    have[j.title_id] = 65536;
    CHECK(!installed_already(j, have));
    have[j.title_id] = 131072;
    CHECK(installed_already(j, have));
    have[j.title_id] = 196608;
    CHECK(installed_already(j, have));
    // A base game: the title at all.
    Job base;
    base.title_id = 0x0100000000010000ull;
    CHECK(!installed_already(base, have));
    have[base.title_id] = 0;
    CHECK(installed_already(base, have));
    // No title id known: never.
    CHECK(!installed_already(Job{}, have));
}

void test_find_again()
{
    std::vector<library::Item> items(4);
    items[0].title_id = 0x0100000000010000ull;
    items[0].url = "https://s/new/base.nsp";
    items[1].title_id = 0x0100000000010800ull;
    items[1].version = 65536;
    items[1].has_version = true;
    items[1].url = "https://s/new/u1.nsp";
    items[2].title_id = 0x0100000000010800ull;
    items[2].version = 131072;
    items[2].has_version = true;
    items[2].url = "https://s/new/u2.nsp";
    items[3].title_id = 0x0100000000010800ull;   // no address
    items[3].version = 196608;
    items[3].has_version = true;
    Job update;
    update.title_id = 0x0100000000010800ull;
    update.version = 131072;
    CHECK(find_again(update, items) == 2);
    update.version = 196608;
    CHECK(find_again(update, items) == -1);
    Job base;
    base.title_id = 0x0100000000010000ull;
    CHECK(find_again(base, items) == 0);
    CHECK(find_again(Job{}, items) == -1);
}
}   // namespace

int main()
{
    test_causes();
    test_retries();
    test_remedies();
    test_rebase();
    test_installed();
    test_find_again();
    return CHECK_DONE("download recovery: causes, own tries, remedies, the other address, installed already, found again passed");
}
