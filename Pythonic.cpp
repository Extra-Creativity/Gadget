// Check whether key matches required type rules.
// C++20 needed, clang & gcc compile right while msvc fails in Sep 1st, 2022(This has been reported to the MS team).
// In the future, this may evolve to Pythonic key-value function wrapper, which needs 
// mapping from string to varaibles that may be supported by C++23 reflection.

#include <array>
#include <type_traits>

constexpr bool CompareStr(const char* str1, const char* str2)
{
    return *str1 == *str2 && (*str1 == '\0' || CompareStr(str1 + 1, str2 + 1));
}    

template<template<typename> typename ...Ts>
class Checker
{
public:
    class TypeChecker
    {
    public:
        template<typename T>
        static constexpr bool CheckType(int id)
        {
            if (id >= sizeof...(Ts) || id < 0)
                return false;
            return CheckType_<T, Ts...>(id);
        }
    private:
        // just to escape compliation error.
        template<typename T>
        static constexpr bool CheckType_(int id)
        {
            return false;
        }

        template<typename T, template<typename> typename U, template<typename> typename ...Us>
        static constexpr bool CheckType_(int id)
        {
            if (id == 0)
                return U<T>::value;
            return CheckType_<T, Us...>(id - 1);
        }
    };

    constexpr Checker(const std::array<const char*, sizeof...(Ts)>& init_keyList) :
        keyList(init_keyList){};
    std::array<const char*, sizeof...(Ts)> keyList;
    TypeChecker typeChecker;
};

template<typename T, template<typename> typename ...Ts>
constexpr int inArr(const char* para, const Checker<Ts...>& checker)
{
    for (std::size_t i = 0; i < checker.keyList.size(); i++)
    {
        if (CompareStr(para, checker.keyList[i]))
        {
            return checker.typeChecker.template CheckType<T>(i) ? i : -2;
        }
    }
    return -1;
}

template<typename ...Args>
constexpr int FuncCheck(const int id, Args... args)
{
    if (sizeof...(args) == 1)
        return -1;
    return id;
}

template<typename T, template<typename> typename ...Ts, typename ...Args>
constexpr int FuncCheck(const int id, const Checker<Ts...>& checker, const char* arg1, T&&, Args... args)
{
    if (inArr<std::remove_cvref_t<T>>(arg1, checker) == -1)
        return id;
    else if (inArr<std::remove_cvref_t<T>>(arg1, checker) == -2)
        return id + 1;
    return FuncCheck(id + 2, checker, args...);
}

#define CallWithChecker(func, checker, ...) \
    [&]()->const int\
    {\
        constexpr int __r = FuncCheck(0, checker, __VA_ARGS__); \
        static_assert(__r == -1, "Some parameters are not in the key list, or the corresponding value has a wrong type."\
            "\nCheck the result value to get which one is wrong."); \
        return __r; \
    }(); \
    func(__VA_ARGS__);

template<typename ...Ts>
void func(Ts...) {};

int main()
{
    constexpr Checker<std::is_integral, std::is_floating_point> checker{ {"width", "height"} };
    int b = 2;
    // Wait for reflection.
    CallWithChecker(func, checker, "width", b, "height", 1.0f);
    return 0;
}
