#include <array>
#include <iostream>
#include <type_traits>

struct Judgement
{
    virtual constexpr bool Judge() const { return false; };
};

template<typename T>
struct IsInt : Judgement
{
public:
    virtual constexpr bool Judge() const override { return std::is_integral<T>::value; }

};
template<typename T>
constexpr auto IsIntInstance = IsInt<T>{};

template<typename T>
struct IsFloat : Judgement
{
public:
    virtual constexpr bool Judge() const override { return std::is_floating_point<T>::value; }
};
template<typename T>
constexpr auto IsFloatInstance = IsFloat<T>{};

// To be consistent with std::make_tuple etc.
template<class T, class... Tail>
constexpr auto make_array(T head, Tail... tail)->std::array<T, 1 + sizeof...(Tail)>
{
    std::array<T, 1 + sizeof...(Tail)> a = { head, tail ... };
    return a;
}

constexpr auto arr = make_array("width", "height");

template<typename T>
constexpr auto typeChecker = std::array<const Judgement*, 2>{
    &IsIntInstance<T>, &IsFloatInstance<T>
};

constexpr bool CompareStr(const char* str1, const char* str2)
{
    return *str1 == *str2 && (*str1 == '\0' || CompareStr(str1 + 1, str2 + 1));
}

template<typename T>
constexpr int inArr(const char* para)
{
    for (std::size_t i = 0; i < arr.size(); i++)
    {
        if (CompareStr(para, arr[i]) && typeChecker<T>[i]->Judge())
            return i;
    }
    return -1;
}

template<typename ...Args>
constexpr int FuncCheck(const int id, Args... args)
{
    if (sizeof...(args) == 0)
        return -1;
    return id;
}

template<typename T, typename ...Args>
constexpr int FuncCheck(const int id, const char* arg1, T, Args... args)
{
    if (inArr<T>(arg1) == -1)
        return id;
    return FuncCheck(id + 2, args...);
}

template<typename ...Args>
void Func(Args... args) {
    // Really executed code.
    // Still considering.
}

#define TEST(...) \
    [&]()->const int\
    {\
        constexpr int __r = FuncCheck(0, __VA_ARGS__); \
        static_assert(__r == -1, "Some parameters are not in the key list, or the corresponding value has a wrong type."\
            "\nCheck the result value to get which one is wrong.");\
        return __r;\
    }();\
    Func(__VA_ARGS__);

int main()
{
    const int check = TEST("width", 1, "height", 1.0f);
    std::cout << check;
    return 0;
}