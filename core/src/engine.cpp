#include "ime/engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "pinyin.hpp"

namespace ime {

namespace {

struct DictRec {
    std::string_view key;   // "zhong'guo" — views into the loaded blob
    std::string_view text;  // "中国"
    float logw = 0;
    int sylCount = 0;
};

struct UserEntry {
    std::string text;
    long count = 0;
    long lastUsed = 0;
};

int countSyllables(std::string_view key) {
    int n = 1;
    for (char c : key)
        if (c == '\'') ++n;
    return n;
}

bool startsWith(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

}  // namespace

struct Engine::Impl {
    std::string blob;            // dictionary file contents; DictRecs point into it
    std::vector<DictRec> recs;   // sorted by (key asc, logw desc)
    std::vector<int> byCount;    // rec indices sorted by (sylCount, key) — completion index
    SylTable syls;
    double logTotal = 1;         // log2 of total corpus weight, the per-word DP cost

    std::string userPath;
    std::unordered_map<std::string, std::vector<UserEntry>> userDict;

    // ---- loading ----

    void loadDict(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "[zhipin] cannot open dictionary: %s\n", path.c_str());
            return;
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        blob = ss.str();

        double total = 0;
        size_t pos = 0, n = blob.size();
        while (pos < n) {
            size_t eol = blob.find('\n', pos);
            if (eol == std::string::npos) eol = n;
            std::string_view line(blob.data() + pos, eol - pos);
            pos = eol + 1;
            size_t t1 = line.find('\t');
            if (t1 == std::string_view::npos) continue;
            size_t t2 = line.find('\t', t1 + 1);
            if (t2 == std::string_view::npos) continue;
            DictRec r;
            r.key = line.substr(0, t1);
            r.text = line.substr(t1 + 1, t2 - t1 - 1);
            double w = std::strtod(std::string(line.substr(t2 + 1)).c_str(), nullptr);
            if (r.key.empty() || r.text.empty()) continue;
            r.logw = static_cast<float>(std::log2(w + 2.0));
            r.sylCount = countSyllables(r.key);
            total += w;
            recs.push_back(r);
        }
        std::sort(recs.begin(), recs.end(), [](const DictRec& a, const DictRec& b) {
            if (a.key != b.key) return a.key < b.key;
            return a.logw > b.logw;
        });
        logTotal = std::log2(total + 2.0);

        // Derive the valid-syllable table from the keys themselves.
        for (const auto& r : recs) {
            size_t start = 0;
            for (size_t i = 0; i <= r.key.size(); ++i) {
                if (i == r.key.size() || r.key[i] == '\'') {
                    syls.add(std::string(r.key.substr(start, i - start)));
                    start = i + 1;
                }
            }
        }

        byCount.resize(recs.size());
        for (size_t i = 0; i < recs.size(); ++i) byCount[i] = static_cast<int>(i);
        std::sort(byCount.begin(), byCount.end(), [this](int a, int b) {
            if (recs[a].sylCount != recs[b].sylCount) return recs[a].sylCount < recs[b].sylCount;
            if (recs[a].key != recs[b].key) return recs[a].key < recs[b].key;
            return recs[a].logw > recs[b].logw;
        });
    }

    // ---- dictionary lookups (recs sorted by key) ----

    std::pair<size_t, size_t> exactRange(std::string_view key) const {
        auto lo = std::lower_bound(recs.begin(), recs.end(), key,
                                   [](const DictRec& r, std::string_view k) { return r.key < k; });
        auto hi = lo;
        while (hi != recs.end() && hi->key == key) ++hi;
        return {static_cast<size_t>(lo - recs.begin()), static_cast<size_t>(hi - recs.begin())};
    }

    bool hasExactKey(std::string_view key) const {
        auto [lo, hi] = exactRange(key);
        return lo < hi;
    }

    // Is `p` a strict prefix of any key (used to prune lattice paths)?
    bool hasKeyWithPrefix(const std::string& p) const {
        auto lo = std::lower_bound(recs.begin(), recs.end(), std::string_view(p),
                                   [](const DictRec& r, std::string_view k) { return r.key < k; });
        return lo != recs.end() && startsWith(lo->key, p);
    }

    // Completion of the last, partially-typed syllable: keys starting with
    // `prefix` that have exactly `sylCount` syllables — same character count
    // as typed, never more.
    void completions(const std::string& prefix, int sylCount,
                     std::vector<const DictRec*>& out) const {
        auto cmp = [this](int idx, const std::pair<int, std::string_view>& v) {
            if (recs[idx].sylCount != v.first) return recs[idx].sylCount < v.first;
            return recs[idx].key < v.second;
        };
        auto it = std::lower_bound(byCount.begin(), byCount.end(),
                                   std::make_pair(sylCount, std::string_view(prefix)), cmp);
        for (; it != byCount.end(); ++it) {
            const DictRec& r = recs[*it];
            if (r.sylCount != sylCount || !startsWith(r.key, prefix)) break;
            out.push_back(&r);
        }
    }

