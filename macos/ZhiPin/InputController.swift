import Cocoa
import InputMethodKit

@objc(ZhiPinInputController)
class ZhiPinInputController: IMKInputController {
    // Mode is shared across all clients and remembered between launches.
    private static var chineseMode = UserDefaults.standard.object(forKey: "chineseMode") == nil
        ? true : UserDefaults.standard.bool(forKey: "chineseMode")
    private static var pairedDoubleQuoteOpen = true
    private static var pairedSingleQuoteOpen = true

    // Composition state. `originalRaw` is every pinyin char typed for this
    // composition; `chain` holds candidate picks made so far; the remaining
    // (unconverted) buffer is always a suffix of originalRaw.
    private var originalRaw = ""
    private var chain: [(text: String, consumed: Int)] = []
    private var result = Engine.QueryResult.empty
    private var page = 0
    private var highlight = 0
    private var shiftPending = false
    // Retained for panel mouse events, which arrive outside handle(_:client:).
    private weak var lastClient: AnyObject?

    private var consumedTotal: Int { chain.reduce(0) { $0 + $1.consumed } }
    private var remaining: String { String(originalRaw.dropFirst(consumedTotal)) }
    private var pendingText: String { chain.map(\.text).joined() }
    private var composing: Bool { !originalRaw.isEmpty }

    override func recognizedEvents(_ sender: Any!) -> Int {
        Int(NSEvent.EventTypeMask([.keyDown, .flagsChanged]).rawValue)
    }

    // MARK: - Event handling

    override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
        guard let event = event, let client = sender as? IMKTextInput else { return false }
        lastClient = sender as AnyObject

        if event.type == .flagsChanged {
            return handleFlagsChanged(event, client: client)
        }
        guard event.type == .keyDown else { return false }
        shiftPending = false

        if !Self.chineseMode { return false }

        let mods = event.modifierFlags.intersection([.command, .control, .option, .function])
        let keyCode = event.keyCode
        let chars = event.charactersIgnoringModifiers ?? ""

        // ⌃⌫ permanently deletes the highlighted learned phrase.
        if composing && keyCode == 51 && mods == .control {
            forgetHighlighted(client: client)
            return true
        }
        if mods.contains(.command) || mods.contains(.control) || mods.contains(.option) {
            if composing { commitAsTyped(client: client) }
            return false
        }

        if !composing {
            if let ch = chars.first, ch.isLowercase, ch.isLetter, ch.isASCII,
               !event.modifierFlags.contains(.shift) {
                originalRaw = String(ch)
                refresh(client: client)
                return true
            }
            if let mapped = mapPunctuation(chars) {
                client.insert(mapped)
                return true
            }
            return false
        }

        // --- composing ---
        switch keyCode {
        case 51:  // backspace
            if remaining.isEmpty, !chain.isEmpty {
                // Undo the last pick; its letters return to the buffer.
                chain.removeLast()
                refresh(client: client)
            } else {
                originalRaw.removeLast()
                if originalRaw.isEmpty {
                    reset(client: client)
                } else {
                    refresh(client: client)
                }
            }
            return true
        case 53:  // escape
            reset(client: client)
            return true
        case 36, 76:  // return
            commitAsTyped(client: client)
            return true
        case 49:  // space
            selectCandidate(page * CandidatePanel.pageSize + highlight, client: client)
            return true
        case 116:  // page up
            changePage(-1, client: client)
            return true
        case 121:  // page down
            changePage(1, client: client)
            return true
        case 126:  // up
            moveHighlight(-1, client: client)
            return true
        case 125:  // down
            moveHighlight(1, client: client)
            return true
        case 123, 124:  // left / right — reserved, swallow to protect the caret
            return true
        default:
            break
        }

