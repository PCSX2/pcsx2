/*
 * MIT License
 *
 * Copyright (c) 2022 Colion Braley
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// From https://raw.githubusercontent.com/cbraley/threadpool/master/src/thread_pool.h

#pragma once

// A simple thread pool class.
// Usage examples:
//
// {
//   ThreadPool pool(16);  // 16 worker threads.
//   for (int i = 0; i < 100; ++i) {
//     pool.Schedule([i]() {
//       DoSlowExpensiveOperation(i);
//     });
//   }
//
//   // `pool` goes out of scope here - the code will block in the ~ThreadPool
//   // destructor until all work is complete.
// }
//
// // TODO(cbraley): Add examples with std::future.

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// We want to use std::invoke if C++17 is available, and fallback to "hand
// crafted" code if std::invoke isn't available.
#if __cplusplus >= 201703L || defined(_MSC_VER)
        #define INVOKE_MACRO(CALLABLE, ARGS_TYPE, ARGS)  std::invoke(CALLABLE, std::forward<ARGS_TYPE>(ARGS)...)
#elif __cplusplus >= 201103L
  // Update this with http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4169.html.
        #define INVOKE_MACRO(CALLABLE, ARGS_TYPE, ARGS)  CALLABLE(std::forward<ARGS_TYPE>(ARGS)...)
#else
  #error ("C++ version is too old! C++98 is not supported.")
#endif

namespace cb {

class ThreadPool {
 public:
  // Create a thread pool with `num_workers` dedicated worker threads.
  explicit ThreadPool(int num_workers);

  // Default construction is disallowed.
  ThreadPool() = delete;

  // Get the number of logical cores on the CPU. This is implemented using
  // std::thread::hardware_concurrency().
  // https://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency
  static unsigned int GetNumLogicalCores();

  // The `ThreadPool` destructor blocks until all outstanding work is complete.
  ~ThreadPool();

  // No copying, assigning, or std::move-ing.
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  // Add the function `func` to the thread pool. `func` will be executed at some
  // point in the future on an arbitrary thread.
  void Schedule(std::function<void(void)> func);

  // Add `func` to the thread pool, and return a std::future that can be used to
  // access the function's return value.
  //
  // *** Usage example ***
  //   Don't be alarmed by this function's tricky looking signature - this is
  //   very easy to use. Here's an example:
  //
  //   int ComputeSum(std::vector<int>& values) {
  //     int sum = 0;
  //     for (const int& v : values) {
  //       sum += v;
  //     }
  //     return sum;
  //   }
  //
  //   ThreadPool pool = ...;
  //   std::vector<int> numbers = ...;
  //
  //   std::future<int> sum_future = ScheduleAndGetFuture(
  //     []() {
  //       return ComputeSum(numbers);
  //     });
  //
  //   // Do other work...
  //
  //   std::cout << "The sum is " << sum_future.get() << std::endl;
  //
  // *** Details ***
  //   Given a callable `func` that returns a value of type `RetT`, this
  //   function returns a std::future<RetT> that can be used to access
  //   `func`'s results.
  template <typename FuncT, typename... ArgsT>
  auto ScheduleAndGetFuture(FuncT&& func, ArgsT&&... args)
      -> std::future<decltype(INVOKE_MACRO(func, ArgsT, args))>;

  // Wait for all outstanding work to be completed.
  void Wait();

  // Return the number of outstanding functions to be executed.
  int OutstandingWorkSize() const;

  // Return the number of threads in the pool.
  int NumWorkers() const;

  void SetWorkDoneCallback(std::function<void(int)> func);

 private:
  void ThreadLoop();

  // Number of worker threads - fixed at construction time.
  int num_workers_;

  // The destructor sets `exit_` to true and then notifies all workers. `exit_`
  // causes each thread to break out of their work loop.
  bool exit_;

  mutable std::mutex mu_;

  // Work queue. Guarded by `mu_`.
  struct WorkItem {
    std::function<void(void)> func;
  };
  std::queue<WorkItem> work_;

  // Condition variable used to notify worker threads that new work is
  // available.
  std::condition_variable condvar_;

  // Worker threads.
  std::vector<std::thread> workers_;

  // Condition variable used to notify that all work is complete - the work
  // queue has "run dry".
  std::condition_variable work_done_condvar_;

  // Whenever a work item is complete, we call this callback. If this is empty,
  // nothing is done.
  std::function<void(int)> work_done_callback_;
};

namespace impl {

// This helper class simply returns a std::function that executes:
//   ReturnT x = func();
//   promise->set_value(x);
// However, this is tricky in the case where T == void. The code above won't
// compile if ReturnT == void, and neither will
//   promise->set_value(func());
// To workaround this, we use a template specialization for the case where
// ReturnT is void. If the "regular void" proposal is accepted, this could be
// simpler:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0146r1.html.

// The non-specialized `FuncWrapper` implementation handles callables that
// return a non-void value.
template <typename ReturnT>
struct FuncWrapper {
  template <typename FuncT, typename... ArgsT>
  std::function<void()> GetWrapped(
      FuncT&& func, std::shared_ptr<std::promise<ReturnT>> promise,
      ArgsT&&... args) {
    // TODO(cbraley): Capturing by value is inefficient. It would be more
    // efficient to move-capture everything, but we can't do this until C++14
    // generalized lambda capture is available. Can we use std::bind instead to
    // make this more efficient and still use C++11?
    return [promise, func, args...]() mutable {
      promise->set_value(INVOKE_MACRO(func, ArgsT, args));
    };
  }
};

template <typename FuncT, typename... ArgsT>
void InvokeVoidRet(FuncT&& func, std::shared_ptr<std::promise<void>> promise,
                   ArgsT&&... args) {
  INVOKE_MACRO(func, ArgsT, args);
  promise->set_value();
}

// This `FuncWrapper` specialization handles callables that return void.
template <>
struct FuncWrapper<void> {
  template <typename FuncT, typename... ArgsT>
  std::function<void()> GetWrapped(FuncT&& func,
                                   std::shared_ptr<std::promise<void>> promise,
                                   ArgsT&&... args) {
    return [promise, func, args...]() mutable {
      INVOKE_MACRO(func, ArgsT, args);
      promise->set_value();
    };
  }
};

}  // namespace impl

template <typename FuncT, typename... ArgsT>
auto ThreadPool::ScheduleAndGetFuture(FuncT&& func, ArgsT&&... args)
    -> std::future<decltype(INVOKE_MACRO(func, ArgsT, args))> {
  using ReturnT = decltype(INVOKE_MACRO(func, ArgsT, args));

  // We are only allocating this std::promise in a shared_ptr because
  // std::promise is non-copyable.
  std::shared_ptr<std::promise<ReturnT>> promise =
      std::make_shared<std::promise<ReturnT>>();
  std::future<ReturnT> ret_future = promise->get_future();

  impl::FuncWrapper<ReturnT> func_wrapper;
  std::function<void()> wrapped_func = func_wrapper.GetWrapped(
      std::move(func), std::move(promise), std::forward<ArgsT>(args)...);

  // Acquire the lock, and then push the WorkItem onto the queue.
  {
    std::lock_guard<std::mutex> scoped_lock(mu_);
    WorkItem work;
    work.func = std::move(wrapped_func);
    work_.emplace(std::move(work));
  }
  condvar_.notify_one();  // Tell one worker we are ready.
  return ret_future;
}

}  // namespace cb

#undef INVOKE_MACRO