    // ---- user dictionary ----

    void loadUserDict() {
        std::ifstream f(userPath, std::ios::binary);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ls(line);
            std::string key, text, countS, lastS;
            if (!std::getline(ls, key, '\t') || !std::getline(ls, text, '\t') ||
                !std::getline(ls, countS, '\t') || !std::getline(ls, lastS, '\t'))
                continue;
            UserEntry e;
            e.text = text;
            e.count = std::strtol(countS.c_str(), nullptr, 10);
            e.lastUsed = std::strtol(lastS.c_str(), nullptr, 10);
            userDict[key].push_back(std::move(e));
        }
    }

    void saveUserDict() const {
        std::string tmp = userPath + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) {
                std::fprintf(stderr, "[zhipin] cannot write user dict: %s\n", tmp.c_str());
                return;
            }
            for (const auto& [key, entries] : userDict)
                for (const auto& e : entries)
                    f << key << '\t' << e.text << '\t' << e.count << '\t' << e.lastUsed << '\n';
        }
        std::rename(tmp.c_str(), userPath.c_str());
    }

    // ---- lattice ----

    // All complete-syllable dictionary/user words starting at letter `pos`.
    struct SpanMatch {
        int end = 0;  // letter index past the match
        const DictRec* rec = nullptr;
        const UserEntry* userEntry = nullptr;  // set for user-dict matches
        std::string key;
    };

    void wordsFrom(const ParsedInput& in, int pos, std::vector<SpanMatch>& out) const {
        std::string key;
        dfsWords(in, pos, pos, key, 0, out);
    }

    void dfsWords(const ParsedInput& in, int start, int pos, std::string& key, int depth,
                  std::vector<SpanMatch>& out) const {
        int n = static_cast<int>(in.letters.size());
        if (depth >= Engine::kMaxSyllables) return;
        int maxEnd = std::min(n, pos + syls.maxLen);
        for (int end = pos + 1; end <= maxEnd; ++end) {
            if (!in.spanAllowed(pos, end)) continue;
            std::string syl = in.letters.substr(pos, end - pos);
            if (!syls.isFull(syl)) continue;
            size_t keyLen = key.size();
            if (!key.empty()) key.push_back('\'');
            key += syl;

            auto [lo, hi] = exactRange(key);
            for (size_t i = lo; i < hi; ++i)
                out.push_back({end, &recs[i], nullptr, key});
            auto uit = userDict.find(key);
            if (uit != userDict.end())
                for (const auto& e : uit->second)
                    out.push_back({end, nullptr, &e, key});

            // Recurse only if some dict key can extend this path.
            if (end < n && (lo < hi || hasKeyWithPrefix(key + "'")))
                dfsWords(in, start, end, key, depth + 1, out);
            else if (end < n && userHasPrefix(key + "'"))
                dfsWords(in, start, end, key, depth + 1, out);

            key.resize(keyLen);
        }
    }

    bool userHasPrefix(const std::string& p) const {
        for (const auto& [k, _] : userDict)
            if (startsWith(k, p)) return true;
        return false;
    }

    // Best remaining-path log-probability from each letter position to the end
    // of the input. suffix[p] answers: "if a candidate consumes letters [0,p),
    // how probable is the best reading of what's left?" Candidates are ranked
    // by log P(candidate) + suffix[end], so a short word only outranks a long
    // one when the probabilities actually say so.
    std::vector<double> suffixScores(const ParsedInput& in) const {
        constexpr double kNeg = -1e18;
        int n = static_cast<int>(in.letters.size());
        std::vector<double> suf(n + 1, kNeg);
        if (!in.lastIncomplete) suf[n] = 0;
        std::vector<SpanMatch> matches;
        for (int p = n - 1; p >= 0; --p) {
            // The remainder may be one partially-typed syllable: its edge
            // weight is the best same-length completion (never more chars).
            if (in.lastIncomplete && n - p <= syls.maxLen && in.spanAllowed(p, n)) {
                std::string r = in.letters.substr(p);
                if (syls.isPrefix(r)) {
                    std::vector<const DictRec*> comps;
                    completions(r, 1, comps);
                    double best = kNeg;
                    for (const DictRec* c : comps)
                        best = std::max(best, static_cast<double>(c->logw));
                    if (best > kNeg / 2) suf[p] = std::max(suf[p], best - logTotal);
                }
            }
            matches.clear();
            wordsFrom(in, p, matches);
            for (const auto& m : matches) {
                if (!m.rec || suf[m.end] <= kNeg / 2) continue;
                suf[p] = std::max(suf[p], m.rec->logw - logTotal + suf[m.end]);
            }
        }
        return suf;
    }

    // Best full conversion via unigram DP over the word lattice.
    // Returns empty string unless the best path uses >= 2 words.
    std::string bestSentence(const ParsedInput& in) const {
        int n = static_cast<int>(in.letters.size());
        constexpr double kNeg = -1e18;
        std::vector<double> best(n + 1, kNeg);
        std::vector<int> prevPos(n + 1, -1);
        std::vector<std::string_view> prevWord(n + 1);
        best[0] = 0;
        std::vector<SpanMatch> matches;
        for (int pos = 0; pos < n; ++pos) {
            if (best[pos] <= kNeg / 2) continue;
            matches.clear();
            wordsFrom(in, pos, matches);
            // Best dictionary entry per (end,key) is the first, since entries
            // within a key are weight-sorted; just relax them all.
            for (const auto& m : matches) {
                if (!m.rec) continue;  // user phrases surface separately, keep DP predictable
                double score = best[pos] + m.rec->logw - logTotal;
                if (score > best[m.end]) {
                    best[m.end] = score;
                    prevPos[m.end] = pos;
                    prevWord[m.end] = m.rec->text;
                }
            }
        }
        if (best[n] <= kNeg / 2) return "";
        std::vector<std::string_view> words;
        for (int p = n; p > 0; p = prevPos[p]) words.push_back(prevWord[p]);
        if (words.size() < 2) return "";
        std::string s;
        for (auto it = words.rbegin(); it != words.rend(); ++it) s += std::string(*it);
        return s;
    }
};

