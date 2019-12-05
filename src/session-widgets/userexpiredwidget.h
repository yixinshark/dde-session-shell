﻿/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     zorowk <near.kingzero@gmail.com>
 *
 * Maintainer: zorowk <near.kingzero@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USEREXPIREDWIDGET_H
#define USEREXPIREDWIDGET_H

#include "sessionbasemodel.h"

#include <darrowrectangle.h>
#include <DBlurEffectWidget>
#include <DFloatingButton>
#include <DLineEdit>

DWIDGET_USE_NAMESPACE

class User;
class UserAvatar;
class LoginButton;
class FrameDataBind;
class QVBoxLayout;
class KeyboardMonitor;

class UserExpiredWidget : public QWidget
{
    Q_OBJECT
public:
    enum AvatarSize {
        AvatarSmallSize = 80,
        AvatarNormalSize = 90,
        AvatarLargeSize = 100
    };

    explicit UserExpiredWidget(QWidget *parent = nullptr);
    ~UserExpiredWidget() override;
    void resetAllState();
    void setFaildTipMessage(const QString &message);
    void setName(const QString &name);
    void setAvatar(const QString &avatar);
    void setUserAvatarSize(const AvatarSize &avatarSize);

signals:
    void requestChangePassword(const QString &password);

public slots:
    void refreshBlurEffectPosition();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void initUI();
    void initConnect();
    void onOtherPageConfirmPasswordChanged(const QVariant &value);
    void onOtherPagePasswordChanged(const QVariant &value);
    void onChangePassword();
    bool errorFilter(const QString &new_pass, const QString &confirm);
    void updateNameLabel();

private:
    DBlurEffectWidget *m_blurEffectWidget;         //阴影窗体
    UserAvatar *m_userAvatar;                      //用户头像
    QLabel *m_nameLbl;                             //用户名
    DLineEdit *m_passwordEdit;                     //新密码输入框
    DLineEdit *m_confirmPasswordEdit;              //新密码确认
    DFloatingButton *m_lockButton;                 //解锁按钮
    QVBoxLayout *m_userLayout;                     //用户输入框布局
    QVBoxLayout *m_lockLayout;                     //解锁按钮布局
    QStringList m_KBLayoutList;
    KeyboardMonitor *m_capslockMonitor;
    QHBoxLayout *m_nameLayout;
    QFrame *m_nameFrame;
    QString m_name;
};

#endif // USEREXPIREDWIDGET_H