#include "pet_animation_queue.h"

#include <algorithm>

void PetAnimationQueue::reset()
{
    head_ = 0U;
    tail_ = 0U;
    count_ = 0U;
    playing_ = false;
    waiting_ = false;
    node_started_ms_ = 0U;
}

bool PetAnimationQueue::enqueue(const PetAnimationNode &node, uint32_t now_ms)
{
    if (count_ >= kPetAnimationQueueCapacity) {
        return false;
    }
    nodes_[tail_] = node;
    tail_ = static_cast<uint8_t>((tail_ + 1U) % kPetAnimationQueueCapacity);
    ++count_;
    if (!playing_) {
        start_current(now_ms);
    }
    return true;
}

void PetAnimationQueue::update(uint32_t now_ms)
{
    if (!playing_ || waiting_ || count_ == 0U) {
        return;
    }
    if (now_ms - node_started_ms_ >= duration(nodes_[head_])) {
        advance(now_ms);
    }
}

void PetAnimationQueue::resume(uint32_t now_ms)
{
    if (waiting_) {
        waiting_ = false;
        advance(now_ms);
    }
}

void PetAnimationQueue::skip(uint32_t now_ms)
{
    if (playing_ && !waiting_) {
        advance(now_ms);
    }
}

bool PetAnimationQueue::empty() const
{
    return count_ == 0U;
}

bool PetAnimationQueue::playing() const
{
    return playing_;
}

bool PetAnimationQueue::waiting_for_input() const
{
    return waiting_;
}

uint8_t PetAnimationQueue::size() const
{
    return count_;
}

const PetAnimationNode *PetAnimationQueue::current() const
{
    return playing_ && count_ > 0U ? &nodes_[head_] : nullptr;
}

uint8_t PetAnimationQueue::current_frame(uint32_t now_ms) const
{
    const PetAnimationNode *node = current();
    if (node == nullptr || node->frame_count <= 1U ||
        node->frame_delay_ms == 0U) {
        return 0U;
    }
    const uint32_t frame =
        (now_ms - node_started_ms_) / node->frame_delay_ms;
    return static_cast<uint8_t>(std::min<uint32_t>(
        frame, static_cast<uint32_t>(node->frame_count - 1U)));
}

void PetAnimationQueue::start_current(uint32_t now_ms)
{
    if (count_ == 0U) {
        playing_ = false;
        waiting_ = false;
        return;
    }
    playing_ = true;
    node_started_ms_ = now_ms;
    waiting_ = nodes_[head_].type == PetAnimationNodeType::WaitInput;
    if (nodes_[head_].on_start != nullptr) {
        nodes_[head_].on_start();
    }
}

void PetAnimationQueue::advance(uint32_t now_ms)
{
    if (count_ == 0U) {
        return;
    }
    PetAnimationNode &finished = nodes_[head_];
    if (finished.on_complete != nullptr) {
        finished.on_complete();
    }
    head_ = static_cast<uint8_t>((head_ + 1U) % kPetAnimationQueueCapacity);
    --count_;
    start_current(now_ms);
}

uint32_t PetAnimationQueue::duration(const PetAnimationNode &node) const
{
    if (node.duration_ms > 0U) {
        return node.duration_ms;
    }
    if (node.type == PetAnimationNodeType::WaitInput) {
        return UINT32_MAX;
    }
    if (node.type == PetAnimationNodeType::Callback) {
        return 1U;
    }
    if (node.frame_count > 0U && node.frame_delay_ms > 0U) {
        return static_cast<uint32_t>(node.frame_count) *
               node.frame_delay_ms;
    }
    return 500U;
}
