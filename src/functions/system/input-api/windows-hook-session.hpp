#pragma once

#include <atomic>
#include <functional>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace lookey::functions::system::input_api {

class WindowsHookSession {
public:
    WindowsHookSession() = default;
    ~WindowsHookSession();

#if defined(_WIN32)
    bool start(
        int hook_type,
        HOOKPROC hook_proc,
        std::function<void()> on_thread_started,
        std::function<void()> on_thread_stopped);
#else
    bool start(
        int hook_type,
        void* hook_proc,
        std::function<void()> on_thread_started,
        std::function<void()> on_thread_stopped);
#endif

    void stop();
    [[nodiscard]] bool is_running() const;

private:
#if defined(_WIN32)
    void run_loop();

    int hook_type_{0};
    HOOKPROC hook_proc_{nullptr};
#else
    int hook_type_{0};
    void* hook_proc_{nullptr};
#endif

    std::function<void()> on_thread_started_;
    std::function<void()> on_thread_stopped_;

    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> start_success_{false};
    std::thread worker_thread_;
    std::atomic<unsigned long> worker_thread_id_{0};
};

} // namespace lookey::functions::system::input_api
