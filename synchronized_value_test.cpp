#include "synchronized_value.h"
#include <shared_mutex>
#include <iostream>
#include <thread>
#include <future>
#include <catch2/catch_test_macros.hpp>

#define DBG(msg) std::cout << "thr#"<<std::this_thread::get_id() << ": " << msg << std::endl

class Big {
public:
    // Big() = default;
    Big(int val) : mVal(val) {
        // DBG(this<<" "<<*this<<" Big(int)");
    }
    int mVal = 0;

    // Big() = delete;
    // Big           (const Big&) = delete;
    // Big& operator=(const Big&) = delete;

    // Big (Big&& other) : mVal(std::move(other.mVal)) {
    //     other.mVal = 0;
    //     DBG(this<<" "<<*this<<" Big(Big&&)");
    // }
    // Big& operator=(Big&& other) {
    //     if (this != &other) {
    //         mVal = std::move(other.mVal);
    //         other.mVal = 0;
    //         DBG(this<<" "<<*this<<" Big::operator=(Big&&)");
    //     }
    //     return *this;
    // }
    // ~Big() {
    //     DBG(this<<" "<<*this<<" ~Big()");
    // }
    // void add(int val) {
    //     mVal += val;
    //     DBG(this<<" "<<*this<<" Big::add("<<val<<")");
    // }
    // int val() const {
    //     return mVal;
    // }
    // friend std::ostream& operator<<(std::ostream& os, const Big& b) {
    //     return os<<b.mVal;
    // }
};

class Big2 {
public:
    Big2() = default;
    Big2(int val1, int val2) : mVal1(val1), mVal2(val2) {
        // DBG(this<<" "<<*this<<" Big(int)");
    }
    int mVal1 = 0;
    int mVal2 = 0;
};

TEST_CASE("constructors", "[synchronized_value]")
{
    SECTION("default constructor") {
        synchronized_value<Big2, std::shared_mutex> syncval{};
        synchronize(syncval.reader(), [](const Big2& val){
            REQUIRE(val.mVal1 == 0);
            REQUIRE(val.mVal2 == 0);
        });
    }

    SECTION("default constructor") {
        synchronized_value<Big2, std::shared_mutex> syncval{10, 20};
        synchronize(syncval.reader(), [](const Big2& val){
            REQUIRE(val.mVal1 == 10);
            REQUIRE(val.mVal2 == 20);
        });
    }

    SECTION("copy constructor") {
        synchronized_value<Big2, std::shared_mutex> syncval{Big2{10, 20}};
        synchronize(syncval.reader(), [](const Big2& val){
            REQUIRE(val.mVal1 == 10);
            REQUIRE(val.mVal2 == 20);
        });
    }
}

TEST_CASE("sequential", "[synchronized_value]")
{
    synchronized_value<Big, std::shared_mutex> syncval1{10};
    synchronized_value<Big, std::shared_mutex> syncval2{20};

    auto f_get = [&]{
        return synchronize(
            syncval1.reader(), syncval2.reader(),
            [](const Big& val1, const Big& val2){
                return std::make_tuple(val1.mVal, val2.mVal);
            });
    };
    REQUIRE(f_get() == std::make_tuple(10, 20));

    SECTION("synchronize single with functor first") {
        synchronize([](Big& val1){
            val1.mVal += 1;
        }, syncval1);

        REQUIRE(f_get() == std::make_tuple(11, 20));
    }

    SECTION("synchronize single with functor last") {
        synchronize(syncval1, [](Big& val1){
            val1.mVal += 1;
        });

        REQUIRE(f_get() == std::make_tuple(11, 20));
    }

    SECTION("synchronize multiple with functor first") {
        synchronize([](Big& val1, Big& val2){
            val1.mVal += 1;
            val2.mVal += 2;
        }, syncval1, syncval2);

        REQUIRE(f_get() == std::make_tuple(11, 22));
    }

    SECTION("synchronize multiple with functor last") {
        synchronize(syncval1, syncval2, [](Big& val1, Big& val2){
            val1.mVal += 1;
            val2.mVal += 2;
        });

        REQUIRE(f_get() == std::make_tuple(11, 22));
    }

    SECTION("synchronize multiple Writer/Reader accessors with functor first") {
        synchronize([](Big& val1, const Big& val2){
            val1.mVal += val2.mVal;
        }, syncval1.writer(), syncval2.reader());
        
        REQUIRE(f_get() == std::make_tuple(30, 20));
    }

    SECTION("synchronize multiple Writer/Reader accessors with functor last") {
        auto res = synchronize(
            syncval1.writer(), syncval2.reader(), 
            [](Big& val1, const Big& val2){
                val1.mVal += val2.mVal;
                return std::make_tuple(val1.mVal, val2.mVal);
            });

        REQUIRE(res     == std::make_tuple(30, 20));
        REQUIRE(f_get() == std::make_tuple(30, 20));
    }
}


#if __cplusplus >= 202002L // c++20 and newer
TEST_CASE("nonstrict", "[synchronized_value]")
{
    synchronized_value_nonstrict<Big, std::shared_mutex> syncval(13);
    REQUIRE(syncval.valueUnprotected().mVal == 13);
    syncval.valueUnprotected().mVal += 1;
    REQUIRE(syncval.valueUnprotected().mVal == 14);
}
#endif


TEST_CASE("parallel", "[synchronized_value]")
{
    constexpr int iterations = 1000000;
    synchronized_value<Big, std::shared_mutex> syncval1{0};
    synchronized_value<Big, std::shared_mutex> syncval2{0};

    auto f_get = [&]{
        return synchronize(
            syncval1.reader(), syncval2.reader(),
            [](const Big& val1, const Big& val2){
                return std::make_tuple(val1.mVal, val2.mVal);
            });
    };
    REQUIRE(f_get() == std::make_tuple(0, 0));

    auto f_sleep = [](int ms){
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    };
    
    auto a1 = std::async([&]{
        DBG("async +1 +1 begin");
        for (int i=0; i<iterations; i++) {
            // DBG("async +1 +1 i:"<<i);
            synchronize(
                syncval1.writer(), syncval2.writer(),
                [&](Big& val1, Big& val2){
                    val1.mVal += 1;
                    val2.mVal += 1;
                    // f_sleep(10);
                });
        }
        DBG("async +1 +1 end");
    });
    auto a2 = std::async([&]{
        DBG("async +1 0 begin");
        for (int i=0; i<iterations; i++) {
            // DBG("async +1 0 i:"<<i);
            synchronize(
                syncval1.writer(),
                [&](Big& val1){
                    val1.mVal += 1;
                    // f_sleep(20);
                });
        }
        DBG("async +1 0 end");
    });
    auto a3 = std::async([&]{
        DBG("async 0 +1 begin");
        for (int i=0; i<iterations; i++) {
            // DBG("async 0 +1 i:"<<i);
            synchronize(
                syncval2.writer(),
                [&](Big& val2){
                    val2.mVal += 1;
                    // f_sleep(10);
                });
        }
        DBG("async 0 +1 end");
    });
    auto a4 = std::async([&]{
        DBG("async read begin");
        for (int i=0; i<10; i++) {
            auto vals = f_get();
            DBG("async read vals:"<<std::get<0>(vals)<<" "<<std::get<1>(vals));
            f_sleep(1);
        }
        DBG("async read end");
    });
    a1.wait();
    a2.wait();
    a3.wait();
    a4.wait();
    REQUIRE(f_get() == std::make_tuple(2*iterations, 2*iterations));
}