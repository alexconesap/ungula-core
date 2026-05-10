#include <gtest/gtest.h>
#include <ungula/core/util/string_utils.h>

// --- Trim ---

TEST(StringUtils, TrimLeadingAndTrailing)
{
    std::string s = "  hello  ";
    ungula::core::util::str::trim(s);
    EXPECT_EQ(s, "hello");
}

TEST(StringUtils, TrimEmptyString)
{
    std::string s = "   ";
    ungula::core::util::str::trim(s);
    EXPECT_EQ(s, "");
}

TEST(StringUtils, TrimNoWhitespace)
{
    std::string s = "hello";
    ungula::core::util::str::trim(s);
    EXPECT_EQ(s, "hello");
}

TEST(StringUtils, AsTrimReturnsNewString)
{
    std::string s = "  hello  ";
    std::string trimmed = ungula::core::util::str::as_trim(s);
    EXPECT_EQ(trimmed, "hello");
    EXPECT_EQ(s, "  hello  "); // Original unchanged
}

// --- Case ---

TEST(StringUtils, ToLower)
{
    std::string s = "Hello WORLD";
    ungula::core::util::str::to_lower(s);
    EXPECT_EQ(s, "hello world");
}

TEST(StringUtils, ToUpper)
{
    std::string s = "Hello world";
    ungula::core::util::str::to_upper(s);
    EXPECT_EQ(s, "HELLO WORLD");
}

TEST(StringUtils, AsLowerReturnsNewString)
{
    EXPECT_EQ(ungula::core::util::str::as_lower("ABC"), "abc");
}

TEST(StringUtils, AsUpperReturnsNewString)
{
    EXPECT_EQ(ungula::core::util::str::as_upper("abc"), "ABC");
}

// --- StartsWith ---

TEST(StringUtils, StartsWithCString)
{
    std::string s = "hello world";
    EXPECT_TRUE(ungula::core::util::str::startsWith(s, "hello"));
    EXPECT_FALSE(ungula::core::util::str::startsWith(s, "world"));
}

TEST(StringUtils, StartsWithEmptyPrefix)
{
    std::string s = "hello";
    EXPECT_TRUE(ungula::core::util::str::startsWith(s, ""));
}

TEST(StringUtils, StartsWithString)
{
    std::string s = "hello world";
    std::string prefix = "hello";
    EXPECT_TRUE(ungula::core::util::str::startsWith(s, prefix));
}

// --- ReplaceAll ---

TEST(StringUtils, ReplaceAllBasic)
{
    std::string s = "aabaa";
    int count = ungula::core::util::str::replaceAll(s, "a", "X");
    EXPECT_EQ(s, "XXbXX");
    EXPECT_EQ(count, 4);
}

TEST(StringUtils, ReplaceAllNoMatch)
{
    std::string s = "hello";
    int count = ungula::core::util::str::replaceAll(s, "xyz", "Q");
    EXPECT_EQ(s, "hello");
    EXPECT_EQ(count, 0);
}

TEST(StringUtils, ReplaceAllRemoval)
{
    std::string s = "a-b-c";
    ungula::core::util::str::replaceAll(s, "-", "");
    EXPECT_EQ(s, "abc");
}

TEST(StringUtils, ReplaceAllEmptyPattern)
{
    std::string s = "AB";
    int count = ungula::core::util::str::replaceAll(s, "", "-");
    EXPECT_EQ(s, "-A-B-");
    EXPECT_EQ(count, 3);
}

// --- EscapeString ---

TEST(StringUtils, EscapeSpecialChars)
{
    std::string result = ungula::core::util::str::escapeString("a\"b\\c\nd\re\tf");
    EXPECT_EQ(result, "a\\\"b\\\\c\\nd\\re\\tf");
}

TEST(StringUtils, EscapeNoSpecialChars)
{
    EXPECT_EQ(ungula::core::util::str::escapeString("hello"), "hello");
}

// --- CountChar ---

