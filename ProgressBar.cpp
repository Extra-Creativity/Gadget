// multithread-safe progress bar.
#include <iostream>
#include <mutex>
#include <atomic>
#include <format>
#include <thread>
#include <algorithm>
#include <chrono>

// Lock-needed version.
class ProgressBar
{
public:
    std::atomic<int> counter;
    int maxProgress;
    ProgressBar(int init_maxProgress) :counter{ 0 }, maxProgress(init_maxProgress), 
        barStr_(barLen_, ' '), lastPos_(0) {}

    void update()
    {
        int currCnt = counter.fetch_add(1, std::memory_order_relaxed) + 1;
        std::unique_lock<std::mutex> _(guard_, std::try_to_lock);
        bool ownLock = _.owns_lock(), ending = (currCnt == maxProgress);
        auto endingOutput = [this]() {
            std::fill(barStr_.begin() + lastPos_, barStr_.end(), '#');
            std::cerr << std::format("[{0}] 100%\n", barStr_);
            return;
        };

        if (ownLock)
        {
            if (ending) [[unlikely]]
            {
                endingOutput();
                return;
            }
            float percent = static_cast<float>(currCnt) / maxProgress;
            int newPos = static_cast<int>(percent * barLen_);
            std::fill(barStr_.begin() + lastPos_, barStr_.begin() + newPos, '#');
            lastPos_ = newPos;
            std::cerr << std::format("[{0}] {1}%\r", barStr_, static_cast<int>(percent * 100.0f));
        }
        else if (ending) [[unlikely]]
        {
            _.lock(); // Wait for possible missed ending.
            endingOutput();
        }
        return;
    }
   
    void reset(int reset_maxProgress = 0) {
        lastPos_ = 0;
        std::fill(barStr_.begin(), barStr_.end(), ' ');
        counter.store(0);
        if (reset_maxProgress > 0)
        {
            maxProgress = reset_maxProgress;
        }
        return;
    }
private:
    std::mutex guard_;
    std::string barStr_;
    int lastPos_;
    static const int barLen_ = 50;
};

// Lock-free version
class ProgressBar
{
public:
    std::atomic<int> counter;
    int maxProgress;
    ProgressBar(int init_maxProgress) :counter{ 0 }, guard_{ true }, maxProgress(init_maxProgress),
        barStr_(barLen_, ' '), lastPos_(0) {}

    void update()
    {
        int currCnt = counter.fetch_add(1, std::memory_order_relaxed) + 1;
        bool ending = (currCnt == maxProgress);
        auto endingOutput = [this]() {
            std::fill(barStr_.begin() + lastPos_, barStr_.end(), '#');
            std::cerr << std::format("[{0}] 100%\n", barStr_);
            return;
        };
        
        if (guard_.exchange(false, std::memory_order_acq_rel))
        {
            if (ending) [[unlikely]]
            {
                endingOutput();
                return;
            }
            float percent = static_cast<float>(currCnt) / maxProgress;
            int newPos = static_cast<int>(percent * barLen_);
            std::fill(barStr_.begin() + lastPos_, barStr_.begin() + newPos, '#');
            lastPos_ = newPos;
            std::cerr << std::format("[{0}] {1}%\r", barStr_, static_cast<int>(percent * 100.0f));
            guard_.store(true, std::memory_order_release);
        }
        else if (ending) [[unlikely]]
        {
            // wait for ending.
            while (!guard_.load(std::memory_order_acquire));
            endingOutput();
        }
        return;
    }
   
    void reset(int reset_maxProgress = 0) {
        lastPos_ = 0;
        std::fill(barStr_.begin(), barStr_.end(), ' ');
        counter.store(0);
        if (reset_maxProgress > 0)
        {
            maxProgress = reset_maxProgress;
        }
        guard_.store(true, std::memory_order_release);
        return;
    }
private:
    std::atomic<bool> guard_;
    std::string barStr_;
    int lastPos_;
    static const int barLen_ = 50;
};

// For test purpose.
ProgressBar bar(100);
// generate random work time.
std::random_device rd{};
std::uniform_int_distribution distribution(10, 200);
std::default_random_engine generator(rd());

int main()
{
    auto update = []() {
        for (int i = 0; i < 100; i++)
        {
            using namespace std::chrono_literals;
            bar.update();
            std::this_thread::sleep_for(distribution(generator) * 1ms);
        }
        return;
    };
    
    std::cout << "In single thread:\n";
    update();

    bar.reset(1000);
    std::cout << "In multithreads:\n";

    std::thread threads[10];
    for (int i = 0; i < 10; i++)
    {
        threads[i] = std::thread{ update };
    }
    for (int i = 0; i < 10; i++)
    {
        threads[i].join();
    }
    return 0;
}
