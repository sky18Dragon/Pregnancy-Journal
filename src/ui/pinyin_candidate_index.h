#pragma once

#include <cstddef>
#include <cstdint>

struct PinyinCandidateSpan {
    const uint16_t *codepoints;
    size_t count;
};

// Looks up one complete, lowercase, tone-free pinyin syllable. The returned
// span points into immutable generated Flash data and performs no allocation.
PinyinCandidateSpan pinyin_candidate_lookup(const char *syllable);

size_t pinyin_candidate_key_count();
size_t pinyin_candidate_reference_count();
size_t pinyin_candidate_resource_bytes();
uint16_t pinyin_candidate_maximum_count();
uint16_t pinyin_candidate_ranking_version();
size_t pinyin_candidate_domain_benchmark_count();
size_t pinyin_candidate_domain_first_page_count();
uint16_t pinyin_candidate_domain_maximum_rank();
