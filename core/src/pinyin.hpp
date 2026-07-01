// Pinyin input parsing: syllable table + exact segmentation. No fuzzy rules.
#pragma once

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ime {

// Valid syllables are derived from the dictionary keys at load time, so the
// segmenter always agrees with the data it searches.
struct SylTable {
    std::unordered_set<std::string> full;    // complete syllables ("zhong")
    std::unordered_set<std::string> prefix;  // proper prefixes of syllables ("zh", "zho")
    int maxLen = 0;

    void add(const std::string& syl);
    bool isFull(const std::string& s) const { return full.count(s) != 0; }
    bool isPrefix(const std::string& s) const { return prefix.count(s) != 0; }
};

struct ParsedInput {
    bool ok = false;
    std::string letters;         // raw input with apostrophes removed
    std::vector<int> rawPos;     // rawPos[i] = index in raw of letters[i]
    int rawLen = 0;
    std::vector<bool> boundary;  // boundary[i]: apostrophe forces a split before letters[i]

    // Canonical greedy longest-match segmentation.
    std::vector<std::pair<int, int>> segs;  // [start,end) ranges into letters
    bool lastIncomplete = false;            // last seg is a syllable prefix, not a full syllable
    std::string segmented;                  // display form: "zhong'gu"
    std::string canonicalKey;               // == segmented, set only when fully complete

    // True if a syllable may span [start,end) without crossing a forced boundary.
    bool spanAllowed(int start, int end) const {
        for (int i = start + 1; i < end; ++i)
            if (boundary[i]) return false;
        return true;
    }
    // Raw chars consumed by a match covering letters[0..q). Swallows separators.
    int consumedAt(int q) const {
        return q >= static_cast<int>(letters.size()) ? rawLen : rawPos[q];
    }
};

// raw: lowercase a-z and apostrophes only; anything else -> ok=false.
ParsedInput parseInput(const std::string& raw, const SylTable& t, int maxSyllables);

}  // namespace ime
