import Cocoa
import InputMethodKit

let connectionName = "ZhiPin_1_Connection"
guard let bundleId = Bundle.main.bundleIdentifier else {
    NSLog("[zhipin] missing bundle identifier")
    exit(1)
}
guard IMKServer(name: connectionName, bundleIdentifier: bundleId) != nil else {
    NSLog("[zhipin] failed to create IMKServer")
    exit(1)
}
// Load the dictionary before the run loop starts so the first keystroke is instant.
_ = Engine.shared
NSApplication.shared.run()
