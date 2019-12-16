#include "background.h"

#include <QtDBus>
#include <QGSettings>

#define DBUS_APPEARANCE_SERVICE "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF "com.deepin.daemon.Appearance"

#define DBUS_DEEPIN_WM_SERVICE "com.deepin.wm"
#define DBUS_DEEPIN_WM_OBJ "/com/deepin/wm"
#define DBUS_DEEPIN_WM_INTF "com.deepin.wm"

const char fallback_background_name[] = "file:///usr/share/backgrounds/default_background.jpg";

Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gs_dde_appearance, ("com.deepin.dde.appearance"))

#define GsettingsBackgroundUri "backgroundUris"

BackgroundManager& BackgroundManager::instance()
{
    static BackgroundManager* _self = nullptr;
    if (!_self) {
        _self = new BackgroundManager();
    }

    return *_self;
}

BackgroundManager::BackgroundManager()
    :QObject()
{
    m_defaultNewDesktopURI = QLatin1String(fallback_background_name);

    connect(_gs_dde_appearance, &QGSettings::changed, this, &BackgroundManager::onGsettingsDDEAppearanceChanged);
    // hook screen signals

    emit defaultBackgroundURIChanged();
}

QPixmap BackgroundManager::getBackground(int workspace, int monitor)
{
    if (workspace <= 0) return QPixmap();

    QString uri = QLatin1String(fallback_background_name);
    auto uris = _gs_dde_appearance->get(GsettingsBackgroundUri).toStringList();
    if (workspace > uris.size()) {
        //TODO:
    } else {
        uri = uris.value(workspace - 1);
    }

    if (uri.startsWith("file:///")) {
        uri.remove("file://");
    }

    QPixmap pm;
    if (!pm.load(uri)) {
        pm.load(QLatin1String(fallback_background_name));
    }

    //qDebug() << "--------- " << __func__ << workspace << uri << pm.isNull();
    return pm;
}

void BackgroundManager::onGsettingsDDEAppearanceChanged(const QString &key)
{
    qDebug() << "---------- " << __func__ << key;
    if (key == GsettingsBackgroundUri) {
        emit wallpapersChanged();
    }
}

void BackgroundManager::desktopAboutToRemoved(int d)
{
    auto uris = _gs_dde_appearance->get(GsettingsBackgroundUri).toStringList();

    QDBusInterface wm(DBUS_DEEPIN_WM_SERVICE, DBUS_DEEPIN_WM_OBJ, DBUS_DEEPIN_WM_INTF);

    auto n = qMin(uris.size(), m_desktopCount);
    for (int i = d; i < n; i++) {
        qDebug() << "-------- dbus SetWorkspaceBackground" << i << uris[i];
        QDBusReply<QString> reply = wm.call( "SetWorkspaceBackground", i, uris[i]);
    }

}

QString BackgroundManager::getDefaultBackgroundURI()
{
    return m_defaultNewDesktopURI;
}

void BackgroundManager::shuffleDefaultBackgroundURI()
{
    if (m_preinstalledWallpapers.size() == 0) {
        QDBusInterface remoteApp(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ, DBUS_APPEARANCE_INTF);
        QDBusReply<QString> reply = remoteApp.call( "List", "background");

        QJsonDocument json = QJsonDocument::fromJson(reply.value().toUtf8());
        QJsonArray arr = json.array();
        if (!arr.isEmpty()) {
            auto p = arr.constBegin();
            while (p != arr.constEnd()) {
                auto o = p->toObject();
                if (!o.value("Id").isUndefined() && !o.value("Deletable").toBool()) {
                    m_preinstalledWallpapers << o.value("Id").toString();
                }
                ++p;
            }

            //qDebug() << m_preinstalledWallpapers;
        }
    }

    if (m_preinstalledWallpapers.size() > 0) {
        int id = QRandomGenerator::global()->bounded(m_preinstalledWallpapers.size());
        m_defaultNewDesktopURI = m_preinstalledWallpapers[id];
        emit defaultBackgroundURIChanged();
    }
}

