#include "EngineBridge.h"

#include <shlobj.h>

#include <memory>
#include <mutex>

#include "Globals.h"

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string a(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &a[0], n, nullptr, nullptr);
    return a;
}

namespace {

// The core engine opens files with narrow (char*) paths, i.e. the ANSI code
// page on Windows. Returns an empty string when the path cannot be
// represented losslessly in the ACP.
std::string WideToAcpPath(const std::wstring& w) {
    if (w.empty()) return std::string();
    BOOL usedDefault = FALSE;
    int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), (int)w.size(), nullptr, 0,
                                nullptr, &usedDefault);
    if (n <= 0 || usedDefault) return std::string();
    std::string a(n, '\0');
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), (int)w.size(), &a[0], n, nullptr,
                        &usedDefault);
    if (usedDefault) return std::string();
    return a;
}

std::wstring DictPathW() {
    WCHAR path[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_hInst, path, MAX_PATH)) return std::wstring();
    std::wstring p(path);
    size_t slash = p.find_last_of(L'\\');
    if (slash == std::wstring::npos) return std::wstring();
    return p.substr(0, slash + 1) + L"dict.tsv";
}

// %APPDATA%\ZhiPin\user_dict.tsv, falling back to %ProgramData%\ZhiPin when
// the roaming path is not ACP-representable (e.g. exotic user names).
std::wstring UserDictPathW() {
    auto tryBase = [](REFKNOWNFOLDERID id) -> std::wstring {
        PWSTR base = nullptr;
        std::wstring result;
        if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &base)) && base) {
            std::wstring dir = std::wstring(base) + L"\\ZhiPin";
            CreateDirectoryW(dir.c_str(), nullptr);
            result = dir + L"\\user_dict.tsv";
        }
        if (base) CoTaskMemFree(base);
        return result;
    };
    std::wstring p = tryBase(FOLDERID_RoamingAppData);
    if (!p.empty() && !WideToAcpPath(p).empty()) return p;
    return tryBase(FOLDERID_ProgramData);
}

std::mutex g_engineMutex;
std::unique_ptr<ime::Engine> g_engine;
bool g_engineFailed = false;

}  // namespace

ime::Engine* GetEngine() {
    std::lock_guard<std::mutex> lock(g_engineMutex);
    if (g_engine) return g_engine.get();
    if (g_engineFailed) return nullptr;

    std::string dict = WideToAcpPath(DictPathW());
    std::string user = WideToAcpPath(UserDictPathW());
    if (dict.empty()) {
        g_engineFailed = true;
        return nullptr;
    }
    if (user.empty()) user = dict + ".user";  // last-resort fallback next to the dict
    g_engine = std::make_unique<ime::Engine>(dict, user);
    return g_engine.get();
}
