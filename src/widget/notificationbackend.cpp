/*
    Copyright © 2015 by The qTox Project

    This file is part of qTox, a Qt-based graphical interface for Tox.

    qTox is libre software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    qTox is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with qTox.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "notificationbackend.h"
#include <QApplication>
#include <QPushButton>
#include <QBoxLayout>

#include <libsnore/settingsdialog.h>

NotificationBackend::NotificationBackend(QObject *parent)
    : QObject(parent)
{
    Snore::Icon icon(QIcon("://img/icons/qtox.svg").pixmap(48, 48).toImage());
    snoreApp = Snore::Application(QApplication::applicationName(), icon);
    snoreApp.addAlert(Snore::Alert(typeToString(NewMessage), icon));
    snoreApp.addAlert(Snore::Alert(typeToString(Highlighted), icon));
    snoreApp.addAlert(Snore::Alert(typeToString(FileTransferFinished), icon));
    snoreApp.addAlert(Snore::Alert(typeToString(FriendRequest), icon));
    snoreApp.addAlert(Snore::Alert(typeToString(AVCall), icon));

    snoreApp.hints().setValue("windows-app-id", "ToxFoundation.qTox");
    snoreApp.hints().setValue("desktop-entry", QApplication::applicationName());

    Snore::SnoreCore::instance().loadPlugins(Snore::SnorePlugin::BACKEND);
    Snore::SnoreCore::instance().registerApplication(snoreApp);
    Snore::SnoreCore::instance().setPrimaryNotificationBackend("Snore");
}

void NotificationBackend::notify(Type type, GenericChatroomWidget *chat, const QString &title, const QString &message)
{
    Snore::Icon icon(QImage{});
    Snore::Notification notification(snoreApp, snoreApp.alerts().values()[type], title, message, icon);
    connect(&Snore::SnoreCore::instance(), &Snore::SnoreCore::actionInvoked, this, &NotificationBackend::notificationInvoked);
    connect(&Snore::SnoreCore::instance(), &Snore::SnoreCore::notificationClosed, this, &NotificationBackend::notificationClose);

    Snore::SnoreCore::instance().broadcastNotification(notification);

    chatList[notification.id()] = chat;
}

QWidget* NotificationBackend::settingsWidget()
{
    QWidget* settings = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(settings);
    Snore::SettingsDialog* dialog = new Snore::SettingsDialog(settings);
    dialog->layout()->setMargin(0);
    QPushButton* apply = new QPushButton(tr("Apply"), settings);

    connect(apply, &QPushButton::clicked, [dialog]()
    {
        dialog->accept();
    });

    layout->addWidget(dialog);

    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->addStretch(1);
    hlayout->addWidget(apply);
    layout->addLayout(hlayout);

    return settings;
}

void NotificationBackend::setOptions(const QString& option)
{
    Snore::SnoreCore::instance().setPrimaryNotificationBackend(option);
}

void NotificationBackend::notificationInvoked(Snore::Notification notification)
{
    emit activated(chatList[notification.id()]);
}

void NotificationBackend::notificationClose(Snore::Notification notification)
{
    chatList.remove(notification.id());
}

QString NotificationBackend::typeToString(Type type)
{
    switch (type)
    {
        case NewMessage:
            return tr("New Message");
        case Highlighted:
            return tr("Highlighted");
        case FileTransferFinished:
            return tr("File Transfer Finished");
        case FriendRequest:
            return tr("Friend Request");
        case AVCall:
            return tr("AV Call");
        default:
            return QString();
    }
}