Engine::Engine(const std::string& dictPath, const std::string& userDictPath) : impl_(new Impl) {
    impl_->userPath = userDictPath;
    impl_->loadDict(dictPath);
    impl_->loadUserDict();
}

Engine::~Engine() { delete impl_; }

QueryResult Engine::query(const std::string& rawInput, int maxCandidates) const {
    QueryResult res;
    res.segmented = rawInput;
    if (rawInput.empty() || static_cast<int>(rawInput.size()) > kMaxInputLen) return res;

    ParsedInput in = parseInput(rawInput, impl_->syls, kMaxSyllables);
    if (!in.ok) return res;
    res.valid = true;
    res.segmented = in.segmented;

    int n = static_cast<int>(in.letters.size());
    std::vector<Candidate> out;
    auto seen = [&out](const std::string& text, int consumed) {
        for (const auto& c : out)
            if (c.consumed == consumed && c.text == text) return true;
        return false;
    };
    auto push = [&](const std::string& text, int consumed, bool user) {
        if (!seen(text, consumed)) out.push_back({text, consumed, user});
    };

    // 1. Learned phrases covering the entire input (exact key, alternative
    //    segmentation, or completion of the final syllable) get adaptive
    //    placement: first use ranks just below the best natural candidate;
    //    from the second use on they take the top spot. Collected here,
    //    placed during assembly below.
    struct UserHit {
        std::string text;
        long count = 0;
        long lastUsed = 0;
    };
    std::vector<UserHit> fullUser;
    auto addFullUser = [&fullUser](const UserEntry& e) {
        for (auto& h : fullUser) {
            if (h.text == e.text) {
                h.count = std::max(h.count, e.count);
                h.lastUsed = std::max(h.lastUsed, e.lastUsed);
                return;
            }
        }
        fullUser.push_back({e.text, e.count, e.lastUsed});
    };
    if (!in.canonicalKey.empty()) {
        auto uit = impl_->userDict.find(in.canonicalKey);
        if (uit != impl_->userDict.end())
            for (const auto& e : uit->second) addFullUser(e);
    }

    // 2. Best whole-input conversion (only when it needs >= 2 words).
    std::string sentence;
    if (!in.canonicalKey.empty()) sentence = impl_->bestSentence(in);

    // 3. Words starting at the beginning of the input, plus completions of the
    //    last partially-typed syllable. Each candidate is ranked by the
    //    probability of the best full reading that starts with it:
    //    log P(candidate) + suffix[end].
    struct Ranked {
        std::string text;
        int consumed;
        bool user;
        double score;
    };
    std::vector<Ranked> ranked;

    std::vector<double> suf = impl_->suffixScores(in);
    // Learned entries rank like very common words, growing with use count.
    constexpr double kUserBaseLogW = 20.0;
    auto userLogW = [&](long count) {
        return kUserBaseLogW + std::log2(static_cast<double>(count) + 1.0);
    };
    // When the rest of the input has no valid reading, fall back to preferring
    // longer coverage; keep these below every probability-scored candidate.
    auto fallbackScore = [](int consumed) { return -1e17 + consumed; };

    std::vector<Impl::SpanMatch> matches;
    impl_->wordsFrom(in, 0, matches);
    for (const auto& m : matches) {
        int consumed = in.consumedAt(m.end);
        if (m.userEntry && consumed == in.rawLen) {
            addFullUser(*m.userEntry);  // whole-input match: adaptive placement
            continue;
        }
        double w = m.userEntry ? userLogW(m.userEntry->count)
                               : static_cast<double>(m.rec->logw);
        double score = suf[m.end] > -5e17 ? w - impl_->logTotal + suf[m.end]
                                          : fallbackScore(consumed);
        if (m.userEntry)
            ranked.push_back({m.userEntry->text, consumed, true, score});
        else
            ranked.push_back({std::string(m.rec->text), consumed, false, score});
    }

    if (in.lastIncomplete && !in.segs.empty()) {
        auto [tailStart, tailEnd] = in.segs.back();
        (void)tailEnd;
        std::string prefix;
        for (size_t i = 0; i + 1 < in.segs.size(); ++i) {
            if (i) prefix.push_back('\'');
            prefix.append(in.letters, in.segs[i].first, in.segs[i].second - in.segs[i].first);
        }
        if (!prefix.empty()) prefix.push_back('\'');
        prefix.append(in.letters, tailStart, n - tailStart);

        std::vector<const DictRec*> comps;
        impl_->completions(prefix, static_cast<int>(in.segs.size()), comps);
        for (const DictRec* r : comps)
            ranked.push_back({std::string(r->text), in.rawLen, false,
                              static_cast<double>(r->logw) - impl_->logTotal});
        // Learned phrases can complete too, with the same syllable-count rule.
        // They cover the whole input, so they get adaptive placement.
        for (const auto& [k, entries] : impl_->userDict) {
            if (!startsWith(k, prefix) || countSyllables(k) != static_cast<int>(in.segs.size()))
                continue;
            for (const auto& e : entries) addFullUser(e);
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.consumed > b.consumed;
    });
    std::sort(fullUser.begin(), fullUser.end(), [](const UserHit& a, const UserHit& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.lastUsed > b.lastUsed;
    });

    // Assembly: repeat learned phrases (count >= 2) on top, then the best
    // natural candidate, then first-time learned phrases, then the rest.
    std::vector<Candidate> natural;
    if (!sentence.empty()) natural.push_back({sentence, in.rawLen, false});
    for (const auto& r : ranked) natural.push_back({r.text, r.consumed, r.user});

    for (const auto& h : fullUser)
        if (h.count >= 2) push(h.text, in.rawLen, true);
    size_t ni = 0;
    if (ni < natural.size()) {
        push(natural[ni].text, natural[ni].consumed, natural[ni].user);
        ++ni;
    }
    for (const auto& h : fullUser)
        if (h.count == 1) push(h.text, in.rawLen, true);
    for (; ni < natural.size(); ++ni) {
        if (static_cast<int>(out.size()) >= maxCandidates) break;
        push(natural[ni].text, natural[ni].consumed, natural[ni].user);
    }
    if (static_cast<int>(out.size()) > maxCandidates) out.resize(maxCandidates);

    res.candidates = std::move(out);
    return res;
}