TEST(StringUtils, CountCharBasic)
{
    EXPECT_EQ(ungula::core::util::str::countChar("hello", 'l'), 2u);
    EXPECT_EQ(ungula::core::util::str::countChar("hello", 'z'), 0u);
}

TEST(StringUtils, CountCharString)
{
    std::string s = "abcabc";
    EXPECT_EQ(ungula::core::util::str::countChar(s, 'a'), 2u);
}

// --- CountTokensByChar ---

TEST(StringUtils, CountTokensBasic)
{
    EXPECT_EQ(ungula::core::util::str::countTokensByChar("a,b,c", ','), 3);
}

TEST(StringUtils, CountTokensWithSpaces)
{
    EXPECT_EQ(ungula::core::util::str::countTokensByChar("a   ,   b,c", ','), 3);
}

TEST(StringUtils, CountTokensEmptyTokens)
{
    EXPECT_EQ(ungula::core::util::str::countTokensByChar("a,,c", ','), 2);
}

TEST(StringUtils, CountTokensEmpty)
{
    EXPECT_EQ(ungula::core::util::str::countTokensByChar("", ','), 0);
}

TEST(StringUtils, CountTokensOnlyDelimiter)
{
    EXPECT_EQ(ungula::core::util::str::countTokensByChar(",", ','), 0);
}

TEST(StringUtils, CountTokensNoDelimiter)
{
    EXPECT_EQ(ungula::core::util::str::countTokensByChar("abc", ','), 1);
}

// --- Tokenize ---

TEST(StringUtils, TokenizeByDelimiter)
{
    auto tokens = ungula::core::util::str::tokenizeByDelimiter("a,b,c", ',');
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "a");
    EXPECT_EQ(tokens[1], "b");
    EXPECT_EQ(tokens[2], "c");
}

TEST(StringUtils, TokenizeEmpty)
{
    auto tokens = ungula::core::util::str::tokenizeByDelimiter("", ',');
    EXPECT_TRUE(tokens.empty());
}

// --- CleanDelimitedValues ---

TEST(StringUtils, CleanDelimitedValues)
{
    EXPECT_EQ(ungula::core::util::str::cleanDelimitedValues("  a   ,   b qq , , c "), "a,b qq,c");
}

TEST(StringUtils, CleanDelimitedValuesEmpty)
{
    EXPECT_EQ(ungula::core::util::str::cleanDelimitedValues(""), "");
}

// --- NumToString ---

TEST(StringUtils, NumToStringInt)
{
    EXPECT_EQ(ungula::core::util::str::num_to_string(42), "42");
    EXPECT_EQ(ungula::core::util::str::num_to_string(-7), "-7");
}

// --- StringIndexOf ---

TEST(StringUtils, StringIndexOf)
{
    std::string s = "hello";
    EXPECT_EQ(ungula::core::util::str::string_indexOf(s, 'l'), 2);
    EXPECT_EQ(ungula::core::util::str::string_indexOf(s, 'z'), -1);
    EXPECT_EQ(ungula::core::util::str::string_indexOf(s, 'l', 3), 3);
}

// --- StringSubstring ---

TEST(StringUtils, StringSubstring)
{
    std::string s = "hello world";
    EXPECT_EQ(ungula::core::util::str::string_substring(s, 0, 5), "hello");
    EXPECT_EQ(ungula::core::util::str::string_substring(s, 6), "world");
}

// --- SkipWhitespace ---

TEST(StringUtils, SkipWhitespace)
{
    const char *s = "   hello";
    EXPECT_STREQ(ungula::core::util::str::skipWhitespace(s), "hello");
}

TEST(StringUtils, SkipWhitespaceView)
{
    std::string_view sv = "   hello";
    EXPECT_EQ(ungula::core::util::str::skipWhitespaceView(sv), "hello");
}

// --- TrimWhitespace (C-buffer) ---

TEST(StringUtils, TrimWhitespaceCBuffer)
{
    char buf[] = "  hello  ";
    size_t len = 9;
    ungula::core::util::str::trimWhitespace(buf, len);
    EXPECT_EQ(len, 5u);
    EXPECT_EQ(std::string(buf, len), "hello");
}
