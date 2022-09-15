// Experiments on tailed 2D matrix.
// If in a tailed and normally sequenced way : 
// Clang 15.0.0 -O3 output : 
/* Empty loop : 9.8e-08s
   Normal matrix : 0.000580095s
   No padding, no lookup matrix : 0.000704029s
   Padding, no lookup matrix : 0.000408301s
   Padding, lookup matrix : 0.000854599s
   Fill in right.
*/
// GCC 12.2 -O3 output : 
/* Empty loop : 1.27e-07s
   Normal matrix : 0.00116924s
   No padding, no lookup matrix : 0.000911699s
   Padding, no lookup matrix : 0.00128065s
   Padding, lookup matrix : 0.000834393s
   Fill in right.
*/
// x64 MSVC 19.32 /O2 /Oi output : 
/* Empty loop : 1e-07s
   Normal matrix : 0.0014819s
   No padding, no lookup matrix : 0.0020457s
   Padding, no lookup matrix : 0.0016727s
   Padding, lookup matrix : 0.0014371s
*/
// If in a random permutation way :
// Clang 15.0.0 -O3 output : 
/* Empty loop : 1.2e-07s
   Normal matrix : 0.000967834s
   No padding, no lookup matrix : 0.0009548s
   Padding, no lookup matrix : 0.000940871s
   Padding, lookup matrix : 0.00115011s
   Fill in right.
*/
// GCC 12.2 -O3 output :
/* Empty loop : 1.1e-07s
   Normal matrix : 0.000880904s
   No padding, no lookup matrix : 0.000898521s
   Padding, no lookup matrix : 0.000930923s
   Padding, lookup matrix : 0.00121801s
   Fill in right.
*/
// x64 MSVC 19.32 /O2 /Oi output : 
/* Empty loop : 1e-07s
   Normal matrix : 0.0017142s
   No padding, no lookup matrix : 0.0023464s
   Padding, no lookup matrix : 0.0019774s
   Padding, lookup matrix : 0.001578s
   Fill in right.
*/
// Unfortunately, there seems to be little acceleration.

#include <cassert>
#include <array>
#include <format>
#include <vector>

// For test purpose
#include <iostream>
#include <chrono>
#include <random>

inline auto GetIntervalSecond(std::chrono::steady_clock::time_point& t1,
    std::chrono::steady_clock::time_point& t2)
{
    return std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
}

constexpr int IntCeilDiv(int num1, int num2)
{
    return (num1 - 1) / num2 + 1;
}

template<typename T, int rowNum, int colNum, int sliceRowSize, int sliceColSize,
    bool needLookUp = false, bool needPadding = false>
    requires (rowNum > 0) && (colNum > 0) && (sliceRowSize > 0) && (sliceColSize > 0)
    class TailedArray2D;

template<typename T, int rowNum, int colNum, int sliceRowSize, int sliceColSize>
class TailedArray2D<T, rowNum, colNum, sliceRowSize, sliceColSize, false, false>
{
public:
    T& operator()(size_t i, size_t j)
    {
        assert(i < rowNum && j < colNum);
        size_t iQuot = i / sliceRowSize, iRemainder = i % sliceRowSize,
            jQuot = j / sliceColSize, jRemainder = j % sliceColSize;
        constexpr size_t lastBlockCol = (colNum / sliceColSize) * sliceColSize,
            lastBlockRow = (rowNum / sliceRowSize) * sliceRowSize;
        constexpr size_t blockColRemainder = colNum % sliceColSize,
            blockRowRemainder = rowNum % sliceRowSize;

        size_t id = (i - iRemainder) * colNum + jRemainder + iRemainder *
            (j < lastBlockCol ? sliceColSize : blockColRemainder) + jQuot * sliceColSize * 
            (i < lastBlockRow ? sliceRowSize : blockRowRemainder);
        return m_arr[id];
    }

    constexpr int RowSize() { return rowNum; }
    constexpr int ColSize() { return colNum; }

    constexpr int SliceRowSize() { return sliceRowSize; }
    constexpr int SliceColSize() { return sliceColSize; }
private:
    std::vector<T> m_arr = std::vector<T>(rowNum * colNum);
};

template<typename T, int rowNum, int colNum, int sliceRowSize, int sliceColSize>
class TailedArray2D<T, rowNum, colNum, sliceRowSize, sliceColSize, true, true>
{
public:
    T& operator()(size_t i, size_t j)
    {
        assert(i < rowNum && j < colNum);
        return m_arr[m_rowLookup[i] + m_colLoopup[j]];
    }

    constexpr int RowSize() { return rowNum; }
    constexpr int ColSize() { return colNum; }

