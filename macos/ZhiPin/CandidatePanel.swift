import Cocoa

/// Custom candidate window. IMKCandidates cannot mark user phrases or handle a
/// delete shortcut, so we draw our own.
final class CandidatePanel {
    static let shared = CandidatePanel()
    static let pageSize = 9

    private let panel: NSPanel
    private let view: CandidateView

    private init() {
        view = CandidateView()
        panel = NSPanel(contentRect: .zero,
                        styleMask: [.borderless, .nonactivatingPanel],
                        backing: .buffered, defer: true)
        panel.level = NSWindow.Level(rawValue: Int(CGWindowLevelForKey(.popUpMenuWindow)))
        panel.isOpaque = false
        panel.backgroundColor = .clear
        panel.hasShadow = true
        panel.hidesOnDeactivate = false
        panel.isFloatingPanel = true
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        panel.contentView = view
    }

    /// items: the current page. highlight: index within the page.
    func show(items: [Engine.Candidate], highlight: Int, pageNumber: Int, hasMorePages: Bool,
              caretRect: NSRect) {
        view.update(items: items, highlight: highlight, pageNumber: pageNumber,
                    hasMorePages: hasMorePages)
        let size = view.desiredSize()
        var origin = NSPoint(x: caretRect.origin.x, y: caretRect.origin.y - size.height - 6)
        if let screen = screenContaining(caretRect.origin) {
            let f = screen.visibleFrame
            if origin.x + size.width > f.maxX { origin.x = f.maxX - size.width }
            if origin.x < f.minX { origin.x = f.minX }
            if origin.y < f.minY { origin.y = caretRect.maxY + 6 }
        }
        panel.setFrame(NSRect(origin: origin, size: size), display: true)
        panel.orderFrontRegardless()
    }

    func hide() {
        panel.orderOut(nil)
    }

    private func screenContaining(_ p: NSPoint) -> NSScreen? {
        NSScreen.screens.first { $0.frame.contains(p) } ?? NSScreen.main
    }
}

private final class CandidateView: NSView {
    private var items: [Engine.Candidate] = []
    private var highlight = 0
    private var pageNumber = 0
    private var hasMorePages = false

    private let candidateFont = NSFont.systemFont(ofSize: 16)
    private let labelFont = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .regular)
    private let hintFont = NSFont.systemFont(ofSize: 10)
    private let rowHeight: CGFloat = 24
    private let padding: CGFloat = 8
    private let hintHeight: CGFloat = 16

    func update(items: [Engine.Candidate], highlight: Int, pageNumber: Int, hasMorePages: Bool) {
        self.items = items
        self.highlight = highlight
        self.pageNumber = pageNumber
        self.hasMorePages = hasMorePages
        needsDisplay = true
    }

    private func rowText(_ i: Int) -> NSAttributedString {
        let c = items[i]
        let s = NSMutableAttributedString()
        s.append(NSAttributedString(string: "\(i + 1) ", attributes: [
            .font: labelFont, .foregroundColor: NSColor.secondaryLabelColor,
        ]))
        s.append(NSAttributedString(string: c.text, attributes: [
            .font: candidateFont, .foregroundColor: NSColor.labelColor,
        ]))
        if c.user {
            s.append(NSAttributedString(string: " ★", attributes: [
                .font: hintFont, .foregroundColor: NSColor.systemOrange,
            ]))
        }
        return s
    }

    func desiredSize() -> NSSize {
        var width: CGFloat = 120
        for i in items.indices {
            width = max(width, rowText(i).size().width + padding * 2 + 8)
        }
        width = max(width, footerText().size().width + padding * 2)
        let height = CGFloat(items.count) * rowHeight + hintHeight + padding * 2
        return NSSize(width: min(width, 420), height: height)
    }

    private func footerText() -> NSAttributedString {
        var hint = "⇧ 中/英"
        if hasMorePages || pageNumber > 0 { hint += "   -/= 翻页(\(pageNumber + 1))" }
        if items.indices.contains(highlight), items[highlight].user { hint += "   ⌃⌫ 删除自造词" }
        return NSAttributedString(string: hint, attributes: [
            .font: hintFont, .foregroundColor: NSColor.tertiaryLabelColor,
        ])
    }

    override func draw(_ dirtyRect: NSRect) {
        let bg = NSBezierPath(roundedRect: bounds, xRadius: 8, yRadius: 8)
        NSColor.windowBackgroundColor.setFill()
        bg.fill()
        NSColor.separatorColor.setStroke()
        bg.lineWidth = 0.5
        bg.stroke()

        var y = bounds.height - padding - rowHeight
        for i in items.indices {
            let rowRect = NSRect(x: padding / 2, y: y, width: bounds.width - padding,
                                 height: rowHeight)
            if i == highlight {
                let sel = NSBezierPath(roundedRect: rowRect, xRadius: 5, yRadius: 5)
                NSColor.selectedContentBackgroundColor.withAlphaComponent(0.25).setFill()
                sel.fill()
            }
            let text = rowText(i)
            let ts = text.size()
            text.draw(at: NSPoint(x: padding, y: y + (rowHeight - ts.height) / 2))
            y -= rowHeight
        }
        footerText().draw(at: NSPoint(x: padding, y: padding / 2))
    }
}

/// Small transient HUD shown when the user toggles Chinese/English with Shift.
final class ModeHUD {
    static let shared = ModeHUD()
    private let panel: NSPanel
    private let label: NSTextField
    private var hideTimer: Timer?

    private init() {
        label = NSTextField(labelWithString: "中")
        label.font = .systemFont(ofSize: 20, weight: .medium)
        label.alignment = .center
        label.frame = NSRect(x: 0, y: 0, width: 48, height: 34)
        panel = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 48, height: 34),
                        styleMask: [.borderless, .nonactivatingPanel],
                        backing: .buffered, defer: true)
        panel.level = NSWindow.Level(rawValue: Int(CGWindowLevelForKey(.popUpMenuWindow)))
        panel.isOpaque = false
        panel.backgroundColor = NSColor.windowBackgroundColor.withAlphaComponent(0.95)
        panel.hasShadow = true
        panel.isFloatingPanel = true
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        panel.contentView = label
    }

    func flash(_ text: String, near p: NSPoint) {
        label.stringValue = text
        panel.setFrameOrigin(NSPoint(x: p.x, y: p.y + 8))
        panel.orderFrontRegardless()
        hideTimer?.invalidate()
        hideTimer = Timer.scheduledTimer(withTimeInterval: 0.6, repeats: false) { [panel] _ in
            panel.orderOut(nil)
        }
    }
}
