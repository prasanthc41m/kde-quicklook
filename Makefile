PREFIX  ?= /usr
BINDIR   = $(PREFIX)/bin
MOC      = /usr/lib64/qt6/libexec/moc
SRC      = src/quicklook.cpp
MOC_OUT  = src/quicklook.moc
BIN      = /tmp/kde-quicklook-build

HOME_DIR := $(shell eval echo ~$$SUDO_USER 2>/dev/null || echo $$HOME)
LOCAL_BIN = $(HOME_DIR)/.local/bin
LOCAL_SVC = $(HOME_DIR)/.local/share/kio/servicemenus
LOCAL_APP = $(HOME_DIR)/.local/share/applications

QT_INC  = /usr/include/qt6
KF6_INC = /usr/include/KF6

INC := -I$(QT_INC) \
       -I$(QT_INC)/QtCore \
       -I$(QT_INC)/QtWidgets \
       -I$(QT_INC)/QtGui \
       -I$(QT_INC)/QtMultimedia \
       -I$(QT_INC)/QtMultimediaWidgets \
       -I$(KF6_INC)/KIOCore \
       -I$(KF6_INC)/KI18n \
       -I/usr/include/poppler/qt6 \
       -I/usr/include/poppler \
       -I/usr/include/taglib \
       -Isrc

CFLAGS := $(INC) -fPIC \
          $(shell pkg-config --cflags Qt6Core Qt6Widgets Qt6Multimedia Qt6MultimediaWidgets poppler-qt6 taglib)

LIBS := $(shell pkg-config --libs Qt6Core Qt6Widgets Qt6Multimedia Qt6MultimediaWidgets poppler-qt6 taglib) \
        -lKF6KIOCore -lKF6I18n

.PHONY: all build install uninstall clean help

all: build

help:
	@echo ""
	@echo "KDE QuickLook - macOS-style Quick Look for KDE Plasma 6"
	@echo ""
	@echo "  make build      - Compile the C++ preview binary"
	@echo "  make install    - Build + install everything"
	@echo "  make uninstall  - Remove all installed files"
	@echo "  make clean      - Remove build artifacts"
	@echo ""

build:
	@echo ">>> Generating MOC..."
	@$(MOC) $(INC) $(SRC) -o $(MOC_OUT)
	@echo ">>> Compiling kde-quicklook..."
	@g++ -o $(BIN) $(CFLAGS) $(SRC) $(LIBS)
	@echo ">>> Build complete."

install: build
	@echo ">>> Installing kde-quicklook binary to $(BINDIR)..."
	@sudo cp $(BIN) $(BINDIR)/kde-quicklook
	@sudo chmod +x $(BINDIR)/kde-quicklook
	@echo ">>> Installing launcher script to $(LOCAL_BIN)..."
	@mkdir -p $(LOCAL_BIN)
	@install -m 755 scripts/dolphin-quicklook.sh $(LOCAL_BIN)/dolphin-quicklook.sh
	@echo ">>> Installing service menu to $(LOCAL_SVC)..."
	@mkdir -p $(LOCAL_SVC)
	@install -m 755 desktop/dolphin-quicklook.desktop $(LOCAL_SVC)/dolphin-quicklook.desktop
	@echo ">>> Installing application entry to $(LOCAL_APP)..."
	@mkdir -p $(LOCAL_APP)
	@install -m 644 desktop/quicklook-app.desktop $(LOCAL_APP)/quicklook-app.desktop
	@echo ">>> Refreshing KDE service cache..."
	@kbuildsycoca6 2>/dev/null || true
	@echo ""
	@echo "✅ Installation complete!"
	@echo ""
	@echo "Next step: Bind your shortcut key:"
	@echo "  Run: systemsettings kcm_keys"
	@echo "  Add a new Command shortcut pointing to: $(LOCAL_BIN)/dolphin-quicklook.sh"
	@echo "  Assign: Shift+Space (or any key you prefer)"
	@echo ""

uninstall:
	@echo ">>> Removing installed files..."
	@sudo rm -f $(BINDIR)/kde-quicklook
	@rm -f $(LOCAL_BIN)/dolphin-quicklook.sh
	@rm -f $(LOCAL_SVC)/dolphin-quicklook.desktop
	@rm -f $(LOCAL_APP)/quicklook-app.desktop
	@kbuildsycoca6 2>/dev/null || true
	@echo "✅ Uninstall complete."

clean:
	@echo ">>> Cleaning build artifacts..."
	@rm -f $(MOC_OUT) $(BIN)
	@echo ">>> Done."
