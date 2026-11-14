// Single-threaded poll() event loop with timers and a thread-safe task queue.
//
// All sockets, HTTP/WebSocket connections, endpoints and the node runner live on ONE loop thread.
// Other threads (audio callbacks, Windows serial reader threads, DNS workers, API callers) talk to
// the loop only through post(), which is thread-safe and wakes the loop.
//
// Callbacks may freely add/remove watches and timers (including their own) while being dispatched.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "host/net.h"

namespace ember_host {

class Loop {
 public:
  using Task = std::function<void()>;
  using IoFn = std::function<void(short revents)>;

  Loop();
  ~Loop();
  Loop(const Loop&) = delete;
  Loop& operator=(const Loop&) = delete;

  // I/O watches (loop thread only). `events` is a mask of kPollIn / kPollOut.
  uint64_t watch(sock_t fd, short events, IoFn fn);
  void set_events(uint64_t id, short events);
  void unwatch(uint64_t id);

  // One-shot timer (loop thread only). Returns id for cancel().
  uint64_t timer(uint64_t delay_ms, Task fn);
  void cancel(uint64_t timer_id);

  // Thread-safe: queue a task for the loop thread and wake it.
  void post(Task fn);
  void wake() { waker_.wake(); }

  // Hook run at the start of every iteration on the loop thread; returns the max time (ms) the loop
  // may sleep (e.g. until the node's next tick). Used by the node runner.
  void set_prepare(std::function<int64_t()> fn) { prepare_ = std::move(fn); }

  // Runs one iteration: prepare hook, poll up to `max_wait_ms` (-1 = until something happens),
  // dispatch I/O, expired timers and posted tasks.
  void run_once(int64_t max_wait_ms);
  // Runs until stop() (thread-safe).
  void run();
  void stop();
  bool stopping() const { return stop_.load(); }
  void reset_stop() { stop_ = false; }

  // Marks the calling thread as the loop thread (run() does this automatically).
  void bind_thread() { owner_.store(std::this_thread::get_id()); }
  // No thread owns the loop (until run()/bind_thread()): calls from any thread are posted. Used
  // before spawning the loop thread so the spawner never races it.
  void unbind_thread() { owner_.store(std::thread::id()); }
  bool in_loop_thread() const { return owner_.load() == std::this_thread::get_id(); }
  size_t watch_count() const { return watches_.size(); }

 private:
  struct Watch {
    sock_t fd;
    short events;
    IoFn fn;
  };
  void run_tasks();
  int64_t run_timers();  // returns ms until next timer, -1 if none

  Waker waker_;
  std::map<uint64_t, Watch> watches_;
  std::multimap<uint64_t, uint64_t> timer_order_;  // due_ms -> id
  std::map<uint64_t, std::pair<uint64_t, Task>> timers_;  // id -> (due, fn)
  uint64_t next_id_ = 1;
  std::mutex task_mu_;
  std::vector<Task> tasks_;
  std::function<int64_t()> prepare_;
  std::atomic<bool> stop_{false};
  std::atomic<std::thread::id> owner_;
};

}  // namespace ember_host
