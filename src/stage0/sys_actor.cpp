// ============================================================
// Yukinary Native Actor Engine (Green Threads & Message Passing)
// ============================================================

#include <stdio.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <map>
#include <condition_variable>

struct Message {
    int sender_id;
    char payload[256];
};

struct Actor {
    int id;
    std::queue<Message> mailbox;
    std::mutex mailbox_mutex;
    std::condition_variable cv;
    bool is_alive;
    // Pointer to the YKN function that represents the actor's behavior
    void* function_ptr; 
};

std::map<int, Actor*> actor_registry;
std::mutex registry_mutex;
int next_actor_id = 1;

extern "C" {

int yukinary_actor_spawn(void* func_ptr) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    int id = next_actor_id++;
    
    Actor* act = new Actor();
    act->id = id;
    act->is_alive = true;
    act->function_ptr = func_ptr;
    
    actor_registry[id] = act;
    
    // In a real implementation, we submit this Actor to a Worker Thread Pool
    // For now, we simulate starting an OS thread per actor (like Erlang's early days)
    // std::thread([act]() {
    //     // Call func_ptr inside the actor's context
    // }).detach();

    return id;
}

int yukinary_actor_send(int target_id, int sender_id, const char* msg) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    if (actor_registry.find(target_id) != actor_registry.end()) {
        Actor* act = actor_registry[target_id];
        std::lock_guard<std::mutex> act_lock(act->mailbox_mutex);
        
        Message m;
        m.sender_id = sender_id;
        strncpy(m.payload, msg, 255);
        
        act->mailbox.push(m);
        act->cv.notify_one(); // Wake up the actor if it's waiting
        return 1;
    }
    return 0; // Actor not found or dead
}

// Blocks until a message is received (Pattern Matching simulation)
int yukinary_actor_receive(int my_id, char* out_msg_buffer) {
    Actor* act = nullptr;
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        if (actor_registry.find(my_id) == actor_registry.end()) return 0;
        act = actor_registry[my_id];
    }
    
    std::unique_lock<std::mutex> act_lock(act->mailbox_mutex);
    act->cv.wait(act_lock, [act]{ return !act->mailbox.empty() || !act->is_alive; });
    
    if (!act->is_alive) return 0;
    
    Message m = act->mailbox.front();
    act->mailbox.pop();
    
    strcpy(out_msg_buffer, m.payload);
    return m.sender_id;
}

} // extern "C"
