// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

// -----------------------------------------------------------------------------
// File: string_utils.h
// Description: Utility string functions used across C++ embedded projects.
// Namespace: 'stru'
// Author: Alex Conesa, alexconesap@gmail.com
// Version: 1.1 - March 3, 2026
// C++ version: C++17
// -----------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <algorithm>
#include <cstring>
#include <string>

#include "string_types.h"

namespace stru {

    /// @brief Skip leading whitespace in a C-style string.
    /// @param pstr Pointer to the C-style string.
    /// @return Pointer to the first non-whitespace character in the string.
    inline const char* skipWhitespace(const char* pstr) {
        while (*pstr != '\0' && std::isspace(*pstr)) {
            ++pstr;
        }
        return pstr;
    }

    /// @brief Skip leading whitespace in a string_view.
    /// @param sv The string_view to skip whitespace in.
    /// @return A new string_view starting from the first non-whitespace character.
    inline string_view_t skipWhitespaceView(string_view_t string) {
        size_t start = 0;
        while (start < string.size() && std::isspace(static_cast<unsigned char>(string[start]))) {
            ++start;
        }
        return string.substr(start);
    }

    /// @brief Trim leading and trailing whitespace from a C-style string.
    /// @param buf Pointer to the C-style string buffer.
    /// @param len Length of the string in the buffer.
    inline void trimWhitespace(char* buf, size_t& len) {
        size_t start = 0;
        while (start < len && std::isspace(static_cast<unsigned char>(buf[start]))) {
            start++;
        }
        size_t end = len;
        while (end > start && std::isspace(static_cast<unsigned char>(buf[end - 1]))) {
            end--;
        }
        if (start > 0 || end < len) {
            memmove(buf, buf + start, end - start);
            len = end - start;
        }
    }

    /// @brief Trim leading and trailing whitespace from a string_view.
    /// @param str The string_view to trim.
    /// @return A new string_view with leading and trailing whitespace removed.
    inline string_view_t trimWhitespace(string_view_t str) {
        size_t curr = 0, e = str.size();
        while (curr < e && std::isspace(static_cast<unsigned char>(str[curr]))) {
            ++curr;
        }
        while (e > curr && std::isspace(static_cast<unsigned char>(str[e - 1]))) {
            --e;
        }
        return str.substr(curr, e - curr);
    }

    /// @brief Check if a string starts with a given C-style string prefix.
    /// @param str The string to check.
    /// @param prefix The C-style string prefix to check for. Empty prefix is always a match.
    /// @return True if the string starts with the prefix, false otherwise.
    inline bool startsWith(const string_t& str, const char* prefix) {
        size_t len = std::strlen(prefix);
        return str.size() >= len && std::memcmp(str.data(), prefix, len) == 0;
    }

    /// @brief Check if a string starts with a given string prefix.
    /// @param str The string to check.
    /// @param prefix The string prefix to check for. Empty prefix is always a match.
    /// @return True if the string starts with the prefix, false otherwise.
    inline bool startsWith(const string_t& str, const string_t& prefix) {
        //
        return str.rfind(prefix, 0) == 0;
    }

    /// @brief Check if a string starts with a given string prefix.
    /// @param str The string to check.
    /// @param prefix The string prefix to check for. Empty prefix is always a match.
    /// @return True if the string starts with the prefix, false otherwise.
    inline bool startsWith(const string_view_t& str, const string_view_t& prefix) {
        //
        return str.rfind(prefix, 0) == 0;
    }

    /// @brief Replace all occurrences of `pattern` in `text_in_out` with `replacement`. Same
    /// behavior as Javascript's function.
    /// @param text_in_out The string to modify.
    /// @param pattern The substring to search for in `text_in_out`.
    /// @param replacement The substring to replace `pattern` with.
    /// @return The number of replacements made.
    /// @note - If `pattern` is empty, it will insert `replacement` before every character and at
    /// the end.
    /// @note - If `text_in_out` is empty, it will set `text_in_out` to `replacement` and return 1.
    /// @note - If `pattern` is not found in `text_in_out`, it will return 0 and leave `text_in_out`
    /// unchanged.
    /// @note This function modifies `text_in_out` in place.
    /// @note If `replacement` is empty, it will remove all occurrences of `pattern` from
    /// `text_in_out`.
    inline int replaceAll(string_t& text_in_out, const string_view_t pattern,
                          const string_view_t replacement) {
        if (pattern.empty()) {
            if (replacement.empty()) {
                return 0;
            }

            // If text_in_out is empty, just set it to replacement. One replacement.
            if (text_in_out.empty()) {
                text_in_out = replacement;
                return 1;
            }

            // “X” → “<R>X<R>”
            const size_t size = text_in_out.size();
            string_t out;
            out.reserve(size + (size + 1) * replacement.size());
            for (size_t i = 0; i < size; ++i) {
                out.append(replacement);
                out.push_back(text_in_out[i]);
            }
            out.append(replacement);
            text_in_out.swap(out);
            return static_cast<int>(size + 1);
        }

        int count = 0;
        size_t pos = 0;
        while ((pos = text_in_out.find(pattern, pos)) != string_t::npos) {
            text_in_out.replace(pos, pattern.length(), replacement);
            pos += replacement.length();
            ++count;
        }
        return count;
    }

