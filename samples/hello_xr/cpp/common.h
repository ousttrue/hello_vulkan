// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <locale>
#include <algorithm>
#include <memory>
#include <stddef.h>

struct IgnoreCaseStringLess {
    bool operator()(const std::string& a, const std::string& b, const std::locale& loc = std::locale()) const noexcept {
        const std::ctype<char>& ctype = std::use_facet<std::ctype<char>>(loc);
        const auto ignoreCaseCharLess = [&](char c1, char c2) { return ctype.tolower(c1) < ctype.tolower(c2); };
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), ignoreCaseCharLess);
    }
};

template <typename T>
struct ScopeGuard {
    // Needs C++17: static_assert(std::is_invocable_v<T>, "Type must be invocable function.");

    ScopeGuard(T&& guard) noexcept : m_guard(std::move(guard)) {}

    ScopeGuard(ScopeGuard&&) noexcept = default;
    ScopeGuard& operator=(ScopeGuard&&) noexcept = default;

    ScopeGuard(ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard&) = delete;

    ~ScopeGuard() { m_guard(); }

   private:
    T m_guard;
};

// Usage: auto guard = MakeScopeGuard([&] { foobar; });
template <typename T>
ScopeGuard<T> MakeScopeGuard(T&& guard) {
    return ScopeGuard<T>(std::forward<T>(guard));
}



// The equivalent of C++17 std::size. A helper to get the dimension for an array.
template <typename T, size_t Size>
constexpr size_t ArraySize(const T (&unused)[Size]) noexcept {
    (void)unused;
    return Size;
}

#include "logger.h"
#include "check.h"