    constexpr int SliceRowSize() { return sliceRowSize; }
    constexpr int SliceColSize() { return sliceColSize; }
private:
    std::vector<T> m_arr = std::vector<T>(IntCeilDiv(rowNum, sliceRowSize) * sliceRowSize *
        IntCeilDiv(colNum, sliceColSize) * sliceColSize);
    static constexpr std::array<size_t, rowNum> m_rowLookup = []() {
        std::array<size_t, rowNum> result;
        for (size_t i = 0; i < rowNum; i++)
        {
            size_t iQuot = i / sliceRowSize, iRemainder = i % sliceRowSize;
            result[i] = iQuot * IntCeilDiv(colNum, sliceColSize) * sliceRowSize * sliceColSize
                + iRemainder * sliceColSize;
        }
        return result;
    }();
    static constexpr std::array<size_t, colNum> m_colLoopup = []() {
        std::array<size_t, colNum> result;
        for (size_t j = 0; j < colNum; j++)
        {
            size_t jQuot = j / sliceColSize, jRemainder = j % sliceColSize;
            result[j] = jQuot * (sliceRowSize * sliceColSize) + jRemainder;
        }
        return result;
    }();
};

template<typename T, int rowNum, int colNum, int sliceRowSize, int sliceColSize>
class TailedArray2D<T, rowNum, colNum, sliceRowSize, sliceColSize, false, true>
{
public:
    T& operator()(size_t i, size_t j)
    {
        assert(i < rowNum&& j < colNum);
        size_t iQuot = i / sliceRowSize, iRemainder = i % sliceRowSize;
        size_t jQuot = j / sliceColSize, jRemainder = j % sliceColSize;
        constexpr size_t blockSize = sliceRowSize * sliceColSize;
        constexpr size_t blockSumSizePerRow = IntCeilDiv(colNum, sliceColSize) * blockSize;
        size_t id = iQuot * blockSumSizePerRow + iRemainder * sliceColSize + jQuot * blockSize + jRemainder;
        return m_arr[id];
    }

    constexpr int RowSize() { return rowNum; }
    constexpr int ColSize() { return colNum; }

    constexpr int SliceRowSize() { return sliceRowSize; }
    constexpr int SliceColSize() { return sliceColSize; }
private:
    std::vector<T> m_arr = std::vector<T>(IntCeilDiv(rowNum, sliceRowSize) * sliceRowSize *
        IntCeilDiv(colNum, sliceColSize) * sliceColSize);
};

template<typename T, int rowNum, int colNum>
class VectorWrapper
{
public:
    T& operator()(int i, int j) {
        assert(i < rowNum&& j < colNum);
        return m_arr[i * colNum + j]; 
    }
private:
    std::vector<T> m_arr = std::vector<T>(rowNum * colNum);
};

// main1.cpp, normally sequenced.
constexpr std::array matShape{ 1024,1024 };
constexpr std::array tileShape{ 16, 16 };
const int checkTimes = 100;


