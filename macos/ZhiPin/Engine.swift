import Foundation

/// Swift wrapper over the C core. IMK delivers all events on the main thread,
/// so no extra locking is needed beyond keeping usage on that thread.
final class Engine {
    static let shared = Engine()

    struct Candidate: Decodable {
        let text: String
        let consumed: Int
        let user: Bool
    }
    struct QueryResult: Decodable {
        let valid: Bool
        let segmented: String
        let candidates: [Candidate]

        static let empty = QueryResult(valid: false, segmented: "", candidates: [])
    }

    private var handle: OpaquePointer?

    private init() {
        guard let dictPath = Bundle.main.path(forResource: "dict", ofType: "tsv") else {
            NSLog("[zhipin] dict.tsv missing from bundle")
            return
        }
        let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("ZhiPin", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        let userPath = dir.appendingPathComponent("user_dict.tsv").path
        handle = ime_engine_create(dictPath, userPath)
    }

    func query(_ raw: String, maxCandidates: Int32 = 100) -> QueryResult {
        guard let h = handle, let cstr = ime_engine_query(h, raw, maxCandidates) else {
            return .empty
        }
        defer { ime_string_free(cstr) }
        let data = Data(bytes: cstr, count: strlen(cstr))
        return (try? JSONDecoder().decode(QueryResult.self, from: data)) ?? .empty
    }

    func learn(raw: String, text: String) {
        guard let h = handle else { return }
        ime_engine_learn(h, raw, text)
    }

    func forget(raw: String, text: String) {
        guard let h = handle else { return }
        ime_engine_forget(h, raw, text)
    }
}
