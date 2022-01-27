/*
 * Copyright (C) 2022 Smirnov Vladimir mapron1@gmail.com
 * Source code licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in file COPYING-APACHE-2.0.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.h
 */

#include "TestUtils.h"

#include <AbstractInvocation.h>
#include <ArgumentList.h>

std::ostream& operator<<(std::ostream& out, const Wuild::StringVector& v)
{
    out << '[';
    for (const auto& str : v)
        out << "'" << str << "', ";
    out << "]";

    return out;
}

std::ostream& operator<<(std::ostream& out, const Wuild::AbstractInvocation::ArgList& v)
{
    out << '[';
    for (const auto& arg : v)
        out << "'" << (arg.m_isOption ? arg.m_optionSymbol : '_') << arg.m_arg << "', ";
    out << "]";

    return out;
}

/*
 * Autotest for command line parsing.
 */
int main(int argc, char** argv)
{
    using namespace Wuild;
    ConfiguredApplication app(argc, argv, "TestCommandline");

#define TEST_EXPECT(input, settings, expected) \
    { \
        auto actual = ParseArgumentList(input, settings); \
        if (actual.m_args != expected) { \
            std::cout << "For input='" << input << "', expected=" << expected << ", but actual=" << actual.m_args << "\n"; \
            assert(0); \
            return 1; \
        } \
    }

    TEST_EXPECT((""), {}, StringVector{});
    TEST_EXPECT(("foo"), {}, StringVector{ "foo" });
    TEST_EXPECT(("foo bar"), {}, (StringVector{ "foo", "bar" }));
    TEST_EXPECT(("foo  \r\n  bar"), {}, (StringVector{ "foo", "bar" }));
    TEST_EXPECT((StringVector{ "foo bar", "baz baz" }), {}, (StringVector{ "foo", "bar", "baz", "baz" }));

    TEST_EXPECT(("\"foo bar\""), {}, (StringVector{ "foo bar" }));
    TEST_EXPECT(("\"foo bar\""), (ArgumentParseSettings{ false, false }), (StringVector{ "\"foo bar\"" }));
    TEST_EXPECT((StringVector{ "qoo", "\"foo bar\" baz \"ber\"" }), {}, (StringVector{ "qoo", "foo bar", "baz", "ber" }));

    TEST_EXPECT(("foo\\ bar"), (ArgumentParseSettings{ true, true, false }), (StringVector{ "foo\\", "bar" }));
    TEST_EXPECT(("foo\\ bar"), (ArgumentParseSettings{ false, true, false }), (StringVector{ "foo\\", "bar" }));
    TEST_EXPECT(("foo\\ bar"), (ArgumentParseSettings{ true, true, true }), (StringVector{ "foo bar" }));
    TEST_EXPECT(("foo\\ bar"), (ArgumentParseSettings{ false, true, true }), (StringVector{ "foo\\ bar" }));

#undef TEST_EXPECT
#define TEST_EXPECT(input, settings, expected) \
    { \
        ArgumentList input2{ input }; \
        auto         actual = input2.ToString(settings); \
        if (actual != expected) { \
            std::cout << "For input=" << input << ", expected='" << expected << "', but actual='" << actual << "'\n"; \
            assert(0); \
            return 1; \
        } \
    }
    TEST_EXPECT(StringVector{}, {}, (""));
    TEST_EXPECT((StringVector{ "foo" }), {}, "foo");
    TEST_EXPECT((StringVector{ "foo " }), (ArgumentStringifySettings{ true, false }), "\"foo \"");
    TEST_EXPECT((StringVector{ "foo baz", "bar" }), (ArgumentStringifySettings{ true, false }), "\"foo baz\" bar");
    TEST_EXPECT((StringVector{ "foo baz", "bar" }), (ArgumentStringifySettings{ true, true }), "foo\\ baz bar");
    TEST_EXPECT((StringVector{ "foo baz", "bar" }), (ArgumentStringifySettings{ false, false }), "foo baz bar");

#undef TEST_EXPECT
#define TEST_EXPECT(input, settings) \
    { \
        AbstractInvocation input2{ ArgumentList{ input }, AbstractInvocationParseSettings settings }; \
        auto               actual = input2.ToArgumentList().m_args; \
        if (actual != input) { \
            std::cout << "For input=" << input << ", expected identical result, but actual=" << actual << "\n"; \
            assert(0); \
            return 1; \
        } \
    }
    TEST_EXPECT(StringVector{}, {});
    TEST_EXPECT(StringVector{ " " }, {});
    TEST_EXPECT(StringVector{ "foo" }, {});
    TEST_EXPECT(StringVector{ "-" }, {});
    TEST_EXPECT(StringVector{ "foo" }, { false });
    TEST_EXPECT((StringVector{ "foo", "bar" }), { false });

#undef TEST_EXPECT
#define TEST_EXPECT(input, settings, expected) \
    { \
        AbstractInvocation input2{ ArgumentList{ input }, settings }; \
        const auto&        actual = input2.GetArgs(); \
        if (actual != expected) { \
            std::cout << "For input=" << input << ", expected=" << expected << ", but actual=" << actual << "\n"; \
            assert(0); \
            return 1; \
        } \
    }
    TEST_EXPECT(StringVector{}, {}, AbstractInvocation::ArgList{});
    TEST_EXPECT(StringVector{ "foo" }, {}, AbstractInvocation::ArgList{});
    TEST_EXPECT((StringVector{ "foo", "bar" }), {}, AbstractInvocation::ArgList{ AbstractInvocation::Arg{ "bar" } });
    TEST_EXPECT((StringVector{ "foo", "-bar" }), {}, (AbstractInvocation::ArgList{ AbstractInvocation::Arg{ "bar", true, '-' } }));
    TEST_EXPECT((StringVector{ "foo", "/bar" }), {}, (AbstractInvocation::ArgList{ AbstractInvocation::Arg{ "/bar" } }));
    TEST_EXPECT((StringVector{ "foo", "/bar", "-baz" }), (AbstractInvocationParseSettings{ true, AbstractInvocationParseSettings::s_ms }), (AbstractInvocation::ArgList{ AbstractInvocation::Arg{ "bar", true, '/' }, AbstractInvocation::Arg{ "baz", true, '-' } }));
    TEST_EXPECT((StringVector{ "foo", "-bar", "fe zz" }), {}, (AbstractInvocation::ArgList{ AbstractInvocation::Arg{ "bar", true, '-' }, AbstractInvocation::Arg{ "fe zz" } }));

    std::cout << "OK\n";
    return 0;
}