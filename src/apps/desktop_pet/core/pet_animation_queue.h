#pragma once

#include <cstdint>

// Fixed-size non-blocking queue adapted from esp32-artoria-tamagotchi.
// 改编自esp32-artoria-tamagotchi的定长非阻塞动画队列。

constexpr uint8_t kPetAnimationQueueCapacity = 16U;

enum class PetAnimationNodeType : uint8_t {
    Pose,
    Overlay,
    Evolution,
    Delay,
    WaitInput,
    Callback,
};

using PetAnimationCallback = void (*)();

struct PetAnimationNode {
    PetAnimationNodeType type = PetAnimationNodeType::Pose;
    uint16_t asset_id = 0U;
    uint16_t duration_ms = 0U;
    uint8_t frame_count = 1U;
    uint16_t frame_delay_ms = 0U;
    PetAnimationCallback on_start = nullptr;
    PetAnimationCallback on_complete = nullptr;
};

class PetAnimationQueue {
public:
    void reset();
    bool enqueue(const PetAnimationNode &node, uint32_t now_ms);
    void update(uint32_t now_ms);
    void resume(uint32_t now_ms);
    void skip(uint32_t now_ms);

    bool empty() const;
    bool playing() const;
    bool waiting_for_input() const;
    uint8_t size() const;
    const PetAnimationNode *current() const;
    uint8_t current_frame(uint32_t now_ms) const;

private:
    void start_current(uint32_t now_ms);
    void advance(uint32_t now_ms);
    uint32_t duration(const PetAnimationNode &node) const;

    PetAnimationNode nodes_[kPetAnimationQueueCapacity] = {};
    uint8_t head_ = 0U;
    uint8_t tail_ = 0U;
    uint8_t count_ = 0U;
    bool playing_ = false;
    bool waiting_ = false;
    uint32_t node_started_ms_ = 0U;
};
