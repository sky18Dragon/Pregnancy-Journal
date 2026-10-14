#pragma once

#include <cstddef>
#include <cstdint>

#include "checkup.h"

class CheckupService {
public:
    static constexpr size_t kCapacity = 8U;
    bool create(Checkup item, uint32_t *created_id = nullptr);
    bool update(const Checkup &item);
    bool remove(uint32_t id);
    bool complete(uint32_t id);
    void clear();
    bool set_next_id_floor(uint64_t next_id);
    size_t count() const { return count_; }
    uint32_t next_id() const { return next_id_; }
    const Checkup *at(size_t index) const;
    Checkup *find(uint32_t id);
    const Checkup *next(uint32_t now, uint32_t *epoch = nullptr) const;

private:
    Checkup items_[kCapacity] = {};
    size_t count_ = 0U;
    uint32_t next_id_ = 1U;
};
