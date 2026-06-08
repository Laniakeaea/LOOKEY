#include "windows-hook-session.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace lookey::functions::system::input_api {

WindowsHookSession::~WindowsHookSession() {
    stop();
}

#if defined(_WIN32)
bool WindowsHookSession::start(
    int hook_type,
    HOOKPROC hook_proc,
    std::function<void()> on_thread_started,
    std::function<void()> on_thread_stopped) {
    if (running_) {
        return true;
    }

    hook_type_ = hook_type;
    hook_proc_ = hook_proc;
    on_thread_started_ = std::move(on_thread_started);
    on_thread_stopped_ = std::move(on_thread_stopped);
    ready_ = false;
    start_success_ = false;
    worker_thread_id_.store(0);

    running_ = true;
    worker_thread_ = std::thread(&WindowsHookSession::run_loop, this);

    while (!ready_.load() && running_.load()) {
        SwitchToThread();
    }

    if (!start_success_.load()) {
        stop();
        return false;
    }

    return true;
}
#else
bool WindowsHookSession::start(
    int hook_type,
    void* hook_proc,
    std::function<void()> on_thread_started,
    std::function<void()> on_thread_stopped) {
    (void)hook_type;
    (void)hook_proc;
    (void)on_thread_started;
    (void)on_thread_stopped;
    return false;
}
#endif

void WindowsHookSession::stop() {
    if (!running_) {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        return;
    }

    running_ = false;

#if defined(_WIN32)
    const unsigned long thread_id = worker_thread_id_.load();
    if (thread_id != 0) {
        PostThreadMessageA(thread_id, WM_QUIT, 0, 0);
    }
#endif

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool WindowsHookSession::is_running() const {
    return running_.load();
}

#if defined(_WIN32)
void WindowsHookSession::run_loop() {
    worker_thread_id_.store(GetCurrentThreadId());

    if (on_thread_started_) {
        on_thread_started_();
    }

    HHOOK hook = SetWindowsHookExA(hook_type_, hook_proc_, GetModuleHandleA(nullptr), 0);
    if (hook == nullptr) {
        running_ = false;
        start_success_ = false;
        ready_ = true;
        if (on_thread_stopped_) {
            on_thread_stopped_();
        }
        worker_thread_id_.store(0);
        return;
    }

    start_success_ = true;
    ready_ = true;

    MSG message;
    while (running_.load() && GetMessageA(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    UnhookWindowsHookEx(hook);

    if (on_thread_stopped_) {
        on_thread_stopped_();
    }

    worker_thread_id_.store(0);
}
#endif

} // namespace lookey::functions::system::input_api
