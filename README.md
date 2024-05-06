![workflow](https://github.com/michalfapso/synchronized_value/actions/workflows/cmake-multi-platform.yml/badge.svg)

`synchronized_value` is a C++ header-only wrapper class for protecting data with a mutex.

# Requirements

- C++17 compiler
- For tests:
  - CMake
  - Catch2 (automatically fetched by CMakeLists.txt)

# Usage

```cpp
#include "synchronized_value.h"
```

Accessing a single value:
```cpp
// Wrapping an int protected by an std::mutex (by default)
synchronized_value<int> syncval{10};
// The wrapped int can be accessed only inside synchronize():
synchronize(syncval,
    [](int& val){
        val += 1;
    });
```

Using `std::shared_mutex` and returning a value back to the caller:
```cpp
synchronized_value<int, std::shared_mutex> syncval{10};
int res = synchronize(
    syncval.unique(), // or just syncval (unique is default), or for a read-only access use syncval.shared()
    [](int& val){
        val += 1;
        return val;
    });
std::cout << "res: "<<res << std::endl;
```

Accessing multiple values:
```cpp
synchronized_value<int, std::shared_mutex> syncval1{10};
synchronized_value<int, std::shared_mutex> syncval2{20};
auto res = synchronize(
    syncval1.unique(), syncval2.shared(),
    [](int& val1, const int& val2){
        val1 += val2;
        return std::make_tuple(val1, val2);
    });
std::cout << "res: "<<std::get<0>(res)<<" "<<std::get<1>(res) << std::endl;
```

For direct access to the wrapped value, use `synchronized_value_nonstrict`:
```cpp
synchronized_value_nonstrict<int, std::shared_mutex> syncval{10};
syncval.valueUnprotected() += 1;
```

For more usage examples, see `synchronized_value_test.cpp`

# Tests
```
mkdir build
cd build
cmake ..
```
Now on Windows with MSYS2:
```
ninja
./tests.exe
```
And on Linux and macOS:
```
make
./tests
```
