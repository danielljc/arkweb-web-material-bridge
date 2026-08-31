#ifndef ARKWEB_WEB_MATERIAL_TEST_FRAMEWORK_H_
#define ARKWEB_WEB_MATERIAL_TEST_FRAMEWORK_H_

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace arkweb::material::test {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> body;
};

class Registry {
public:
    static Registry& Instance()
    {
        static Registry registry;
        return registry;
    }

    bool Add(std::string suite, std::string name, std::function<void()> body)
    {
        tests_.push_back({std::move(suite), std::move(name), std::move(body)});
        return true;
    }

    const std::vector<TestCase>& Tests() const
    {
        return tests_;
    }

private:
    std::vector<TestCase> tests_;
};

class AssertionFailure final : public std::runtime_error {
public:
    explicit AssertionFailure(std::string message) : std::runtime_error(std::move(message)) {}
};

inline std::string Location(const char* file, int line)
{
    std::ostringstream stream;
    stream << file << ':' << line;
    return stream.str();
}

inline void Expect(bool condition,
    const char* expression,
    const char* file,
    int line,
    bool fatal)
{
    if (condition) {
        return;
    }
    std::ostringstream stream;
    stream << Location(file, line) << ": expectation failed: " << expression;
    if (fatal) {
        throw AssertionFailure(stream.str());
    }
    throw AssertionFailure(stream.str());
}

template <typename Lhs, typename Rhs>
void ExpectEqual(const Lhs& lhs,
    const Rhs& rhs,
    const char* lhs_expression,
    const char* rhs_expression,
    const char* file,
    int line)
{
    if (lhs == rhs) {
        return;
    }
    std::ostringstream stream;
    stream << Location(file, line) << ": expected " << lhs_expression
           << " == " << rhs_expression;
    throw AssertionFailure(stream.str());
}

template <typename Lhs, typename Rhs>
void ExpectNotEqual(const Lhs& lhs,
    const Rhs& rhs,
    const char* lhs_expression,
    const char* rhs_expression,
    const char* file,
    int line)
{
    if (lhs != rhs) {
        return;
    }
    std::ostringstream stream;
    stream << Location(file, line) << ": expected " << lhs_expression
           << " != " << rhs_expression;
    throw AssertionFailure(stream.str());
}

inline void ExpectNear(double lhs,
    double rhs,
    double tolerance,
    const char* lhs_expression,
    const char* rhs_expression,
    const char* file,
    int line)
{
    if (std::abs(lhs - rhs) <= tolerance) {
        return;
    }
    std::ostringstream stream;
    stream << Location(file, line) << ": expected " << lhs_expression
           << " near " << rhs_expression << " within " << tolerance
           << ", actual difference " << std::abs(lhs - rhs);
    throw AssertionFailure(stream.str());
}

template <typename Exception, typename Callable>
void ExpectThrows(Callable callable,
    const char* expression,
    const char* file,
    int line)
{
    try {
        callable();
    } catch (const Exception&) {
        return;
    } catch (...) {
        throw AssertionFailure(Location(file, line) +
            ": expression threw an unexpected exception type: " + expression);
    }
    throw AssertionFailure(Location(file, line) +
        ": expression did not throw: " + expression);
}

struct RunOptions {
    std::string filter;
    int repeat = 1;
    bool list_only = false;
};

inline bool MatchesFilter(const TestCase& test, const std::string& filter)
{
    if (filter.empty()) {
        return true;
    }
    const std::string full_name = test.suite + "." + test.name;
    return full_name.find(filter) != std::string::npos;
}

inline int RunAll(const RunOptions& options)
{
    const auto& tests = Registry::Instance().Tests();
    if (options.list_only) {
        for (const TestCase& test : tests) {
            if (MatchesFilter(test, options.filter)) {
                std::cout << test.suite << '.' << test.name << '\n';
            }
        }
        return 0;
    }

    int executed = 0;
    int failed = 0;
    for (int iteration = 0; iteration < options.repeat; ++iteration) {
        for (const TestCase& test : tests) {
            if (!MatchesFilter(test, options.filter)) {
                continue;
            }
            ++executed;
            try {
                test.body();
                std::cout << "[  PASS  ] " << test.suite << '.' << test.name << '\n';
            } catch (const std::exception& error) {
                ++failed;
                std::cerr << "[  FAIL  ] " << test.suite << '.' << test.name
                          << "\n            " << error.what() << '\n';
            } catch (...) {
                ++failed;
                std::cerr << "[  FAIL  ] " << test.suite << '.' << test.name
                          << "\n            unknown exception\n";
            }
        }
    }
    std::cout << "\nExecuted " << executed << " tests; " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace arkweb::material::test

#define TEST(SUITE, NAME)                                                        \
    static void SUITE##_##NAME##_Body();                                        \
    static const bool SUITE##_##NAME##_Registered =                             \
        ::arkweb::material::test::Registry::Instance().Add(                     \
            #SUITE, #NAME, SUITE##_##NAME##_Body);                              \
    static void SUITE##_##NAME##_Body()

#define EXPECT_TRUE(EXPRESSION)                                                  \
    ::arkweb::material::test::Expect(                                            \
        static_cast<bool>(EXPRESSION), #EXPRESSION, __FILE__, __LINE__, false)

#define EXPECT_FALSE(EXPRESSION) EXPECT_TRUE(!(EXPRESSION))

#define ASSERT_TRUE(EXPRESSION)                                                  \
    ::arkweb::material::test::Expect(                                            \
        static_cast<bool>(EXPRESSION), #EXPRESSION, __FILE__, __LINE__, true)

#define ASSERT_FALSE(EXPRESSION) ASSERT_TRUE(!(EXPRESSION))

#define EXPECT_EQ(LHS, RHS)                                                      \
    ::arkweb::material::test::ExpectEqual(                                       \
        (LHS), (RHS), #LHS, #RHS, __FILE__, __LINE__)

#define EXPECT_NE(LHS, RHS)                                                      \
    ::arkweb::material::test::ExpectNotEqual(                                    \
        (LHS), (RHS), #LHS, #RHS, __FILE__, __LINE__)

#define ASSERT_EQ(LHS, RHS) EXPECT_EQ(LHS, RHS)

#define ASSERT_NE(LHS, RHS) EXPECT_NE(LHS, RHS)

#define EXPECT_NEAR(LHS, RHS, TOLERANCE)                                         \
    ::arkweb::material::test::ExpectNear(                                        \
        static_cast<double>(LHS), static_cast<double>(RHS),                      \
        static_cast<double>(TOLERANCE), #LHS, #RHS, __FILE__, __LINE__)

#define EXPECT_THROW(EXPRESSION, EXCEPTION)                                      \
    ::arkweb::material::test::ExpectThrows<EXCEPTION>(                           \
        [&]() { static_cast<void>(EXPRESSION); }, #EXPRESSION, __FILE__, __LINE__)

#endif  // ARKWEB_WEB_MATERIAL_TEST_FRAMEWORK_H_
