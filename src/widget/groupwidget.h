/*
    Copyright © 2014-2015 by The qTox Project

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

#ifndef GROUPWIDGET_H
#define GROUPWIDGET_H

#include "genericchatroomwidget.h"

class GroupWidget final : public GenericChatroomWidget
{
    Q_OBJECT
public:
    GroupWidget(int GroupId, QString Name);
    virtual void setAsInactiveChatroom() final override;
    virtual void setAsActiveChatroom() final override;
    virtual void updateStatusLight() final override;
    virtual void setChatForm(Ui::MainWindow &) final override;
    virtual void resetEventFlags() final override;
    virtual QString getStatusString() final override;
    void setName(const QString& name);
    void onUserListChanged();
    void editName();

signals:
    void groupWidgetClicked(GroupWidget* widget);
    void renameRequested(GroupWidget* widget, const QString& newName);
    void removeGroup(int groupId);

protected:
    virtual void contextMenuEvent(QContextMenuEvent * event) final override;
    virtual void dragEnterEvent(QDragEnterEvent* ev) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* ev);
    virtual void dropEvent(QDropEvent* ev) override;

public:
    int groupId;
};

#endif // GROUPWIDGET_H
