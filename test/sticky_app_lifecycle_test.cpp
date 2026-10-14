#include <cassert>
#include <thread>
#include <chrono>
#include "sticky_app_lifecycle.h"
TickType_t xTaskGetTickCount() {
    return static_cast<TickType_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
void vTaskDelay(TickType_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
int main() {
    StickyAppLifecycle lifecycle;
    std::atomic<bool> stop{false};
    std::atomic<unsigned> work{0};
    std::thread owner([&] {
        while(!stop.load()) {
            sticky_app_lifecycle_checkpoint(lifecycle);
            ++work;
            std::this_thread::yield();
        }
    });
    for(unsigned i=0;i<100;++i) {
        assert(sticky_app_lifecycle_pause(lifecycle,reinterpret_cast<void *>(1),1000)==ESP_OK);
        const auto before=work.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(work.load()==before);
        sticky_app_lifecycle_resume(lifecycle);
        // Immediately pause again; a previous ACK must never grant ownership.
    }
    stop=true; sticky_app_lifecycle_resume(lifecycle); owner.join();
    assert(sticky_app_lifecycle_pause(lifecycle,reinterpret_cast<void *>(1),20)==ESP_ERR_TIMEOUT);
    assert(lifecycle.pause_requested.load()==0);
}
