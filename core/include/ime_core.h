/* C API for the ZhiPin engine, used by the macOS Swift host.
 * Query results are UTF-8 JSON:
 *   {"valid":true,"segmented":"zhong'guo",
 *    "candidates":[{"text":"中国","consumed":8,"user":false}, ...]}
 * consumed = number of chars of the raw input covered by the candidate.
 * Strings returned by ime_engine_query must be freed with ime_string_free.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ImeEngine ImeEngine;

ImeEngine* ime_engine_create(const char* dict_path, const char* user_dict_path);
void ime_engine_destroy(ImeEngine* e);

char* ime_engine_query(ImeEngine* e, const char* raw_input, int max_candidates);

/* Record a committed composition (original raw pinyin -> final hanzi). */
void ime_engine_learn(ImeEngine* e, const char* raw_input, const char* text);

/* Permanently delete a learned phrase. */
void ime_engine_forget(ImeEngine* e, const char* raw_input, const char* text);

void ime_string_free(char* s);

#ifdef __cplusplus
}
#endif
