#include "trayicon.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSvgRenderer>

#include "backend.h"

namespace {
constexpr char kTemplate[] = ":/duckvox/icons/duck.svg";
} // namespace

QIcon TrayIcon::micIcon(const QColor &color) {
    QFile file{QLatin1String(kTemplate)};
    QString svg;
    if (file.open(QIODevice::ReadOnly))
        svg = QString::fromUtf8(file.readAll());
    svg.replace(QLatin1String("@COLOR@"), color.name(QColor::HexRgb));
    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, 64, 64));
    return QIcon(pixmap);
}

TrayIcon::TrayIcon(Backend *backend, QQuickWindow *window, QObject *parent)
    : QObject(parent), m_backend(backend), m_window(window) {
    m_tray = new QSystemTrayIcon(micIcon(QColor(0xEE, 0xEE, 0xEE)), this);
    m_tray->setToolTip(tr("duckvox — idle"));

    m_menu = new QMenu;
    m_state = m_menu->addAction(tr("Idle"));
    m_state->setEnabled(false);
    m_menu->addSeparator();
    m_toggle = m_menu->addAction(tr("Start dictating"), [this] { m_backend->toggle(); });
    m_copy = m_menu->addAction(tr("Copy last transcription"), [this] { m_backend->copyLast(); });
    m_menu->addSeparator();
    m_quit = m_menu->addAction(tr("Quit duckvox"), [] { QCoreApplication::quit(); });
    m_tray->setContextMenu(m_menu);

    // Left click toggles the window; the context menu owns right click.
    QObject::connect(m_tray, &QSystemTrayIcon::activated, this,
                     [this](QSystemTrayIcon::ActivationReason reason) {
                         if (reason != QSystemTrayIcon::Trigger || !m_window)
                             return;
                         if (m_window->isVisible())
                             m_window->hide();
                         else {
                             m_window->show();
                             m_window->raise();
                             m_window->requestActivate();
                         }
                     });

    // The icon and the menu follow the daemon's state machine.
    QObject::connect(m_backend, &Backend::stateChanged, this, [this] {
        const QString state = m_backend->state();
        if (state == QLatin1String("listening")) {
            m_tray->setToolTip(tr("duckvox — listening (release to insert)"));
            refreshIcon();
        } else if (state == QLatin1String("processing")) {
            m_tray->setToolTip(tr("duckvox — transcribing"));
            refreshIcon();
        } else {
            m_tray->setToolTip(tr("duckvox — idle"));
            refreshIcon();
        }
        rebuildMenu();
    });

    m_tray->show();
}

void TrayIcon::windowVisible(bool visible) {
    if (m_window)
        m_window->setVisible(visible);
}

void TrayIcon::refreshIcon() {
    const QString state = m_backend->state();
    if (state == QLatin1String("listening"))
        m_tray->setIcon(micIcon(QColor(0xFF, 0x45, 0x3A))); // Omarchy urgent red
    else if (state == QLatin1String("processing"))
        m_tray->setIcon(micIcon(QColor(0x30, 0xD1, 0x58))); // Omarchy good green
    else
        m_tray->setIcon(micIcon(QColor(0xEE, 0xEE, 0xEE)));
}

void TrayIcon::rebuildMenu() {
    const QString state = m_backend->state();
    if (state == QLatin1String("listening")) {
        m_state->setText(tr("Listening"));
        m_toggle->setText(tr("Stop dictating"));
        m_toggle->setEnabled(true);
    } else if (state == QLatin1String("processing")) {
        m_state->setText(tr("Transcribing…"));
        m_toggle->setText(tr("Stop dictating"));
        m_toggle->setEnabled(false);
    } else {
        m_state->setText(tr("Idle"));
        m_toggle->setText(tr("Start dictating"));
        m_toggle->setEnabled(true);
    }
    m_copy->setEnabled(!m_backend->lastText().isEmpty());
}
