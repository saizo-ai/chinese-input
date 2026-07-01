// Process-wide singleton around ime::Engine plus UTF conversion helpers.
#pragma once

#define NOMINMAX
#include <windows.h>

#include <string>

#include "ime/engine.hpp"

// Returns the shared engine, creating it on first use (thread-safe).
// May return nullptr if the dictionary cannot be located.
ime::Engine* GetEngine();

std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
