// Engine tests, run against the real dict.tsv.
// Usage: test_engine <dict.tsv> <scratch-dir>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "ime/engine.hpp"

static int failures = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, msg); \
            ++failures;                                               \
        }                                                             \
    } while (0)

static int indexOf(const ime::QueryResult& r, const std::string& text) {
    for (size_t i = 0; i < r.candidates.size(); ++i)
        if (r.candidates[i].text == text) return static_cast<int>(i);
    return -1;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: test_engine <dict.tsv> <scratch-dir>\n");
        return 2;
    }
    std::string userPath = std::string(argv[2]) + "/user_dict_test.tsv";
    std::remove(userPath.c_str());
    ime::Engine eng(argv[1], userPath);

    // Exact word lookup, frequency order.
    {
        auto r = eng.query("zhongguo");
        CHECK(r.valid, "zhongguo parses");
        CHECK(r.segmented == "zhong'guo", "zhongguo segments as zhong'guo");
        CHECK(!r.candidates.empty() && r.candidates[0].text == "中国", "zhongguo -> 中国 first");
        CHECK(r.candidates[0].consumed == 8, "consumed covers whole input");
    }
    // Single-character frequency order.
    {
        auto r = eng.query("de");
        CHECK(!r.candidates.empty() && r.candidates[0].text == "的", "de -> 的 first");
    }
    // Sentence conversion via DP over the word lattice.
    {
        auto r = eng.query("woshizhongguoren");
        CHECK(r.valid, "sentence parses");
        CHECK(indexOf(r, "我是中国人") == 0, "sentence DP produces 我是中国人 first");
        CHECK(r.candidates[0].consumed == 16, "sentence covers whole input");
    }
    // Last-syllable completion: same character count as typed, no prediction.
    {
        auto r = eng.query("zhongg");
        CHECK(r.valid, "zhongg parses with incomplete tail");
        int i = indexOf(r, "中国");
        CHECK(i >= 0, "zhongg completes to 中国");
        CHECK(i >= 0 && r.candidates[i].consumed == 6, "completion consumes all typed chars");
        for (const auto& c : r.candidates)
            if (c.consumed == 6) {
                // Every full-span candidate must be exactly 2 hanzi (6 bytes):
                // completion never guesses extra characters.
                CHECK(c.text.size() <= 8, "completion adds no extra characters");
            }
    }
    // Single letter: completion of one character.
    {
        auto r = eng.query("g");
        CHECK(r.valid && !r.candidates.empty(), "single letter offers completions");
        CHECK(r.candidates[0].text.size() <= 4, "single-letter candidates are single chars");
    }
    // Apostrophe forces segmentation.
    {
        auto rx = eng.query("xian");
        CHECK(rx.segmented == "xian", "xian stays one syllable");
        CHECK(indexOf(rx, "先") >= 0, "xian finds 先");
        auto ra = eng.query("xi'an");
        CHECK(ra.segmented == "xi'an", "xi'an stays two syllables");
        CHECK(indexOf(ra, "西安") == 0, "xi'an -> 西安 first");
        CHECK(ra.candidates[0].consumed == 5, "apostrophe counted in consumed");
    }
    // No fuzzy matching: zh is not z, invalid input yields nothing.
    {
        auto r = eng.query("vzz");
        CHECK(!r.valid && r.candidates.empty(), "unparseable input has no candidates");
    }
    // 10-syllable cap.
    {
        std::string s;
        for (int i = 0; i < 11; ++i) s += "ni";
        CHECK(!eng.query(s).valid, "11 syllables rejected");
        s = "";
        for (int i = 0; i < 10; ++i) s += "ni";
        CHECK(eng.query(s).valid, "10 syllables accepted");
    }
    // Adaptive placement: first use ranks just below the best natural
    // candidate, second use takes the top; forget removes permanently.
    // The learned text is deliberately NOT the natural best conversion
    // (that would be 电脑爆炸) so its placement is observable.
    {
        eng.learn("diannaobaozha", "电闹抱榨");
        auto once = eng.query("diannaobaozha");
        int i1 = indexOf(once, "电闹抱榨");
        CHECK(i1 == 1, "first use ranks second, below the natural best");
        CHECK(i1 >= 0 && once.candidates[i1].user, "learned combo flagged as user entry");
        CHECK(indexOf(once, "电脑爆炸") == 0, "natural best stays on top after one use");
        eng.learn("diannaobaozha", "电闹抱榨");
        auto twice = eng.query("diannaobaozha");
        CHECK(indexOf(twice, "电闹抱榨") == 0, "second use takes the top spot");
        eng.forget("diannaobaozha", "电闹抱榨");
        CHECK(indexOf(eng.query("diannaobaozha"), "电闹抱榨") < 0,
              "forgotten combo no longer suggested");
    }
    // Learned phrases persist across engine instances.
    {
        eng.learn("wodeceshi", "沃德测试");
        eng.learn("wodeceshi", "沃德测试");
        ime::Engine eng2(argv[1], userPath);
        auto r = eng2.query("wodeceshi");
        CHECK(indexOf(r, "沃德测试") == 0 && r.candidates[0].user, "user dict persists to disk");
        eng2.forget("wodeceshi", "沃德测试");
        ime::Engine eng3(argv[1], userPath);
        CHECK(indexOf(eng3.query("wodeceshi"), "沃德测试") < 0, "forget persists to disk");
    }
    // Partial-coverage candidates report correct consumed for host chaining.
    {
        auto r = eng.query("zhongguoren");
        bool ok = false;
        for (const auto& c : r.candidates)
            if (c.text == "中国" && c.consumed == 8) ok = true;
        CHECK(ok, "partial candidate 中国 consumes 8 of zhongguoren");
    }
    // Probability ranking: P(中国人) beats P(中国)*P(best rest), and a partial
    // word's rank reflects the probability of the whole reading it starts.
    {
        auto r = eng.query("zhongguoren");
        int whole = indexOf(r, "中国人");
        int part = indexOf(r, "中国");
        CHECK(whole >= 0 && part >= 0 && whole < part,
              "中国人 ranks above 中国 for zhongguoren");
    }

    std::remove(userPath.c_str());
    if (failures == 0) std::printf("all tests passed\n");
    return failures == 0 ? 0 : 1;
}
