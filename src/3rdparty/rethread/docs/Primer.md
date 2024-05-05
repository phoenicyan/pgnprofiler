#Getting started with rethread
`rethread` is all about cancelling functions. Cancellability is necessary for making of C++ thread objects [RAII](http://en.cppreference.com/w/cpp/language/raii)-compliant - to properly terminate a function invoked in a separate thread, such a function should be _cancellable_.

Cancellable function is a long or blocking function that should:

* Receive a reference to a `cancellation_token` from the caller
* Check the token state periodically
* Return if the token enters "cancelled" state
* Pass the token to other long or blocking functions it invokes

##Cancellation token
Cancellation mechanics is encapsulated by a `cancellation_token` object. The token can be in one of two states: "not cancelled" or "cancelled". The state of the token is controlled by the thread object it belongs to.

Typical usage:
```cpp
void do_work(const cancellation_token& token)
{
  std::unique_lock lock(_mutex);
  while (token) // is cancelled?
  {
    if (_tasks.empty())
    {
      rethread::wait(_condition, lock, token); // cancellable wait
      continue;
    }
    auto task = _tasks.front();
    // invoke task
  }
}
```
This example uses two main `cancellation_token` features:
#####Cancellation state checking
```cpp
while (token)
  // ...
```
Converting `cancellation_token` to `bool` is equivalent to the result of `!token.is_cancelled()`. It equals `true` as long as the token is "not cancelled". When some other thread cancels the token, it will return `false`, thus finishing the loop.
#####Interrupting blocking calls
```cpp
rethread::wait(_condition, lock, token);
```
Cancellation token implements a generic way to cancel arbitrary blocking calls. Out of the box rethread provides cancellable implementations of `condition_variable::wait`, `this_thread::sleep`, and UNIX `poll`.

##Writing cancellable functions
If a blocking function does not support cancellation yet, there are two possible cases:

1. If the function is blocked because of `condition_variable`, `this_thread::sleep` or a simple busy-loop - it has to be reimplemented to add cancellation support
2. If the function is blocked in some other call that can't be reimplemented - it is necessary to write proper `cancellation_handler` and set up proper wake-up mechanism

This guide covers option 1. Advanced guide about writing custom cancellation handlers can be found [advanced guide](AdvancedGuide.md).

Adding cancellability to the blocking function involves several steps:
* Adjust return value
* Add cancellation_token parameter
* Reimplement blocking parts
* Add dummy_cancellation_token if necessary

Let's add cancellation support to the `pop` method of the following `concurrent_queue` class:
```cpp
template <typename T>
class concurrent_queue
{
  std::mutex              _mutex;
  std::queue<T>           _queue;
  std::condition_variable _condition;

public:
  void push(const T& t)
  {
    std::unique_lock<std::mutex> l(_mutex);
    _queue.push(t);
    _condition.notify_one();
  }

  T pop()
  {
    std::unique_lock<std::mutex> l(_mutex);
    while (_queue.empty())
      _condition.wait(l);

    T result = std::move(_queue.front());
    _queue.pop();
    return result;
  }

  // ...
};
```
###Adjust return value
Function must have some reasonable value to return upon cancellation. In our case we'll use `optional<T>` template from boost (and hopefully C++17).
```cpp
std::optional<T> pop()
{
  std::unique_lock<std::mutex> l(_mutex);
  while (_queue.empty())
    _condition.wait(l);

  T result = std::move(_queue.front());
  _queue.pop();
  return result;
}
```
###Add cancellation_token parameter
Cancellable functions should accept cancellation_token by const reference. We'll make use of this constness a bit later.
```cpp
std::optional<T> pop(const cancellation_token& token)
{
  std::unique_lock<std::mutex> l(_mutex);
  while (_queue.empty())
    _condition.wait(l);

  T result = std::move(_queue.front());
  _queue.pop();
  return result;
}
```
###Reimplement blocking parts
In our example thread blocks in `condition_variable::wait`. For condition variables with C++11-like interface rethread provides cancellable wait as a free function. IMPORTANT NOTE: cancellable wait with predicate returns `bool`, similarly to predicate-based versions of `wait_for` and `wait_until` in regular `condition_variable`.
```cpp
std::optional<T> pop(const cancellation_token& token)
{
  std::unique_lock<std::mutex> l(_mutex);
  while (_queue.empty() && token)
    rethread::wait(_condition, l, token);

  if (_queue.empty())
    return nullopt;

  T result = std::move(_queue.front());
  _queue.pop();
  return result;
}
```
###Add dummy_cancellation_token if necessary
Sometimes it makes sense to have non-cancellable version of a function. Use cases:
* Adding cancellability to already existing API without changing its users
* Testing
* Sometimes it is the only way to satisfy caller invariants

The simplest way to add non-cancellable version of a function is to use dummy_cancellation_token as a default value. It is copyable implementation of cancellation_token that may never enter cancelled state.
```cpp
std::optional<T> pop(const cancellation_token& token = dummy_cancellation_token())
{
  std::unique_lock<std::mutex> l(_mutex);
  while (_queue.empty() && token)
    rethread::wait(_condition, l, token);

  if (_queue.empty())
    return nullopt;

  T result = std::move(_queue.front());
  _queue.pop();
  return result;
}
```
That's it! Now `concurrent_queue::pop` supports cancellation. There's only one thing left.
###The finishing touch
This code can be shortened by using predicate-based version of `rethread::wait`. It behaves similarly to usual predicate-based `condition_variable::wait`, but returns `bool` instead of `void`. Return value has the same meaning as the one of `condition_variable::wait_for`. Predicate-based version of `rethread::wait` is equivalent to:
```cpp
bool wait(Condition& cv, Lock& lock, const cancellation_token& token, Predicate predicate)
{
  while (!pred())
  {
    wait(cv, lock, token);

    if (!token)
      return pred();
  }
  return true;
}
```
Using this version of wait, we can shorten `pop` to:
```cpp
std::optional<T> pop(const cancellation_token& token = dummy_cancellation_token())
{
  std::unique_lock<std::mutex> l(_mutex);
  if (!rethread::wait(_condition, l, [&_queue] { return !_queue.empty(); }))
    return nullopt;

  T result = std::move(_queue.front());
  _queue.pop();
  return result;
}
```
###One important observation
Our implementation of `pop` exposes one important property - its behavior is consistent in case of cancellation.
* A lot of threads can wait in `pop` and if one of them gets cancelled - it will return empty `optional`, while others will continue to wait
* If new item appears in `concurrent_queue` before cancellation, thread will finish working with this item before handling cancellation request
* If cancellation was performed for some kind of 'task' - the same thread can later safely return to `pop`

##RAII threads
`rethread` also provides a RAII-compliant thread implementation. It stores a `cancellation_token` object and passes it to the invoked function. When `rethread::thread` object is destroyed, it's destructor at first cancels the stored token and then joins the underlying thread. When `rethread::thread` is used with a cancellable function, it has the following properties:

* Deterministic thread lifetime - no longer than `rethread::thread` object
* Exception safety - no need to explicitly call `join()` before destructor
* Predictable destruction time

###Thread example
Let's write an object that uses aforementioned `concurrent_queue` to asynchronously execute functions.
```cpp
class task_executor final
{
public:
  task_executor() :
    _thread(std::bind(&task_executor::thread_function, this, std::placeholders::_1))
  { }

  ~task_executor() = default;

  void add_task(const std::function<void()>& f)
  { _queue.push(f); }

private:
  concurrent_queue _queue;
  rethread::thread _thread;

  void thread_function(const rethread::cancellation_token& t)
  {
    while (t)
    {
      std::optional<std::function<void()>> f = _queue.pop(t);
      if (f)
        (*f)();
    }
  }
};
```
