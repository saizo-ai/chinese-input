#include "ime_core.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "ime/engine.hpp"

namespace {

void appendJsonString(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

char* dupString(const std::string& s) {
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (p) std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

}  // namespace

extern "C" {

ImeEngine* ime_engine_create(const char* dict_path, const char* user_dict_path) {
    if (!dict_path || !user_dict_path) return nullptr;
    return reinterpret_cast<ImeEngine*>(new ime::Engine(dict_path, user_dict_path));
}

void ime_engine_destroy(ImeEngine* e) {
    delete reinterpret_cast<ime::Engine*>(e);
}

char* ime_engine_query(ImeEngine* e, const char* raw_input, int max_candidates) {
    if (!e || !raw_input) return nullptr;
    ime::QueryResult r =
        reinterpret_cast<ime::Engine*>(e)->query(raw_input, max_candidates);
    std::string json = "{\"valid\":";
    json += r.valid ? "true" : "false";
    json += ",\"segmented\":";
    appendJsonString(json, r.segmented);
    json += ",\"candidates\":[";
    for (size_t i = 0; i < r.candidates.size(); ++i) {
        const auto& c = r.candidates[i];
        if (i) json.push_back(',');
        json += "{\"text\":";
        appendJsonString(json, c.text);
        json += ",\"consumed\":" + std::to_string(c.consumed);
        json += ",\"user\":";
        json += c.user ? "true" : "false";
        json.push_back('}');
    }
    json += "]}";
    return dupString(json);
}

void ime_engine_learn(ImeEngine* e, const char* raw_input, const char* text) {
    if (!e || !raw_input || !text) return;
    reinterpret_cast<ime::Engine*>(e)->learn(raw_input, text);
}

void ime_engine_forget(ImeEngine* e, const char* raw_input, const char* text) {
    if (!e || !raw_input || !text) return;
    reinterpret_cast<ime::Engine*>(e)->forget(raw_input, text);
}

void ime_string_free(char* s) {
    std::free(s);
}

}  // extern "C"
