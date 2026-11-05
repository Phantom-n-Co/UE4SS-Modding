#pragma once
// Pure helpers for nameable teleporter pads (no Unreal dependency, unit-tested).
//
// A custom name maps to a stable frequency id >= kCustomIdBase, so vanilla's
// int-based pad linking (DynamicProperty TeleporterFrequency -> "teleporterpad<id>")
// links every pad with the same name. Tags use ids 0..25 and never collide.
#include <cstdint>
#include <string>

namespace teleporter_names {

constexpr int32_t kCustomIdBase = 1000;
constexpr size_t kMaxNameLength = 32;

inline wchar_t asciiLower(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
}

// Trims whitespace, drops control characters, caps the length.
inline std::wstring normalizeName(const std::wstring& raw) {
    std::wstring out;
    for (wchar_t c : raw) {
        if (c >= 0x20 && c != 0x7f) {
            out.push_back(c);
        }
    }
    const auto b = out.find_first_not_of(L' ');
    if (b == std::wstring::npos) {
        return {};
    }
    out = out.substr(b, out.find_last_not_of(L' ') - b + 1);
    if (out.size() > kMaxNameLength) {
        out.resize(kMaxNameLength);
        out.erase(out.find_last_not_of(L' ') + 1);
    }
    return out;
}

inline bool equalsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (asciiLower(a[i]) != asciiLower(b[i])) {
            return false;
        }
    }
    return true;
}

// FNV-1a over the ASCII-lowercased name: case-insensitive, identical on every
// machine (no locale), always in [kCustomIdBase, kCustomIdBase + 2^30).
inline int32_t nameToId(const std::wstring& name) {
    uint32_t h = 2166136261u;
    for (wchar_t c : name) {
        const uint32_t v = static_cast<uint32_t>(asciiLower(c)) & 0xffffu;
        h = (h ^ (v & 0xffu)) * 16777619u;
        h = (h ^ (v >> 8)) * 16777619u;
    }
    return kCustomIdBase + static_cast<int32_t>(h & 0x3fffffffu);
}

inline bool isCustomId(int32_t id) {
    return id >= kCustomIdBase;
}

// RPC payload "<id>:<name>". Vanilla's string-to-int stops at ':', so a host
// without the mod still links the pad by name (it just can't show the name).
inline std::wstring encodeNameMessage(const std::wstring& name) {
    return std::to_wstring(nameToId(name)) + L":" + name;
}

inline bool decodeNameMessage(const std::wstring& msg, std::wstring& name) {
    const auto colon = msg.find(L':');
    if (colon == std::wstring::npos || colon == 0) {
        return false;
    }
    for (size_t i = 0; i < colon; ++i) {
        if (msg[i] < L'0' || msg[i] > L'9') {
            return false;
        }
    }
    name = normalizeName(msg.substr(colon + 1));
    return !name.empty();
}

// Plain vanilla payload: an optionally signed decimal int32.
inline bool parsePlainId(const std::wstring& msg, int32_t& id) {
    size_t i = (!msg.empty() && msg[0] == L'-') ? 1 : 0;
    if (i >= msg.size() || msg.size() - i > 10) {
        return false;
    }
    int64_t v = 0;
    for (; i < msg.size(); ++i) {
        if (msg[i] < L'0' || msg[i] > L'9') {
            return false;
        }
        v = v * 10 + (msg[i] - L'0');
    }
    if (msg[0] == L'-') {
        v = -v;
    }
    if (v < INT32_MIN || v > INT32_MAX) {
        return false;
    }
    id = static_cast<int32_t>(v);
    return true;
}

} // namespace teleporter_names
