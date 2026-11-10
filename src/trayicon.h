#pragma once

#include <QSystemTrayIcon>

class Backend;
class QQuickWindow;

// Native tray icon (StatusNotifierItem on Wayland — quickshell's tray draws
// it next to superwhisper's). The mic recolors with the dictation state,
// the menu mirrors it, clicking toggles the window, and closing the window
// hides to the tray instead of quitting. One icon, small surface: the bar
// panel stays the rich control surface.
class TrayIcon : public QObject {
    Q_OBJECT
public:
    // Mic glyph rendered from the qrc template with the color baked in.
    static QIcon micIcon(const QColor &color);

    TrayIcon(Backend *backend, QQuickWindow *window, QObject *parent = nullptr);

    // Mirror the window's visibility (close-to-tray wiring; stable against
    // the signal echo because the value does not change).
    void windowVisible(bool visible);

private:
    void rebuildMenu();
    void refreshIcon();

    QSystemTrayIcon *m_tray = nullptr;
    class QMenu *m_menu = nullptr;
    class QAction *m_state = nullptr;
    class QAction *m_toggle = nullptr;
    class QAction *m_copy = nullptr;
    class QAction *m_quit = nullptr;
    Backend *m_backend = nullptr;
    QQuickWindow *m_window = nullptr;
};
