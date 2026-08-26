#pragma once

#include <cstddef>
#include <cstdint>

#include "pet_core.h"

// The selector structure and first phrase set are adapted from the
// MIT-licensed openclaw-tamagotchi DialogueBook.
// 选择器结构和首批语料改编自MIT许可的openclaw-tamagotchi DialogueBook。

enum class PetDialogueContext : uint8_t {
    Idle,
    Walk,
    Sleep,
    Hungry,
    Sad,
    Tired,
    Feed,
    Pet,
    Talk,
    Play,
    Clean,
    EvolutionReady,
    EvolutionComplete,
};

struct PetDialogueEntry {
    uint16_t id;
    PetDialogueContext context;
    PetLifeStage minimum_stage;
    PetLifeStage maximum_stage;
    uint8_t minimum_bond;
    uint8_t maximum_bond;
    const char *text;
};

// Chooses an eligible line and updates the five-entry recent history.
// 选择符合条件的对白，并更新最近五条的防重复记录。
const PetDialogueEntry *pet_dialogue_pick(PetCoreState &state,
                                          PetDialogueContext context,
                                          uint32_t random_value);

// Derives the most relevant dialogue context from the current pet state.
// 根据当前宠物状态推导最合适的对白场景。
PetDialogueContext pet_dialogue_context_for_state(const PetCoreState &state);

// Returns the immutable dialogue table and its number of entries.
// 返回不可变对白表及其条目数量。
const PetDialogueEntry *pet_dialogue_entries(size_t &count);

// Returns the flash bytes occupied by dialogue text, excluding table metadata.
// 返回对白文本占用的Flash字节数，不包含表结构元数据。
size_t pet_dialogue_text_bytes();
