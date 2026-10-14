#include <cassert>
#include <cstddef>
#include <cstdint>

#include "pinyin_candidate_index.h"

namespace {

bool contains(PinyinCandidateSpan span, uint16_t codepoint)
{
    for (size_t index = 0; index < span.count; ++index) {
        if (span.codepoints[index] == codepoint) {
            return true;
        }
    }
    return false;
}

size_t position(PinyinCandidateSpan span, uint16_t codepoint)
{
    for (size_t index = 0; index < span.count; ++index) {
        if (span.codepoints[index] == codepoint) {
            return index;
        }
    }
    return span.count;
}

void assert_first_page(const char *syllable, uint16_t codepoint)
{
    assert(position(pinyin_candidate_lookup(syllable), codepoint) < 8U);
}

}  // namespace

int main()
{
    const PinyinCandidateSpan zhong = pinyin_candidate_lookup("zhong");
    assert(zhong.count > 0U);
    assert(contains(zhong, 0x4E2DU));  // 中
    assert(zhong.codepoints[0] == 0x4E2DU);  // 中, core candidate
    assert_first_page("zhong", 0x91CDU);  // 重, 体重

    const PinyinCandidateSpan chan = pinyin_candidate_lookup("chan");
    assert(contains(chan, 0x4EA7U));  // 产
    assert(chan.codepoints[0] == 0x4EA7U);
    const PinyinCandidateSpan yun = pinyin_candidate_lookup("yun");
    assert(contains(yun, 0x5B55U));  // 孕
    assert(yun.codepoints[0] == 0x5B55U);
    const PinyinCandidateSpan jian = pinyin_candidate_lookup("jian");
    assert(contains(jian, 0x7877U));  // 硷, fallback coverage
    const PinyinCandidateSpan xi = pinyin_candidate_lookup("xi");
    assert(contains(xi, 0x77FDU));  // 矽, fallback coverage
    const PinyinCandidateSpan huang = pinyin_candidate_lookup("huang");
    assert(contains(huang, 0x78FAU));  // 磺, fallback coverage

    assert(pinyin_candidate_lookup("de").codepoints[0] == 0x7684U);  // 的
    assert(pinyin_candidate_lookup("shi").codepoints[0] == 0x662FU);  // 是
    assert_first_page("qi", 0x671FU);    // 期, 孕期
    assert_first_page("jian", 0x68C0U);  // 检, 产检
    assert_first_page("ti", 0x4F53U);    // 体, 体重
    assert_first_page("tai", 0x80CEU);   // 胎, 胎动
    assert_first_page("dong", 0x52A8U);  // 动, 胎动
    assert_first_page("bei", 0x5907U);   // 备, 备忘
    assert_first_page("wang", 0x5FD8U);  // 忘, 备忘
    assert_first_page("lu", 0x5F55U);    // 录, 记录
    assert_first_page("fu", 0x590DU);    // 复, 复诊
    assert_first_page("li", 0x79BBU);    // 离, 离线
    assert_first_page("xian", 0x7EBFU);  // 线, 离线

    assert(pinyin_candidate_lookup(nullptr).count == 0U);
    assert(pinyin_candidate_lookup("").count == 0U);
    assert(pinyin_candidate_lookup("ZHONG").count == 0U);
    assert(pinyin_candidate_lookup("zhong1").count == 0U);
    assert(pinyin_candidate_lookup("zhongg").count == 0U);
    assert(pinyin_candidate_lookup("zzzzzz").count == 0U);
    assert(pinyin_candidate_lookup("notapinyin").count == 0U);

    assert(pinyin_candidate_key_count() == 394U);
    assert(pinyin_candidate_reference_count() == 3757U);
    assert(pinyin_candidate_resource_bytes() == 11552U);
    assert(pinyin_candidate_maximum_count() == 54U);
    assert(pinyin_candidate_ranking_version() == 2U);
    assert(pinyin_candidate_domain_benchmark_count() == 122U);
    assert(pinyin_candidate_domain_first_page_count() == 122U);
    assert(pinyin_candidate_domain_maximum_rank() == 5U);
}
