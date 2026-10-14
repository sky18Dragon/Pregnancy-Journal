#include "pinyin_candidate_index.h"

#include <cstring>

#include "pinyin_index_data.h"

namespace {

constexpr size_t kMaximumSyllableBytes = 6U;

const char *entry_key(size_t index)
{
    return kPinyinGeneratedKeys + kPinyinGeneratedEntries[index].key_offset;
}

bool valid_syllable(const char *syllable)
{
    if (syllable == nullptr || syllable[0] == '\0') {
        return false;
    }
    size_t length = 0U;
    for (; syllable[length] != '\0'; ++length) {
        if (length >= kMaximumSyllableBytes || syllable[length] < 'a' ||
            syllable[length] > 'z') {
            return false;
        }
    }
    return length > 0U;
}

}  // namespace

PinyinCandidateSpan pinyin_candidate_lookup(const char *syllable)
{
    if (!valid_syllable(syllable)) {
        return {nullptr, 0U};
    }
    size_t first = 0U;
    size_t count = kPinyinGeneratedEntryCount;
    while (count > 0U) {
        const size_t step = count / 2U;
        const size_t index = first + step;
        const int comparison = std::strcmp(entry_key(index), syllable);
        if (comparison < 0) {
            first = index + 1U;
            count -= step + 1U;
        } else {
            count = step;
        }
    }
    if (first >= kPinyinGeneratedEntryCount ||
        std::strcmp(entry_key(first), syllable) != 0) {
        return {nullptr, 0U};
    }
    const PinyinGeneratedEntry &entry = kPinyinGeneratedEntries[first];
    return {kPinyinGeneratedCandidates + entry.candidate_offset,
            entry.candidate_count};
}

size_t pinyin_candidate_key_count()
{
    return kPinyinGeneratedEntryCount;
}

size_t pinyin_candidate_reference_count()
{
    return kPinyinGeneratedCandidateCount;
}

size_t pinyin_candidate_resource_bytes()
{
    return kPinyinGeneratedResourceBytes;
}

uint16_t pinyin_candidate_maximum_count()
{
    return kPinyinGeneratedMaximumCandidates;
}

uint16_t pinyin_candidate_ranking_version()
{
    return kPinyinGeneratedRankingVersion;
}

size_t pinyin_candidate_domain_benchmark_count()
{
    return kPinyinGeneratedDomainBenchmarkCount;
}

size_t pinyin_candidate_domain_first_page_count()
{
    return kPinyinGeneratedDomainFirstPageCount;
}

uint16_t pinyin_candidate_domain_maximum_rank()
{
    return kPinyinGeneratedDomainMaximumRank;
}
