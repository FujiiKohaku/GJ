#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "externals/json.hpp"

namespace OnlineProtocol {
constexpr int Version = 1;
constexpr float Step = 1.0f / 30.0f;
constexpr size_t MaxPacket = 1170;
struct Input {
    float move = 0;
    bool jump = false;
    bool shape = false;
    float pullX = 0;
    float pullY = 0;
};
inline void Accumulate(Input& pending, const Input& sample) {
    pending.move = sample.move;
    pending.jump |= sample.jump;
    pending.shape |= sample.shape;
    pending.pullX = std::clamp(pending.pullX + sample.pullX, -2.5f, 2.5f);
    pending.pullY = std::clamp(pending.pullY + sample.pullY, -2.5f, 2.5f);
}
inline void ConsumeTransient(Input& input) {
    input.jump = input.shape = false;
    input.pullX = input.pullY = 0;
}
inline nlohmann::json EncodeInput(const Input& i) {
    return nlohmann::json::array({i.move, i.jump, i.shape, i.pullX, i.pullY});
}
inline Input DecodeInput(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 5 || !j[1].is_boolean() || !j[2].is_boolean())
        throw std::runtime_error("Invalid input");
    Input i{ j[0].get<float>(), j[1].get<bool>(), j[2].get<bool>(), j[3].get<float>(), j[4].get<float>() };
    if (!std::isfinite(i.move) || !std::isfinite(i.pullX) || !std::isfinite(i.pullY) ||
        std::abs(i.move) > 1 || std::abs(i.pullX) > 2.5f || std::abs(i.pullY) > 2.5f)
        throw std::runtime_error("Input out of range");
    return i;
}
inline std::vector<uint8_t> Encode(const nlohmann::json& j) {
    auto bytes = nlohmann::json::to_cbor(j);
    if (bytes.size() > MaxPacket) throw std::runtime_error("Packet too large");
    return bytes;
}
inline nlohmann::json Decode(const std::vector<uint8_t>& bytes, const std::string& match) {
    if (bytes.empty() || bytes.size() > MaxPacket) throw std::runtime_error("Invalid packet size");
    auto j = nlohmann::json::from_cbor(bytes);
    if (!j.is_object() || j.at("v") != Version || j.at("match") != match)
        throw std::runtime_error("Wrong protocol or match");
    return j;
}
inline nlohmann::json Message(const std::string& type, const std::string& match) {
    return {{"v", Version}, {"match", match}, {"type", type}};
}
}
