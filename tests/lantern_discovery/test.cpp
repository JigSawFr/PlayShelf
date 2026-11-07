// source/lantern_app/discovery.cpp over a socket that answers as servers would:
// found once by uid, named safely, the address of the answer.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>
#include <deque>

#include "check.h"
#include "lantern_app/discovery.hpp"

namespace
{
struct Answering final : lantern::UdpSocket
{
    std::deque<std::pair<std::string, std::string>> replies;   // address, JSON
    std::vector<std::string>                        sent;

    lantern::Result<void> send_to(std::string_view address, std::uint16_t port, std::span<const std::byte>) override
    {
        sent.push_back(std::string(address) + ":" + std::to_string(port));
        return {};
    }
    lantern::Result<std::optional<lantern::Datagram>> receive(std::span<std::byte> buffer, std::chrono::milliseconds) override
    {
        if (replies.empty()) return std::optional<lantern::Datagram>{};
        const auto [address, json] = replies.front();
        replies.pop_front();
        std::memcpy(buffer.data(), json.data(), json.size());
        lantern::Datagram d;
        d.size         = json.size();
        d.address_size = address.size();
        std::memcpy(d.address.data(), address.data(), address.size());
        d.port = 47115;
        return std::optional<lantern::Datagram>{d};
    }
};

std::string reply(const char* uid, const char* name, int port)
{
    return std::string(R"({"schema":1,"lantern":"server","uid":")") + uid + R"(","name":")" + name + R"(","port":)" + std::to_string(port)
           + R"(,"base":"/api/lantern","versions":[1]})";
}
}   // namespace

int main()
{
    Answering s;
    s.replies = {
        {"192.168.1.5", reply("6f1c2a8e-4b7d-4e2a-9c31-8d5e2f0a7b14", "Living room", 8443)},
        {"192.168.1.9", reply("11111111-4b7d-4e2a-9c31-8d5e2f0a7b14", "Attic", 9443)},
        {"192.168.1.6", reply("6f1c2a8e-4b7d-4e2a-9c31-8d5e2f0a7b14", "Living room, again", 8443)},
        {"192.168.1.7", "not json"},
    };
    const auto found = lantern_app::discover(s);
    CHECK(!s.sent.empty() && s.sent[0] == "255.255.255.255:47115");
    CHECK(found.size() == 2);
    if (found.size() == 2) {
        CHECK(found[0].url == "https://192.168.1.5:8443" && found[0].name == "Living room");   // the first answer of a uid
        CHECK(found[1].url == "https://192.168.1.9:9443" && found[1].name == "Attic");
    }
    CHECK_DONE("lantern discovery: servers found once by uid, junk ignored passed");
}
