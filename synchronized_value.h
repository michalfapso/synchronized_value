#include <mutex>
#include <shared_mutex>
#include <type_traits>

#if __cplusplus >= 202002L // c++20 and newer
    template<typename T>
    concept is_shared_lockable = requires(T val) {
        {val.lock_shared()};
        {val.unlock_shared()};
        {val.try_lock_shared()};
    };

    template<typename T>
    concept is_lockable = requires(T &val) {
        {val.lock()};
        {val.unlock()};
        {val.try_lock()} -> std::same_as<bool>;
    };
#else // before c++20:
    // is_shared_lockable
    template<typename, typename = void>
    constexpr bool is_shared_lockable = false;
     
    template<typename T>
    constexpr bool is_shared_lockable<
        T,
        std::void_t<decltype(std::declval<T>().lock_shared()),
                    decltype(std::declval<T>().unlock_shared()),
                    decltype(std::declval<T>().try_lock_shared())
        >
    > = true; 

    // is_lockable
    template<typename, typename = void>
    constexpr bool is_lockable = false;
     
    template<typename T>
    constexpr bool is_lockable<
        T,
        std::void_t<decltype(std::declval<T>().lock()),
                    decltype(std::declval<T>().unlock()),
                    decltype(std::declval<T>().try_lock())
        >
    > = true; 
#endif

#if __cplusplus >= 202002L // c++20 and newer
template<typename T, is_lockable Mutex = std::mutex, bool IsStrictlyProtected = true>
#else
template<typename T, typename    Mutex = std::mutex, bool IsStrictlyProtected = true, typename = std::enable_if_t<is_lockable<Mutex>>>
#endif
class synchronized_value
{
public:
    using value_type = T;
    using mutex_type = Mutex;
    static constexpr bool is_strictly_protected = IsStrictlyProtected;

    struct AccessUniqueConst {
        using syncval_type = synchronized_value<T, Mutex>; 
        using lock_type = std::unique_lock<typename syncval_type::mutex_type>;

        AccessUniqueConst(const syncval_type& val) : mSyncVal(val) {}
        const syncval_type& mSyncVal;
    };

    struct AccessUnique {
        using syncval_type = synchronized_value<T, Mutex>; 
        using lock_type = std::unique_lock<typename syncval_type::mutex_type>;

        AccessUnique(syncval_type& val) : mSyncVal(val) {}
        syncval_type& mSyncVal;
    };

    struct AccessShared {
        using syncval_type = synchronized_value<T, Mutex>; 
        using lock_type = std::shared_lock<typename syncval_type::mutex_type>;

        AccessShared(const syncval_type& val) : mSyncVal(val) {}
        const syncval_type& mSyncVal;
    };

    // Forward constructor args to mValue's constructor
    template <typename... Args>
    explicit synchronized_value(Args&&... args)
        : mValue(std::forward<Args>(args)...)
    {}

    synchronized_value(const synchronized_value&) = delete;
    synchronized_value& operator=(const synchronized_value&) = delete;

    synchronized_value(synchronized_value&& other) = delete;
    synchronized_value& operator=(synchronized_value&& other) = delete;

    mutex_type& mutex() const { return mMutex; }

#if __cplusplus >= 202002L // c++20 and newer
          value_type& valueUnprotected()       requires(!is_strictly_protected) { return mValue; }
    const value_type& valueUnprotected() const requires(!is_strictly_protected) { return mValue; }
#endif
    
          AccessShared      shared()       { return AccessShared     (*this); }
    const AccessShared      shared() const { return AccessShared     (*this); }
          AccessUnique      unique()       { return AccessUnique     (*this); }
    // const unique() with shared_mutex uses shared locking
    template <typename M = Mutex, typename = std::enable_if_t< is_shared_lockable<M>>>
    const AccessShared      unique() const { return AccessShared     (*this); }
    // const unique() with a standard mutex uses exclusive locking
    template <typename M = Mutex, typename = std::enable_if_t<!is_shared_lockable<M>>>
    const AccessUniqueConst unique() const { return AccessUniqueConst(*this); }
    
    // To be able to access private value() from within synchronize()
    template<class F, class Arg>
    friend
    auto synchronize(F&& f, Arg& value) -> std::invoke_result_t<F, typename Arg::syncval_type::value_type&>;
    
    template<class F, class ... Args>
    friend 
    auto synchronize(F&& f, Args&... values) -> std::invoke_result_t<F, typename Args::syncval_type::value_type&...>;

private:
          value_type& value()       { return mValue; }
    const value_type& value() const { return mValue; }

    mutable mutex_type mMutex;
    value_type mValue;
};

// Use _nonstrict when you need to access valueUnprotected()
template<typename T, typename Mutex = std::mutex>
using synchronized_value_nonstrict = synchronized_value<T, Mutex, false>;

// Single synchronized_value
// f: function with protected access to the value
// values: AccessShared | AccessUnique accessor
template<class F, class Arg>
auto synchronize(F&& f, Arg& value) -> std::invoke_result_t<F, typename Arg::syncval_type::value_type&>
{
    // Create lock guards to unlock mutexes automatically at the end of the scope
    auto lock = typename Arg::lock_type{value.mSyncVal.mutex()};
    
    // Call f() with the values
    return std::forward<F>(f)(value.mSyncVal.value());
}

// Multiple synchronized_values
// f: function with protected access to values
// values: AccessShared | AccessUnique accessors
template<class F, class ... Args>
auto synchronize(F&& f, Args&... values) -> std::invoke_result_t<F, typename Args::syncval_type::value_type&...>
{
    // Create lock guards to unlock mutexes automatically at the end of the scope
    auto locks = std::make_tuple(
        typename Args::lock_type{values.mSyncVal.mutex(), std::defer_lock}...);
    
    // Use std::apply to lock all mutexes atomically
    std::apply([](auto&... lock) { std::lock(lock...); }, locks);

    // Call f() with the values
    return std::forward<F>(f)(values.mSyncVal.value()...);
}

// f: function with protected access to values
// values: synchronized_value<> with AccessUnique accessor (unique_lock)
template<class F, class ... Args>
auto synchronize(F&& f, Args&... values) -> std::invoke_result_t<F, typename Args::value_type&...>
{
    return synchronize(std::forward<F>(f), values.unique()...);
}


// To pass synchronized_value params before the functor for increased code readability
// Source: How to Pass a Variadic Pack as the First Argument of a Function in C++ 
//         https://www.cppstories.com/2020/09/variadic-pack-first.html/
namespace detail
{
    template <typename... LocksThenF, size_t... LocksIndexes>
    auto synchronize(std::tuple<LocksThenF...> locksThenF, std::index_sequence<LocksIndexes...>)
    {
        auto constexpr FIndex = sizeof...(LocksThenF) - 1;
        return synchronize(std::get<FIndex>(locksThenF), std::get<LocksIndexes>(locksThenF)...);
    }
}
template<typename... LocksThenF>
auto synchronize(LocksThenF&&... locksThenF)
{
    return detail::synchronize(std::forward_as_tuple(locksThenF...), std::make_index_sequence<sizeof...(locksThenF) - 1>{});
}
