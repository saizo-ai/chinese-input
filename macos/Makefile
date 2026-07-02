SWIFTC ?= swiftc
BUILD := build
APP := $(BUILD)/ZhiPin.app
CORE := ../core/build/libime_core.a
DICT := ../data/dict.tsv
VERSION := 0.1.2

SWIFT_SRCS := ZhiPin/main.swift ZhiPin/Engine.swift ZhiPin/InputController.swift \
              ZhiPin/CandidatePanel.swift

.PHONY: all app pkg install uninstall clean core

all: app

core:
	$(MAKE) -C ../core

$(DICT):
	cd ../data && ./fetch.sh && python3 build_dict.py

app: core $(DICT) $(SWIFT_SRCS) ZhiPin/Info.plist
	mkdir -p $(APP)/Contents/MacOS $(APP)/Contents/Resources
	$(SWIFTC) -O -swift-version 5 \
	    -import-objc-header ZhiPin/ZhiPin-Bridging-Header.h \
	    -Xcc -I../core/include \
	    $(SWIFT_SRCS) $(CORE) -lc++ \
	    -framework Cocoa -framework InputMethodKit \
	    -o $(APP)/Contents/MacOS/ZhiPin
	cp ZhiPin/Info.plist $(APP)/Contents/Info.plist
	cp $(DICT) $(APP)/Contents/Resources/dict.tsv
	cp ZhiPin/menuicon.png ZhiPin/menuicon@2x.png $(APP)/Contents/Resources/
	codesign --force --sign - $(APP)

pkg: app
	rm -rf $(BUILD)/pkgroot
	mkdir -p "$(BUILD)/pkgroot/Library/Input Methods"
	cp -R $(APP) "$(BUILD)/pkgroot/Library/Input Methods/"
	pkgbuild --root $(BUILD)/pkgroot \
	    --identifier com.omnific9.inputmethod.ZhiPin \
	    --version $(VERSION) \
	    $(BUILD)/ZhiPin-$(VERSION)-macos.pkg

# Install for the current user (no sudo). Log out/in if the input source
# doesn't appear in System Settings -> Keyboard -> Input Sources.
install: app
	-pkill -f 'Input Methods/ZhiPin.app' 2>/dev/null || true
	rm -rf "$$HOME/Library/Input Methods/ZhiPin.app"
	cp -R $(APP) "$$HOME/Library/Input Methods/"

uninstall:
	-pkill -f 'Input Methods/ZhiPin.app' 2>/dev/null || true
	rm -rf "$$HOME/Library/Input Methods/ZhiPin.app"

clean:
	rm -rf $(BUILD)
