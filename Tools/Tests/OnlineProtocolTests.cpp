#include "Engine/Network/OnlineProtocol.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace OnlineProtocol;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Reject(F function) {
    bool rejected = false;
    try { function(); } catch (...) { rejected = true; }
    Check(rejected, "Invalid network data was accepted");
}
int main() {
    Input pending{};
    Accumulate(pending, {1, true, false, 2, -2});
    Accumulate(pending, {0, false, true, 2, -2});
    Check(pending.move == 0 && pending.jump && pending.shape, "Triggers were lost between simulation ticks");
    Check(pending.pullX == 2.5f && pending.pullY == -2.5f, "Pull was not bounded");
    auto decoded = DecodeInput(EncodeInput(pending));
    Check(decoded.jump && decoded.shape && decoded.pullX == pending.pullX, "Input roundtrip failed");
    pending.move = -1;
    ConsumeTransient(pending);
    Check(pending.move == -1 && !pending.jump && !pending.shape && pending.pullX == 0 && pending.pullY == 0, "Triggers repeated on subsequent ticks");
    auto frame = Message("frame", "match-a");
    frame["tick"] = uint64_t{18446744073709551614ull};
    frame["state"] = uint64_t{18446744073709551615ull};
    frame["inputs"] = nlohmann::json::array({EncodeInput(decoded), EncodeInput(decoded)});
    const auto bytes = Encode(frame);
    Check(bytes.size() <= MaxPacket && Decode(bytes, "match-a") == frame, "Two-player frame roundtrip failed");
    frame["inputs"].push_back(EncodeInput(decoded));
    Check(Decode(Encode(frame), "match-a") == frame, "Three-player frame roundtrip failed");
    Reject([&] { Decode(bytes, "old-match"); });
    frame["v"] = Version + 1;
    Reject([&] { Decode(Encode(frame), "match-a"); });
    Reject([] { Decode({}, "match-a"); });
    Reject([] { Decode(std::vector<uint8_t>(MaxPacket + 1), "match-a"); });
    Reject([] { Decode({0xbf, 0x64, 0x61}, "match-a"); });
    Reject([] { Encode(nlohmann::json(std::string(MaxPacket + 1, 'x'))); });
    Reject([] { DecodeInput(nlohmann::json::array({1, true})); });
    Reject([] { DecodeInput(nlohmann::json::array({2, true, false, 0, 0})); });
    Reject([] { DecodeInput(nlohmann::json::array({0, 1, false, 0, 0})); });
    Reject([] { DecodeInput(nlohmann::json::array({0, true, false, 2.6f, 0})); });
    Reject([] { DecodeInput(nlohmann::json::array({std::numeric_limits<float>::infinity(), true, false, 0, 0})); });
    Reject([] { DecodeInput(nlohmann::json::array({0, true, false, std::numeric_limits<float>::quiet_NaN(), 0})); });
    std::cout << "Online protocol tests passed (roundtrip, 2-3 players, input accumulation, stale/malformed/out-of-range packets).\n";
}