void Engine::learn(const std::string& rawInput, const std::string& text) {
    if (text.empty()) return;
    ParsedInput in = parseInput(rawInput, impl_->syls, kMaxSyllables);
    if (!in.ok || in.canonicalKey.empty()) return;
    auto& entries = impl_->userDict[in.canonicalKey];
    for (auto& e : entries) {
        if (e.text == text) {
            ++e.count;
            e.lastUsed = static_cast<long>(std::time(nullptr));
            impl_->saveUserDict();
            return;
        }
    }
    entries.push_back({text, 1, static_cast<long>(std::time(nullptr))});
    impl_->saveUserDict();
}

void Engine::forget(const std::string& rawInput, const std::string& text) {
    ParsedInput in = parseInput(rawInput, impl_->syls, kMaxSyllables);
    if (!in.ok) return;
    // Match on the letter sequence, not the canonical key: a phrase saved as
    // "fan'gan" must be deletable while the user is typing "fangan".
    auto lettersOf = [](const std::string& key) {
        std::string s;
        for (char c : key)
            if (c != '\'') s.push_back(c);
        return s;
    };
    bool changed = false;
    for (auto it = impl_->userDict.begin(); it != impl_->userDict.end();) {
        if (lettersOf(it->first) == in.letters) {
            auto& entries = it->second;
            size_t before = entries.size();
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                                         [&](const UserEntry& e) { return e.text == text; }),
                          entries.end());
            changed |= entries.size() != before;
            if (entries.empty()) {
                it = impl_->userDict.erase(it);
                continue;
            }
        }
        ++it;
    }
    if (changed) impl_->saveUserDict();
}

}  // namespace ime