        guard let ch = chars.first else { return true }
        if let d = ch.wholeNumberValue, (1...9).contains(d), !result.candidates.isEmpty {
            selectCandidate(page * CandidatePanel.pageSize + (d - 1), client: client)
            return true
        }
        if ch == "-" {
            changePage(-1, client: client)
            return true
        }
        if ch == "=" {
            changePage(1, client: client)
            return true
        }
        if (ch.isLowercase && ch.isLetter && ch.isASCII) || ch == "'" {
            if originalRaw.count < 71 {
                originalRaw.append(ch)
                refresh(client: client)
            } else {
                NSSound.beep()
            }
            return true
        }
        if let mapped = mapPunctuation(String(ch)) {
            // Commit the highlighted candidate (and any tail as typed), then punctuate.
            commitHighlightedThenRest(client: client)
            client.insert(mapped)
            return true
        }
        // Anything else (tab, etc.): commit what we have as typed, pass the key on.
        commitAsTyped(client: client)
        return false
    }

    private func handleFlagsChanged(_ event: NSEvent, client: IMKTextInput) -> Bool {
        let isShiftKey = event.keyCode == 56 || event.keyCode == 60
        let shiftDown = event.modifierFlags.contains(.shift)
        let otherMods = !event.modifierFlags
            .intersection([.command, .control, .option])
            .isEmpty
        if isShiftKey && shiftDown && !otherMods {
            shiftPending = true
            return false
        }
        if isShiftKey && !shiftDown && shiftPending {
            shiftPending = false
            toggleMode(client: client)
            return true
        }
        if !isShiftKey || otherMods { shiftPending = false }
        return false
    }

    private func toggleMode(client: IMKTextInput) {
        if composing { commitAsTyped(client: client) }
        Self.chineseMode.toggle()
        UserDefaults.standard.set(Self.chineseMode, forKey: "chineseMode")
        ModeHUD.shared.flash(Self.chineseMode ? "中" : "英", near: caretRect(client).origin)
    }

    // MARK: - Composition flow

    private func refresh(client: IMKTextInput) {
        result = Engine.shared.query(remaining)
        page = 0
        highlight = 0
        updateUI(client: client)
    }

    private func updateUI(client: IMKTextInput) {
        let shown = pendingText + (result.valid ? result.segmented : remaining)
        setMarked(shown, client: client)
        showPanel(client: client)
    }

    private func showPanel(client: IMKTextInput) {
        guard composing, !result.candidates.isEmpty else {
            CandidatePanel.shared.hide()
            return
        }
        let start = page * CandidatePanel.pageSize
        let items = Array(result.candidates.dropFirst(start).prefix(CandidatePanel.pageSize))
        CandidatePanel.shared.onDelete = { [weak self] pageIndex in
            guard let self, let client = self.lastClient as? IMKTextInput else { return }
            self.forgetCandidate(at: self.page * CandidatePanel.pageSize + pageIndex,
                                 client: client)
        }
        CandidatePanel.shared.show(
            items: items, highlight: highlight, pageNumber: page,
            hasMorePages: result.candidates.count > start + items.count,
            caretRect: caretRect(client))
    }

    private func selectCandidate(_ index: Int, client: IMKTextInput) {
        guard result.candidates.indices.contains(index) else {
            // No candidates (invalid pinyin): space commits what was typed.
            if result.candidates.isEmpty { commitAsTyped(client: client) }
            return
        }
        let c = result.candidates[index]
        chain.append((c.text, c.consumed))
        if remaining.isEmpty {
            let full = pendingText
            client.insert(full)
            Engine.shared.learn(raw: originalRaw, text: full)
            reset(client: client)
        } else {
            refresh(client: client)
        }
    }

    private func forgetHighlighted(client: IMKTextInput) {
        forgetCandidate(at: page * CandidatePanel.pageSize + highlight, client: client)
    }

    private func forgetCandidate(at index: Int, client: IMKTextInput) {
        guard composing, result.candidates.indices.contains(index),
              result.candidates[index].user else { return }
        Engine.shared.forget(raw: remaining, text: result.candidates[index].text)
        refresh(client: client)
    }

    private func changePage(_ delta: Int, client: IMKTextInput) {
        let pages = (result.candidates.count + CandidatePanel.pageSize - 1)
            / CandidatePanel.pageSize
        guard pages > 0 else { return }
        let next = page + delta
        guard (0..<pages).contains(next) else { return }
        page = next
        highlight = 0
        showPanel(client: client)
    }

    private func moveHighlight(_ delta: Int, client: IMKTextInput) {
        let start = page * CandidatePanel.pageSize
        let count = min(CandidatePanel.pageSize, result.candidates.count - start)
        guard count > 0 else { return }
        let next = highlight + delta
        if next < 0 {
            changePage(-1, client: client)
        } else if next >= count {
            changePage(1, client: client)
        } else {
            highlight = next
            showPanel(client: client)
        }
    }

    /// Commit picks made so far plus the un-converted tail as raw letters.
    private func commitAsTyped(client: IMKTextInput) {
        let text = pendingText + remaining
        if !text.isEmpty { client.insert(text) }
        reset(client: client)
    }

    private func commitHighlightedThenRest(client: IMKTextInput) {
        let index = page * CandidatePanel.pageSize + highlight
        if result.candidates.indices.contains(index) {
            let c = result.candidates[index]
            chain.append((c.text, c.consumed))
        }
        let full = pendingText + remaining
        if !full.isEmpty { client.insert(full) }
        if remaining.isEmpty && !pendingText.isEmpty {
            Engine.shared.learn(raw: originalRaw, text: pendingText)
        }
        reset(client: client)
    }

    private func reset(client: IMKTextInput) {
        originalRaw = ""
        chain = []
        result = .empty
        page = 0
        highlight = 0
        setMarked("", client: client)
        CandidatePanel.shared.hide()
    }

    private func setMarked(_ s: String, client: IMKTextInput) {
        let attrs: [NSAttributedString.Key: Any] = [
            .underlineStyle: NSUnderlineStyle.single.rawValue,
        ]
        client.setMarkedText(
            NSAttributedString(string: s, attributes: attrs),
            selectionRange: NSRange(location: s.utf16.count, length: 0),
            replacementRange: NSRange(location: NSNotFound, length: NSNotFound))
    }

    private func caretRect(_ client: IMKTextInput) -> NSRect {
        var rect = NSRect.zero
        client.attributes(forCharacterIndex: 0, lineHeightRectangle: &rect)
        if rect == .zero, let screen = NSScreen.main {
            rect = NSRect(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY,
                          width: 0, height: 0)
        }
        return rect
    }

    // MARK: - Punctuation

    private func mapPunctuation(_ s: String) -> String? {
        switch s {
        case ",": return "，"
        case ".": return "。"
        case "?": return "？"
        case "!": return "！"
        case ";": return "；"
        case ":": return "："
        case "(": return "（"
        case ")": return "）"
        case "[": return "【"
        case "]": return "】"
        case "<": return "《"
        case ">": return "》"
        case "\\": return "、"
        case "\"":
            defer { Self.pairedDoubleQuoteOpen.toggle() }
            return Self.pairedDoubleQuoteOpen ? "“" : "”"
        case "'":
            defer { Self.pairedSingleQuoteOpen.toggle() }
            return Self.pairedSingleQuoteOpen ? "‘" : "’"
        default: return nil
        }
    }

    // MARK: - IMK lifecycle

    override func activateServer(_ sender: Any!) {
        shiftPending = false
    }

    override func deactivateServer(_ sender: Any!) {
        if let client = sender as? IMKTextInput, composing {
            commitAsTyped(client: client)
        }
        CandidatePanel.shared.hide()
    }

    override func commitComposition(_ sender: Any!) {
        if let client = sender as? IMKTextInput, composing {
            commitAsTyped(client: client)
        }
    }

    override func menu() -> NSMenu! {
        let menu = NSMenu()
        let item = NSMenuItem(title: "打开用户词典文件夹…",
                              action: #selector(openUserDictFolder(_:)), keyEquivalent: "")
        item.target = self
        menu.addItem(item)
        return menu
    }

    @objc private func openUserDictFolder(_ sender: Any?) {
        let dir = FileManager.default.urls(for: .applicationSupportDirectory,
                                           in: .userDomainMask)[0]
            .appendingPathComponent("ZhiPin", isDirectory: true)
        NSWorkspace.shared.open(dir)
    }
}

private extension IMKTextInput {
    func insert(_ s: String) {
        insertText(s, replacementRange: NSRange(location: NSNotFound, length: NSNotFound))
    }
}
