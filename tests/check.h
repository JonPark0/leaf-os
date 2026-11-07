// Tiny host test harness — no external deps. Register a case with TEST(name),
// assert with EXPECT_*, run everything from main() via RUN_ALL_TESTS().
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace check {

struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline const char*& current() { static const char* c = ""; return c; }

struct Registrar { Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };

inline void fail(const char* file, int line, const std::string& msg)
{
    ++failures();
    std::printf("  FAIL %s:%d  [%s]  %s\n", file, line, current(), msg.c_str());
}

inline int runAll()
{
    int passed = 0;
    for (auto& c : registry()) {
        current() = c.name;
        int before = failures();
        c.fn();
        if (failures() == before) { ++passed; std::printf("  ok   %s\n", c.name); }
    }
    std::printf("\n%d/%zu passed, %d failure(s)\n",
                passed, registry().size(), failures());
    return failures() == 0 ? 0 : 1;
}

}  // namespace check

#define TEST(name)                                                             \
    static void test_##name();                                                 \
    static ::check::Registrar reg_##name(#name, test_##name);                   \
    static void test_##name()

#define EXPECT_TRUE(cond)                                                      \
    do { if (!(cond)) ::check::fail(__FILE__, __LINE__, "EXPECT_TRUE(" #cond ")"); } while (0)

#define EXPECT_EQ(a, b)                                                        \
    do { auto _va = (a); auto _vb = (b); if (!(_va == _vb))                    \
        ::check::fail(__FILE__, __LINE__,                                      \
            std::string(#a " == " #b "  (") + std::to_string((long long)_va) + \
            " vs " + std::to_string((long long)_vb) + ")"); } while (0)

#define EXPECT_STREQ(a, b)                                                     \
    do { std::string _sa = (a); std::string _sb = (b); if (_sa != _sb)         \
        ::check::fail(__FILE__, __LINE__,                                      \
            std::string(#a " == " #b "  (\"") + _sa + "\" vs \"" + _sb + "\")"); } while (0)

#define RUN_ALL_TESTS() ::check::runAll()
