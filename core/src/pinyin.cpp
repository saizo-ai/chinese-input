#include "pinyin.hpp"

namespace ime {

void SylTable::add(const std::string& syl) {
    if (syl.empty()) return;
    full.insert(syl);
    if (static_cast<int>(syl.size()) > maxLen) maxLen = static_cast<int>(syl.size());
    for (size_t len = 1; len < syl.size(); ++len)
        prefix.insert(syl.substr(0, len));
}

namespace {

// Greedy longest-match with backtracking. Returns true if letters[pos..n) can
// be fully segmented; appends ranges to segs. Longest syllable wins at each
// position; alternatives are only tried when the remainder would dead-end.
bool segmentFrom(const std::string& letters, const ParsedInput& in, const SylTable& t,
                 int pos, int maxSyllables, std::vector<std::pair<int, int>>& segs) {
    int n = static_cast<int>(letters.size());
    if (pos == n) return true;
    if (static_cast<int>(segs.size()) >= maxSyllables) return false;
    int maxEnd = std::min(n, pos + t.maxLen);
    for (int end = maxEnd; end > pos; --end) {
        if (!in.spanAllowed(pos, end)) continue;
        if (!t.isFull(letters.substr(pos, end - pos))) continue;
        segs.emplace_back(pos, end);
        if (segmentFrom(letters, in, t, end, maxSyllables, segs)) return true;
        segs.pop_back();
    }
    return false;
}

// Fallback: full segmentation failed; allow the final segment to be a syllable
// prefix (the character still being typed).
bool segmentWithTail(const std::string& letters, const ParsedInput& in, const SylTable& t,
                     int pos, int maxSyllables, std::vector<std::pair<int, int>>& segs,
                     bool& lastIncomplete) {
    int n = static_cast<int>(letters.size());
    if (pos == n) return true;
    if (static_cast<int>(segs.size()) >= maxSyllables) return false;
    // Tail as incomplete prefix (only valid if it reaches the end of input).
    auto tryTail = [&]() {
        if (!in.spanAllowed(pos, n)) return false;
        std::string tail = letters.substr(pos, n - pos);
        if (t.isPrefix(tail)) {
            segs.emplace_back(pos, n);
            lastIncomplete = true;
            return true;
        }
        return false;
    };
    int maxEnd = std::min(n, pos + t.maxLen);
    for (int end = maxEnd; end > pos; --end) {
        if (!in.spanAllowed(pos, end)) continue;
        if (!t.isFull(letters.substr(pos, end - pos))) continue;
        segs.emplace_back(pos, end);
        if (segmentWithTail(letters, in, t, end, maxSyllables, segs, lastIncomplete)) return true;
        segs.pop_back();
    }
    return tryTail();
}

}  // namespace

ParsedInput parseInput(const std::string& raw, const SylTable& t, int maxSyllables) {
    ParsedInput in;
    in.rawLen = static_cast<int>(raw.size());
    if (raw.empty()) return in;

    bool pendingBoundary = false;
    for (int i = 0; i < in.rawLen; ++i) {
        char c = raw[i];
        if (c == '\'') {
            pendingBoundary = true;
            continue;
        }
        if (c < 'a' || c > 'z') return in;  // reject anything but pinyin letters
        in.boundary.push_back(pendingBoundary && !in.letters.empty());
        pendingBoundary = false;
        in.letters.push_back(c);
        in.rawPos.push_back(i);
    }
    if (in.letters.empty()) return in;

    // Prefer a fully-complete segmentation; otherwise allow an incomplete tail.
    if (!segmentFrom(in.letters, in, t, 0, maxSyllables, in.segs)) {
        in.segs.clear();
        if (!segmentWithTail(in.letters, in, t, 0, maxSyllables, in.segs, in.lastIncomplete))
            return in;
    }

    for (size_t i = 0; i < in.segs.size(); ++i) {
        if (i) in.segmented.push_back('\'');
        in.segmented.append(in.letters, in.segs[i].first, in.segs[i].second - in.segs[i].first);
    }
    if (!in.lastIncomplete) in.canonicalKey = in.segmented;
    in.ok = true;
    return in;
}

}  // namespace ime
