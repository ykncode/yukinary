// ============================================================
// Yukinary Concurrent Garbage Collector (Mark and Sweep)
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

struct GcNode {
    void* ptr;
    size_t size;
    bool marked;
};

std::vector<GcNode*> gc_allocations;
std::mutex gc_mutex;
bool gc_running = false;

extern "C" {

void* yukinary_gc_alloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        GcNode* node = new GcNode{ptr, size, false};
        std::lock_guard<std::mutex> lock(gc_mutex);
        gc_allocations.push_back(node);
    }
    return ptr;
}

// Background thread loop
void gc_background_task() {
    while (gc_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep to not block CPU
        
        std::lock_guard<std::mutex> lock(gc_mutex);
        
        // 1. Mark phase (Mock root scanning for now)
        // Iterate through known stack/global roots and set node->marked = true
        for (auto node : gc_allocations) {
            // For simulation, we assume any node over certain conditions is unreachable
            // node->marked = ...
        }

        // 2. Sweep phase
        for (auto it = gc_allocations.begin(); it != gc_allocations.end(); ) {
            if (!(*it)->marked) {
                // Not marked -> Unreachable -> Sweep!
                // printf("[GC] Sweeping %zu bytes at %p\n", (*it)->size, (*it)->ptr);
                free((*it)->ptr);
                delete *it;
                it = gc_allocations.erase(it);
            } else {
                // Reset for next cycle
                (*it)->marked = false;
                ++it;
            }
        }
    }
}

void yukinary_gc_baslat() {
    if (!gc_running) {
        gc_running = true;
        std::thread gc_thread(gc_background_task);
        gc_thread.detach(); // Let it run concurrently in the background
    }
}

void yukinary_gc_durdur() {
    gc_running = false;
}

} // extern "C"