int main()
{
    // generate a 1024 * 1024 matrix.
    VectorWrapper<int, matShape[0], matShape[1]> normalMatrix;

    // For no padding & no lookup version.
    TailedArray2D<int, matShape[0], matShape[1], tileShape[0], tileShape[1], false, false> arrNPNL;

    // For padding & no loopup version.
    TailedArray2D<int, matShape[0], matShape[1], tileShape[0], tileShape[1], false, true> arrPNL;

    // For padding & lookup version
    TailedArray2D<int, matShape[0], matShape[1], tileShape[0], tileShape[1], true, true> arrPL;

    int cnt = 0;
    auto beginTime = std::chrono::steady_clock::now(), endTime = beginTime;
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                if (i + ii >= matShape[0]) [[unlikely]]
                    break;
                for (int jj = 0; jj < tileShape[1]; jj++)
                    ++cnt;
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Empty loop: {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                if (i + ii >= matShape[0]) [[unlikely]]
                    break;
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    if (j + jj >= matShape[1]) [[unlikely]]
                        break;
                    normalMatrix(i + ii, j + jj) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Normal matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                if (i + ii >= matShape[0]) [[unlikely]]
                    break;
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    if (j + jj >= matShape[1]) [[unlikely]]
                        break;
                    arrNPNL(i + ii, j + jj) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("No padding, no lookup matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                if (i + ii >= matShape[0]) [[unlikely]]
                    break;
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    if (j + jj >= matShape[1]) [[unlikely]]
                        break;
                    arrPNL(i + ii, j + jj) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Padding, no lookup matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                if (i + ii >= matShape[0]) [[unlikely]]
                    break;
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    if (j + jj >= matShape[1]) [[unlikely]]
                        break;
                    arrPL(i + ii, j + jj) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Padding, lookup matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    // Randomly check value.
    std::random_device rd{};
    std::uniform_int_distribution<int> distribution1(0, matShape[0] - 1),
        distribution2(0, matShape[1] - 1);
    std::default_random_engine generator1{ rd() }, generator2{rd()};
    int i = 0, j = 0;
    bool success = true;
    for (int _ = 0; _ < checkTimes; _++)
    {
        i = distribution1(generator1), j = distribution2(generator2);
        auto temp1 = normalMatrix(i, j), temp2 = arrNPNL(i, j),
            temp3 = arrPNL(i, j), temp4 = arrPL(i, j);
        if (!((temp1 == temp2) && (temp2 == temp3) && (temp3 == temp4)))
        {
            success = false;
            break;
        }
    }
    if (success)
        std::cout << "Fill in right.\n";
    else
        std::cout << std::format("Oops, wrong in ({}, {})\n", i, j);

    return 0;
}

// main2.cpp, random permutation
constexpr std::array matShape{ 1024,1024 };
constexpr std::array tileShape{ 16, 16 };
const int checkTimes = 100;
std::array<std::array<std::pair<size_t, size_t>, tileShape[1]>, tileShape[0]> sequenceInBlock;

void GenerateRandomPermutation()
{
    const int tileSize = tileShape[0] * tileShape[1];
    int cnt = tileSize;
    std::random_device rd{};
    std::uniform_int_distribution<int> distribution(0, tileSize - 1);
    std::default_random_engine generator{ rd() };
    while (cnt)
    {
        int randVal1 = distribution(generator);
        if (randVal1 / cnt > tileSize / cnt)
            continue;
        int id1 = randVal1 % cnt;

        int randVal2 = distribution(generator);
        if (randVal2 / cnt > tileSize / cnt)
            continue;
        int id2 = randVal2 % cnt;

        --cnt;
        sequenceInBlock[cnt / tileShape[0]][cnt % tileShape[0]] = { id1,id2 };
    }
    return;
}

int main()
{
    GenerateRandomPermutation();
    // generate a 1024 * 1024 matrix.
    VectorWrapper<int, matShape[0], matShape[1]> normalMatrix;

    // For no padding & no lookup version.
    TailedArray2D<int, matShape[0], matShape[1], tileShape[0], tileShape[1], false, false> arrNPNL;

    // For padding & no loopup version.
    TailedArray2D<int, matShape[0], matShape[1], tileShape[0], tileShape[1], false, true> arrPNL;

    // For padding & lookup version
    TailedArray2D<int, matShape[0], matShape[1], tileShape[0], tileShape[1], true, true> arrPL;

    int cnt = 0;
    auto beginTime = std::chrono::steady_clock::now(), endTime = beginTime;
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    int row = i + sequenceInBlock[ii][jj].first, col = j + sequenceInBlock[ii][jj].second;
                    if (row > matShape[0] || col > matShape[1])
                        continue;
                    ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Empty loop: {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    int row = i + sequenceInBlock[ii][jj].first, col = j + sequenceInBlock[ii][jj].second;
                    if (row > matShape[0] || col > matShape[1])
                        continue;
                    normalMatrix(i, j) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Normal matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    int row = i + sequenceInBlock[ii][jj].first, col = j + sequenceInBlock[ii][jj].second;
                    if (row > matShape[0] || col > matShape[1])
                        continue;
                    arrNPNL(i, j) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("No padding, no lookup matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    int row = i + sequenceInBlock[ii][jj].first, col = j + sequenceInBlock[ii][jj].second;
                    if (row > matShape[0] || col > matShape[1])
                        continue;
                    arrPNL(i, j) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Padding, no lookup matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    cnt = 0;
    beginTime = std::chrono::steady_clock::now();
    for (int i = 0; i < matShape[0]; i += tileShape[0])
        for (int j = 0; j < matShape[1]; j += tileShape[1])
            for (int ii = 0; ii < tileShape[0]; ii++)
            {
                for (int jj = 0; jj < tileShape[1]; jj++)
                {
                    int row = i + sequenceInBlock[ii][jj].first, col = j + sequenceInBlock[ii][jj].second;
                    if (row > matShape[0] || col > matShape[1])
                        continue;
                    arrPL(i, j) = ++cnt;
                }
            }
    endTime = std::chrono::steady_clock::now();
    std::cout << std::format("Padding, lookup matrix : {}\n", GetIntervalSecond(beginTime, endTime));

    // Randomly check value.
    std::random_device rd{};
    std::uniform_int_distribution<int> distribution1(0, matShape[0] - 1),
        distribution2(0, matShape[1] - 1);
    std::default_random_engine generator1{ rd() }, generator2{rd()};
    int i = 0, j = 0;
    bool success = true;
    for (int _ = 0; _ < checkTimes; _++)
    {
        i = distribution1(generator1), j = distribution2(generator2);
        auto temp1 = normalMatrix(i, j), temp2 = arrNPNL(i, j),
            temp3 = arrPNL(i, j), temp4 = arrPL(i, j);
        if (!((temp1 == temp2) && (temp2 == temp3) && (temp3 == temp4)))
        {
            success = false;
            break;
        }
    }
    if (success)
        std::cout << "Fill in right.\n";
    else
        std::cout << std::format("Oops, wrong in ({}, {})\n", i, j);

    return 0;
}
