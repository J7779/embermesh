#include "host/loop.h"

#include "host/util.h"

namespace ember_host {

Loop::Loop() {
  net_init();
  owner_.store(std::this_thread::get_id());
}

Loop::~Loop() {
  // Pending callbacks may own objects whose destructors call back into the loop (cancel/unwatch):
  // move them out first so those calls see valid, empty containers.
  for (int round = 0; round < 4; ++round) {
    auto t = std::move(timers_);
    timers_.clear();
    timer_order_.clear();
    std::vector<Task> tasks;
    {
      std::lock_guard<std::mutex> lk(task_mu_);
      tasks.swap(tasks_);
    }
    if (t.empty() && tasks.empty()) break;
  }
}

uint64_t Loop::watch(sock_t fd, short events, IoFn fn) {
  uint64_t id = next_id_++;
  watches_[id] = Watch{fd, events, std::move(fn)};
  return id;
}

void Loop::set_events(uint64_t id, short events) {
  auto it = watches_.find(id);
  if (it != watches_.end()) it->second.events = events;
}

void Loop::unwatch(uint64_t id) { watches_.erase(id); }

uint64_t Loop::timer(uint64_t delay_ms, Task fn) {
  uint64_t id = next_id_++;
  uint64_t due = steady_ms() + delay_ms;
  timers_[id] = {due, std::move(fn)};
  timer_order_.emplace(due, id);
  return id;
}

void Loop::cancel(uint64_t timer_id) { timers_.erase(timer_id); }

void Loop::post(Task fn) {
  {
    std::lock_guard<std::mutex> lk(task_mu_);
    tasks_.push_back(std::move(fn));
  }
  waker_.wake();
}

void Loop::stop() {
  stop_ = true;
  waker_.wake();
}

void Loop::run_tasks() {
  std::vector<Task> t;
  {
    std::lock_guard<std::mutex> lk(task_mu_);
    t.swap(tasks_);
  }
  for (auto& f : t)
    if (f) f();
}

int64_t Loop::run_timers() {
  uint64_t now = steady_ms();
  // Collect due timers first so callbacks that add timers with 0 delay don't starve I/O.
  std::vector<uint64_t> due;
  while (!timer_order_.empty() && timer_order_.begin()->first <= now) {
    due.push_back(timer_order_.begin()->second);
    timer_order_.erase(timer_order_.begin());
  }
  for (uint64_t id : due) {
    auto it = timers_.find(id);
    if (it == timers_.end()) continue;  // cancelled
    Task fn = std::move(it->second.second);
    timers_.erase(it);
    if (fn) fn();
  }
  // Drop stale order entries of cancelled timers lazily.
  while (!timer_order_.empty() && timers_.find(timer_order_.begin()->second) == timers_.end())
    timer_order_.erase(timer_order_.begin());
  if (timer_order_.empty()) return -1;
  uint64_t next = timer_order_.begin()->first;
  now = steady_ms();
  return next <= now ? 0 : int64_t(next - now);
}

void Loop::run_once(int64_t max_wait_ms) {
  int64_t wait = max_wait_ms;
  if (prepare_) {
    int64_t p = prepare_();
    if (p >= 0 && (wait < 0 || p < wait)) wait = p;
  }
  int64_t t = run_timers();
  if (t >= 0 && (wait < 0 || t < wait)) wait = t;
  {
    std::lock_guard<std::mutex> lk(task_mu_);
    if (!tasks_.empty()) wait = 0;
  }
  if (stop_) wait = 0;
  if (wait > 60000) wait = 60000;

  std::vector<PollItem> items;
  std::vector<uint64_t> ids;
  items.reserve(watches_.size() + 1);
  ids.reserve(watches_.size() + 1);
  items.push_back(PollItem{waker_.fd(), kPollIn, 0});
  ids.push_back(0);
  for (auto& kv : watches_) {
    if (kv.second.fd == kInvalidSock) continue;
    items.push_back(PollItem{kv.second.fd, kv.second.events, 0});
    ids.push_back(kv.first);
  }
  int rc = poll_sockets(items.data(), items.size(), int(wait < 0 ? -1 : wait));
  if (rc > 0) {
    if (items[0].revents) waker_.drain();
    for (size_t i = 1; i < items.size(); ++i) {
      short re = items[i].revents;
      if (!re) continue;
      auto it = watches_.find(ids[i]);
      if (it == watches_.end()) continue;  // removed by an earlier callback this round
      // Only deliver what was asked for, plus errors/hangups.
      short mask = short(it->second.events | kPollErr | kPollHup);
      re = short(re & mask);
      if (!re) continue;
      IoFn fn = it->second.fn;  // copy: the callback may unwatch itself
      fn(re);
    }
  } else if (rc < 0) {
    EH_LOGW("poll failed: %s", sock_strerror(sock_error()).c_str());
  }
  run_tasks();
  run_timers();
}

void Loop::run() {
  bind_thread();
  while (!stop_) run_once(-1);
  // Final drain so posted shutdown tasks run.
  run_tasks();
}

}  // namespace ember_host
