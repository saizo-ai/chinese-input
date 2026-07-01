// ZhiPin core engine — exact-match pinyin conversion.
// Dependency-free C++17. Not thread-safe; callers serialize access.
#pragma once

#include <string>
#include <vector>

namespace ime {

struct Candidate {
    std::string text;   // UTF-8 hanzi
    int consumed = 0;   // chars of the raw input (incl. apostrophes) this candidate covers
    bool user = false;  // true if backed by a learned user phrase (deletable)
};

struct QueryResult {
    bool valid = false;      // false when the input cannot be parsed as pinyin
    std::string segmented;   // display form, e.g. "zhong'guo"; equals raw input when invalid
    std::vector<Candidate> candidates;
};

class Engine {
public:
    // dictPath: main dictionary TSV ("key\ttext\tweight", key = syllables joined by ').
    // userDictPath: learned-phrase store; created on first learn() if missing.
    Engine(const std::string& dictPath, const std::string& userDictPath);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // rawInput: lowercase a-z plus apostrophes, at most kMaxInputLen chars.
    QueryResult query(const std::string& rawInput, int maxCandidates = 100) const;

    // Record a committed composition (full original raw pinyin -> final hanzi).
    // No-op if rawInput is not fully parseable. Persists immediately.
    void learn(const std::string& rawInput, const std::string& text);

    // Permanently remove a learned phrase. Persists immediately.
    void forget(const std::string& rawInput, const std::string& text);

    static constexpr int kMaxSyllables = 10;  // "up to 10 characters"
    static constexpr int kMaxInputLen = 72;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace ime
