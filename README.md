`synchronized_value` is a header-only wrapper class for protecting data with a mutex.

# Requirements


# Usage

```cpp
#include "synchronized_value.h"
```

Accessing a single value:
```cpp
synchronized_value<int, std::shared_mutex> syncval{10};
int res = synchronize(
    syncval.writer(), // or just syncval1 or syncval.reader() for a read-only access
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
    syncval1.writer(), syncval2.reader(), 
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