    inline int replaceAll(string_t& str, const char* pattern, const char* replacement) {
        return replaceAll(str, string_view_t(pattern, std::strlen(pattern)),
                          string_view_t(replacement, std::strlen(replacement)));
    }
    inline int replaceAll(string_t& str, const char* pattern, string_view_t replacement) {
        return replaceAll(str, string_view_t(pattern, std::strlen(pattern)), replacement);
    }
    inline int replaceAll(string_t& str, string_view_t pattern, const char* replacement) {
        return replaceAll(str, pattern, string_view_t(replacement, std::strlen(replacement)));
    }

    /// @brief Escape special characters in a string.
    /// @param text_in_out The text_in_out string to escape.
    /// @details This function escapes double quotes, backslashes, newlines, carriage returns, and
    /// tabs.
    /// @return The escaped string.
    inline string_t escapeString(const string_t& text_in_out) {
        string_t output;
        output.reserve(text_in_out.size() + 8);

        for (char c : text_in_out) {
            switch (c) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    output += c;
            }
        }
        return output;
    }

    inline size_t countChar(const string_t& string, char chr);
    constexpr size_t countChar(const char* pstr, char chr);

    /// @brief Count the number of occurrences of character c in the string or C-style string.
    /// @param pstr C-style string pointer.
    /// @param chr Character to count occurrences of.
    /// @return The number of occurrences of character c in the string.
    constexpr size_t countChar(const char* pstr, char chr) {
        size_t cnt = 0;
        while (*pstr != '\0') {
            cnt += (*pstr++ == chr);
        }
        return cnt;
    }

    /// @brief Count the number of occurrences of character c in the string.
    /// @param string The string to search in.
    /// @param chr Character to count occurrences of.
    /// @return The number of occurrences of character c in the string.
    inline size_t countChar(const string_t& string, char chr) {
        return countChar(string.c_str(), chr);
    }

    /// @brief Count the number of occurrences of character c in a string_view.
    /// @param token The string_view to search in.
    /// @param chr Character to count occurrences of.
    /// @return The number of occurrences of character c in the string_view.
    inline size_t countChar(const string_view_t& token, char chr) {
        return std::count(token.begin(), token.end(), chr);
    }

    /// @brief Count the number of tokens in a string separated by 'c' (ignores empty values and
    /// spaces)
    /// @param str The string to search in. If empty returns 0. If not empty but no delimiter is
    /// found, returns 1.
    /// @param c Character to use as a delimiter
    /// @return The number of tokens in the string.
    /// Example:
    /// \verbatim
    /// ```text
    /// countTokensByChar("a,b,c", ',') // returns 3
    /// countTokensByChar("a   ,   b,c", ',') // returns 3
    /// countTokensByChar("x, ,c", ',') // returns 2 (ignores empty tokens)
    /// countTokensByChar("a,,c", ',') // returns 2 (ignores empty tokens)
    /// countTokensByChar("a", ',') // returns 1 (no delimiter found, but not empty)
    /// countTokensByChar("", ',') // returns 0 (empty string)
    /// countTokensByChar(",", ',') // returns 0 (like an empty string)
    /// \endverbatim
    /// ```
    inline int countTokensByChar(const char* str, char chr) {
        if (str == nullptr || *str == '\0') {
            return 0;
        }

        int count = 0;
        bool inToken = false;
        while (*str != '\0') {
            if (*str == chr) {
                if (inToken) {
                    ++count;
                    inToken = false;
                }
            } else if (!std::isspace(*str)) {
                inToken = true;
            }
            ++str;
        }
        return (inToken ? ++count : count);
    }

    inline size_t countTokensByChar(const string_t& str, char c) {
        return countTokensByChar(str.c_str(), c);
    }

    /// string_indexOf: returns first position of char c after `start`, or -1 if not found
    inline int string_indexOf(const string_t& string, char chr, int start = 0) {
        const size_t pos = string.find(chr, static_cast<size_t>(start));
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    /// substring: Return substring from start to end (exclusive)
    inline string_t string_substring(const string_t& str, int start, int end = -1) {
        if (start < 0) {
            start = 0;
        }
        if (end == -1) {
            return str.substr(start);
        }
        return str.substr(start, end - start);
    }

    /// @brief Remove leading and trailing whitespace from the string
    /// @param str_in_out The string to trim. This string will be modified in place.
    inline void trim(string_t& str_in_out) {
        // Trim leading spaces
        const size_t start = str_in_out.find_first_not_of(" \t\n\r");
        if (start == string_t::npos) {
            str_in_out.clear();
            return;
        }
        // Trim trailing spaces
        const size_t end = str_in_out.find_last_not_of(" \t\n\r");
        str_in_out = str_in_out.substr(start, end - start + 1);
    }

    /// @brief Trim a string and return a new copy.
    /// @param str The string to trim.
    /// @return Trimmed string_t
    inline string_t as_trim(const string_t& str) {
        string_t result = str;
        trim(result);
        return result;
    }

    /// string_equals: Compare two strings for equality
    inline bool string_equals(const string_t& str1, const string_t& str2) {
        return str1 == str2;
    }

    /// Helper for number-to-string conversion, safe and lightweight to avoid using std::to_string
    /// @brief Convert a number to a string and store it in the provided buffer.
    /// @tparam T The type of the number (int, unsigned int, long, unsigned long, float, double).
    /// @param num The number to convert.
    /// @param buf A character buffer to store the resulting string. Must be at least 32 bytes long
    /// for a 64-bit number.
    /// @param buf_size The size of the buffer.
    template <typename T>
    void num_to_stringf(T num, char* buf, size_t buf_size) {
        if constexpr (std::is_same<T, int>::value) {
            snprintf(buf, buf_size, "%d", num);
        } else if constexpr (std::is_same<T, unsigned int>::value) {
            snprintf(buf, buf_size, "%u", num);
        } else if constexpr (std::is_same<T, int64_t>::value) {
            snprintf(buf, buf_size, "%ld", num);
        } else if constexpr (std::is_same<T, uint64_t>::value) {
            snprintf(buf, buf_size, "%lu", num);
        } else if constexpr (std::is_same<T, float>::value) {
            snprintf(buf, buf_size, "%.6f", num);
        } else if constexpr (std::is_same<T, double>::value) {
            snprintf(buf, buf_size, "%.6lf", num);
        } else {
            snprintf(buf, buf_size, "%lld", static_cast<int64_t>(num));
        }
    }

    template <typename T>
    string_t num_to_string(T num) {
        char buf[32];
        num_to_stringf(num, buf, sizeof(buf));
        return {buf};
    }
    /// to_lower: Convert the given string to lowercase
    inline void to_lower(string_t& string) {
        for (auto& c : string) {
            const auto uc = static_cast<unsigned char>(c);
            c = static_cast<char>(std::tolower(uc));
        }
    }

    /// @brief Convert a string to lowercase and return a new string.
    /// @param str The string to convert.
    /// @return The lowercase version of the input string.
    inline string_t as_lower(const string_t& str) {
        string_t out = str;
        to_lower(out);
        return out;
    }

    /// to_upper: Convert the given string to uppercase
    inline void to_upper(string_t& str) {
        for (auto& c : str) {
            const auto uc = static_cast<unsigned char>(c);
            c = static_cast<char>(std::toupper(uc));
        }
    }

    /// @brief Convert a string to uppercase and return a new string.
    /// @param str The string to convert.
    /// @return The uppercase version of the input string.
    inline string_t as_upper(const string_t& str) {
        string_t out = str;
        to_upper(out);
        return out;
    }

    /// @brief Given a string, this function splits it into tokens based on a delimiter character.
    /// @param str The string_view_t to tokenize.
    /// @note Spaces between delimiters are considered tokens.
    /// @param delimiter The character used to split the string into tokens.
    /// @return A vector of string_view_t tokens.
    inline vector_string_t tokenizeByDelimiter(const string_t& str, char delimiter) {
        if (str.empty()) {
            return {};
        }

        vector_string_t tokens;
        tokens.reserve(countChar(str, delimiter) + 1);

        size_t start = 0, end;
        while ((end = str.find(delimiter, start)) != string_view_t::npos) {
            if (end > start) {
                tokens.emplace_back(str.substr(start, end - start));
            }
            start = end + 1;
        }
        if (start < str.size()) {
            tokens.emplace_back(str.data() + start, str.size() - start);
        }
        return tokens;
    }

    /// @brief Given a string with values separated by a delimiter, this function:
    ///   1) Splits on that delimiter
    ///   2) Trims leading & trailing spaces on *each* token
    ///   3) Discards any empty tokens
    ///   4) Joins back with the same delimiter
    /// \verbatim
    /// @param text_in_out     the raw text_in_out (e.g. "  a   ,   b qq , , c ")
    /// @param delimiter the character to split on (default ',')
    /// @return          cleaned string (e.g. "a,b qq,c")
    /// \endverbatim
    inline string_t cleanDelimitedValues(const string_t& text_in_out, char delimiter = ',') {
        if (text_in_out.empty()) {
            return {};
        }

        const auto tokens = tokenizeByDelimiter(text_in_out, delimiter);
        if (tokens.empty()) {
            return {};
        }

        string_t result;
        result.reserve(text_in_out.size());

        bool first = true;
        for (const auto& sv : tokens) {
            auto trimmed = trimWhitespace(sv);
            if (trimmed.empty())
                continue;
            if (!first)
                result.push_back(delimiter);
            result.append(trimmed.data(), trimmed.size());
            first = false;
        }
        return result;
    }

    // When not available Arduino.h: we must define map()
    // inline uint16_t map(uint8_t x, uint8_t in_min, uint8_t in_max, uint16_t out_min, uint16_t
    // out_max) {
    //     return (uint32_t(x - in_min) * (out_max - out_min)) / (in_max - in_min) + out_min;
    // }

}  // namespace stru