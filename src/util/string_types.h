// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

// -----------------------------------------------------------------------------
// File: string_types.h
// Description: Type aliases for string and vector types used across the project.
// Author: Alex Conesa, alexconesap@gmail.com
// Version: 2.0 - April 2026 — moved into ungula:: namespace.
// C++ version: C++17
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ungula {

    using string_t = std::string;
    using string_vector_t = std::vector<string_t>;
    using string_view_t = std::string_view;
    using vector_string_t = std::vector<string_t>;
    using vector_string_view_t = std::vector<string_view_t>;

}  // namespace ungula
