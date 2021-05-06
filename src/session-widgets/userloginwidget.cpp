/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     lixin <lixin_cm@deepin.com>
 *
 * Maintainer: lixin <lixin_cm@deepin.com>
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

#include "userloginwidget.h"

#include "constants.h"
#include "dhidpihelper.h"
#include "dpasswordeditex.h"
#include "framedatabind.h"
#include "kblayoutwidget.h"
#include "keyboardmonitor.h"
#include "lockpasswordwidget.h"
#include "loginbutton.h"
#include "useravatar.h"
#include "userinfo.h"
#include "authenticationmodule.h"

#include <DFontSizeManager>
#include <DPalette>

#include <QAction>
#include <QImage>
#include <QPropertyAnimation>
#include <QVBoxLayout>

static const int BlurRectRadius = 15;
static const int WidgetsSpacing = 10;
static const int Margins = 16;
static const QColor shutdownColor(QColor(247, 68, 68));
static const QColor disableColor(QColor(114, 114, 114));

UserLoginWidget::UserLoginWidget(const SessionBaseModel *model, const WidgetType widgetType, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
    , m_widgetType(widgetType)
    , m_blurEffectWidget(new DBlurEffectWidget(this))
    , m_userLoginLayout(new QVBoxLayout(this))
    , m_userAvatar(new UserAvatar(this))
    , m_nameWidget(new QWidget(this))
    , m_nameLabel(new QLabel(m_nameWidget))
    , m_loginStateLabel(new QLabel(m_nameWidget))
    , m_accountEdit(new DLineEditEx(this))
    , m_lockButton(new DFloatingButton(DStyle::SP_LockElement, this))
    , m_passwordAuth(nullptr)
    , m_fingerprintAuth(nullptr)
    , m_faceAuth(nullptr)
    , m_ukeyAuth(nullptr)
    , m_activeDirectoryAuth(nullptr)
    , m_fingerVeinAuth(nullptr)
    , m_irisAuth(nullptr)
    , m_PINAuth(nullptr)

    , m_kbLayoutBorder(nullptr)

    , m_isLock(false)
    , m_loginState(true)
    , m_isSelected(false)
    , m_isLockNoPassword(false)
    , m_isAlertMessageShow(false)
    , m_aniTimer(new QTimer(this))
{
    setGeometry(0, 0, 280, 280);                           // 设置本窗口默认大小和相对父窗口的位置
    setMinimumSize(228, 146);                              // 设置本窗口的最小大小（参考用户列表模式下的最小大小）
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // 大小策略设置为fixed，使本窗口不受父窗口布局管理器的拉伸效果
    setFocusPolicy(Qt::NoFocus);

    m_capslockMonitor = KeyboardMonitor::instance();
    m_capslockMonitor->start(QThread::LowestPriority);

    initUI();
    initConnections();

    m_accountEdit->installEventFilter(this);
    m_accountEdit->hide();

    if (m_widgetType == LoginType) {
        setMaximumWidth(280); // 设置本窗口的最大宽度（参考登录模式下的最大宽度）
        m_userAvatar->setAvatarSize(UserAvatar::AvatarLargeSize);
        m_loginStateLabel->hide();
        if (m_model->currentType() == SessionBaseModel::LightdmType && m_model->isServerModel()) {
            m_accountEdit->show();
            m_nameLabel->hide();
        }
    } else {
        setMaximumWidth(228); // 设置本窗口的最大宽度（参考用户列表模式下的最大宽度）
        m_userAvatar->setAvatarSize(UserAvatar::AvatarSmallSize);
        m_lockButton->hide();
    }
}

UserLoginWidget::~UserLoginWidget()
{
    for (auto it = m_registerFunctionIndexs.constBegin(); it != m_registerFunctionIndexs.constEnd(); ++it) {
        FrameDataBind::Instance()->unRegisterFunction(it.key(), it.value());
    }
    m_kbLayoutBorder->deleteLater();
}

/**
 * @brief 登录窗口布局
 */
void UserLoginWidget::initUI()
{
    m_userLoginLayout->setContentsMargins(10, 0, 10, 0);
    m_userLoginLayout->setSpacing(10);
    /* 头像 */
    m_userAvatar->setFocusPolicy(Qt::NoFocus);
    m_userLoginLayout->addWidget(m_userAvatar, 0, Qt::AlignHCenter);
    /* 用户名 */
    QHBoxLayout *nameLayout = new QHBoxLayout(m_nameWidget);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->setSpacing(5);
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(":/icons/dedpin/builtin/select.svg");
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_loginStateLabel->setPixmap(pixmap);
    nameLayout->addWidget(m_loginStateLabel, 0, Qt::AlignRight);
    m_nameLabel->setTextFormat(Qt::TextFormat::PlainText);
    DFontSizeManager::instance()->bind(m_nameLabel, DFontSizeManager::T2);
    QPalette palette = m_nameLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_nameLabel->setPalette(palette);
    nameLayout->addWidget(m_nameLabel, 1, Qt::AlignLeft);
    m_userLoginLayout->addWidget(m_nameWidget, 0, Qt::AlignCenter);
    /* 用户名输入框 */
    m_accountEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_accountEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    m_accountEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_accountEdit->setClearButtonEnabled(false);
    m_accountEdit->setPlaceholderText(tr("Account"));
    m_userLoginLayout->addWidget(m_accountEdit);
    /* 解锁图标与上面控件的间距 */
    if (m_widgetType != UserListType) {
        m_userLoginLayout->addSpacing(10);
    } else {
        m_userLoginLayout->addSpacing(20);
    }
    /* 解锁图标 */
    m_lockButton->setFocusPolicy(Qt::StrongFocus);
    m_userLoginLayout->addWidget(m_lockButton, 0, Qt::AlignHCenter);
    /* 模糊背景 */
    m_blurEffectWidget->setMaskColor(DBlurEffectWidget::LightColor);
    m_blurEffectWidget->setMaskAlpha(76); // fix BUG 3400 设置模糊窗体的不透明度为30%
    m_blurEffectWidget->setBlurRectXRadius(BlurRectRadius);
    m_blurEffectWidget->setBlurRectYRadius(BlurRectRadius);
    /* 键盘布局菜单 */
    m_kbLayoutBorder = new DArrowRectangle(DArrowRectangle::ArrowTop);
    m_kbLayoutWidget = new KbLayoutWidget(QStringList());
    m_kbLayoutBorder->setContent(m_kbLayoutWidget);
    m_kbLayoutBorder->setBackgroundColor(QColor(102, 102, 102)); //255*0.2
    m_kbLayoutBorder->setBorderColor(QColor(0, 0, 0, 0));
    m_kbLayoutBorder->setBorderWidth(0);
    m_kbLayoutBorder->setContentsMargins(0, 0, 0, 0);
    m_kbLayoutClip = new Dtk::Widget::DClipEffectWidget(m_kbLayoutBorder);
    updateClipPath();
}

void UserLoginWidget::initConnections()
{
    /* 头像 */
    connect(m_userAvatar, &UserAvatar::clicked, this, &UserLoginWidget::clicked);
    /* 用户名 */
    connect(qGuiApp, &QGuiApplication::fontChanged, this, &UserLoginWidget::updateNameLabel);
    /* 用户名输入框 */
    std::function<void(QVariant)> accountChanged = std::bind(&UserLoginWidget::onOtherPageAccountChanged, this, std::placeholders::_1);
    m_registerFunctionIndexs["UserLoginAccount"] = FrameDataBind::Instance()->registerFunction("UserLoginAccount", accountChanged);
    connect(m_accountEdit, &DLineEditEx::textChanged, this, [=] (const QString &value) {
        FrameDataBind::Instance()->updateValue("UserLoginAccount", value);
    });
    FrameDataBind::Instance()->refreshData("UserLoginAccount");
    connect(m_accountEdit, &DLineEditEx::returnPressed, this, [=] {
        if (m_accountEdit->isVisible() && !m_accountEdit->text().isEmpty()) {
            emit requestCheckAccount(m_accountEdit->text());
        }
    });
    /* 解锁按钮 */
    connect(m_lockButton, &DFloatingButton::clicked, this, [=] {
        if (m_model->currentUser()->isNoPasswdGrp()) {
            emit requestCheckAccount(m_model->currentUser()->name());
        }
    });
    /* 键盘布局菜单 */
    connect(m_kbLayoutWidget, &KbLayoutWidget::setButtonClicked, this, &UserLoginWidget::requestUserKBLayoutChanged);
    std::function<void(QVariant)> kblayoutChanged = std::bind(&UserLoginWidget::onOtherPageKBLayoutChanged, this, std::placeholders::_1);
    m_registerFunctionIndexs["UserLoginKBLayout"] = FrameDataBind::Instance()->registerFunction("UserLoginKBLayout", kblayoutChanged);
    FrameDataBind::Instance()->refreshData("UserLoginKBLayout");
}

/**
 * @brief 根据开启的认证类型更新对应的界面显示。
 *
 * @param type
 */
void UserLoginWidget::updateWidgetShowType(const int type)
{
    int index = 3;
    /**
     * @brief 设置布局
     * 这里是按照显示顺序初始化的，如果后续布局改变或者新增认证方式，初始化顺序需要重新调整
     */
    /* 面容 */
    if (type & AuthenticationModule::AuthTypeFace) {
        initFaceAuth(index++);
    } else if (m_faceAuth != nullptr) {
        m_faceAuth->deleteLater();
        m_faceAuth = nullptr;
    }
    /* 指纹 */
    if (type & AuthenticationModule::AuthTypeFingerprint) {
        initFingerprintAuth(index++);
    } else if (m_fingerprintAuth != nullptr) {
        m_fingerprintAuth->deleteLater();
        m_fingerprintAuth = nullptr;
    }
    /* AD域 */
    if (type & AuthenticationModule::AuthTypeActiveDirectory) {
        initActiveDirectoryAuth(index++);
    }
    /* Ukey */
    if (type & AuthenticationModule::AuthTypeUkey) {
        initUkeyAuth(index++);
    } else if (m_ukeyAuth != nullptr) {
        m_ukeyAuth->deleteLater();
        m_ukeyAuth = nullptr;
    }
    /* 指静脉 */
    if (type & AuthenticationModule::AuthTypeFingerVein) {
        initFingerVeinAuth(index++);
    }
    /* 虹膜 */
    if (type & AuthenticationModule::AuthTypeIris) {
        initIrisAuth(index++);
    }
    /* PIN码 */
    if (type & AuthenticationModule::AuthTypePIN) {
        initPINAuth(index++);
    } else if (m_PINAuth != nullptr) {
        m_PINAuth->deleteLater();
        m_PINAuth = nullptr;
    }
    /* 密码 */
    if (type & AuthenticationModule::AuthTypePassword) {
        initPasswdAuth(index++);
    } else if (m_passwordAuth != nullptr) {
        m_passwordAuth->deleteLater();
        m_passwordAuth = nullptr;
    }

    /**
     * @brief 设置焦点
     * 优先级: 用户名输入框 > PIN码输入框 > 密码输入框 > 解锁按钮
     * 这里的优先级是根据布局来的，如果后续布局改变或者新增认证方式，焦点需要重新调整
     */
    if (m_lockButton->isVisible()) {
        m_lockButton->setFocus();
    }
    if (type & AuthenticationModule::AuthTypePassword) {
        m_passwordAuth->setFocus();
    }
    if (type & AuthenticationModule::AuthTypeUkey) {
        m_ukeyAuth->setFocus();
    }
    if (type & AuthenticationModule::AuthTypePIN) {
        m_PINAuth->setFocus();
    }
    if (m_accountEdit->isVisible()) {
        m_accountEdit->setFocus();
    }
}

/**
 * @brief 密码认证
 */
void UserLoginWidget::initPasswdAuth(const int index)
{
    if (m_passwordAuth != nullptr) {
        return;
    }
    m_passwordAuth = new AuthenticationModule(AuthenticationModule::AuthTypePassword, this);
    m_passwordAuth->setLineEditInfo(tr("Password"), AuthenticationModule::PlaceHolderText);
    m_passwordAuth->setCapsStatus(m_capslockMonitor->isCapslockOn());
    m_passwordAuth->setAuthStatus(":/icons/dedpin/builtin/select.svg");
    m_userLoginLayout->insertWidget(index, m_passwordAuth);

    connect(m_passwordAuth, &AuthenticationModule::activateAuthentication, this, [=] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthenticationModule::AuthTypePassword);
    });
    connect(m_passwordAuth, &AuthenticationModule::requestAuthenticate, this, [=] {
        if (m_passwordAuth->lineEditText().isEmpty()) {
            return;
        }
        m_passwordAuth->setAnimationState(true);
        QString account = m_accountEdit->text().isEmpty() ? m_model->currentUser()->name() : m_accountEdit->text();
        emit sendTokenToAuth(account, AuthenticationModule::AuthTypePassword, m_passwordAuth->lineEditText());
    });
    connect(m_passwordAuth, &AuthenticationModule::requestShowKeyboardList, this, &UserLoginWidget::showKeyboardList);
    connect(m_passwordAuth, &AuthenticationModule::authFinished, this, &UserLoginWidget::checkAuthResult);
    connect(m_lockButton, &QPushButton::clicked, m_passwordAuth, &AuthenticationModule::requestAuthenticate);
    connect(m_capslockMonitor, &KeyboardMonitor::capslockStatusChanged, m_passwordAuth, &AuthenticationModule::setCapsStatus);
    connect(m_kbLayoutWidget, &KbLayoutWidget::setButtonClicked, m_passwordAuth, &AuthenticationModule::setKeyboardButtonInfo);

    /* 输入框数据同步 */
    std::function<void(QVariant)> passwordChanged = std::bind(&UserLoginWidget::onOtherPagePasswordChanged, this, std::placeholders::_1);
    m_registerFunctionIndexs["UserLoginPassword"] = FrameDataBind::Instance()->registerFunction("UserLoginPassword", passwordChanged);
    connect(m_passwordAuth, &AuthenticationModule::lineEditTextChanged, this, [=] (const QString &value) {
        FrameDataBind::Instance()->updateValue("UserLoginPassword", value);
        if(value.length() > 0)
           m_lockButton->setEnabled(true);
        else {
           m_lockButton->setEnabled(false);
        }
    });
    FrameDataBind::Instance()->refreshData("UserLoginPassword");
}

/**
 * @brief 指纹认证
 */
void UserLoginWidget::initFingerprintAuth(const int index)
{
    if (m_fingerprintAuth != nullptr) {
        return;
    }
    m_fingerprintAuth = new AuthenticationModule(AuthenticationModule::AuthTypeFingerprint, this);
    m_fingerprintAuth->setText("Fingerprint ID");
    m_fingerprintAuth->setAuthStatus(":/icons/dedpin/builtin/select.svg");
    m_userLoginLayout->insertWidget(index, m_fingerprintAuth);

    connect(m_fingerprintAuth, &AuthenticationModule::activateAuthentication, this, [=] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthenticationModule::AuthTypeFingerprint);
    });
    connect(m_fingerprintAuth, &AuthenticationModule::authFinished, this, &UserLoginWidget::checkAuthResult);
}

/**
 * @brief 面容认证
 */
void UserLoginWidget::initFaceAuth(const int index)
{
    if (m_faceAuth != nullptr) {
        return;
    }
    m_faceAuth = new AuthenticationModule(AuthenticationModule::AuthTypeFace, this);
    m_faceAuth->setText("Face ID");
    m_faceAuth->setAuthStatus(":/icons/dedpin/builtin/select.svg");
    m_userLoginLayout->insertWidget(index, m_faceAuth);

    connect(m_faceAuth, &AuthenticationModule::activateAuthentication, this, [=] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthenticationModule::AuthTypeFace);
    });
    connect(m_faceAuth, &AuthenticationModule::authFinished, this, &UserLoginWidget::checkAuthResult);
}

/**
 * @brief AD域认证
 */
void UserLoginWidget::initActiveDirectoryAuth(const int index)
{
    Q_UNUSED(index)
    // TODO
}

/**
 * @brief Ukey认证
 */
void UserLoginWidget::initUkeyAuth(const int index)
{
    if (m_ukeyAuth != nullptr) {
        return;
    }
    m_ukeyAuth = new AuthenticationModule(AuthenticationModule::AuthTypeUkey, this);
    m_ukeyAuth->setLineEditInfo(tr("Please enter the PIN code"), AuthenticationModule::PlaceHolderText);
    m_ukeyAuth->setCapsStatus(m_capslockMonitor->isCapslockOn());
    m_ukeyAuth->setAuthStatus(":/icons/dedpin/builtin/select.svg");
    m_userLoginLayout->insertWidget(index, m_ukeyAuth);

    connect(m_ukeyAuth, &AuthenticationModule::activateAuthentication, this, [=] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthenticationModule::AuthTypeUkey);
    });
    connect(m_ukeyAuth, &AuthenticationModule::requestAuthenticate, this, [=] {
        if (m_ukeyAuth->lineEditText().isEmpty()) {
            return;
        }
        m_ukeyAuth->setAnimationState(true);
        emit sendTokenToAuth(m_model->currentUser()->name(), AuthenticationModule::AuthTypeUkey, m_ukeyAuth->lineEditText());
    });
    connect(m_lockButton, &QPushButton::clicked, m_ukeyAuth, &AuthenticationModule::requestAuthenticate);
    connect(m_ukeyAuth, &AuthenticationModule::authFinished, this, &UserLoginWidget::checkAuthResult);
    connect(m_capslockMonitor, &KeyboardMonitor::capslockStatusChanged, m_ukeyAuth, &AuthenticationModule::setCapsStatus);

    /* 输入框数据同步 */
    std::function<void(QVariant)> PINChanged = std::bind(&UserLoginWidget::onOtherPageUKeyChanged, this, std::placeholders::_1);
    m_registerFunctionIndexs["UserLoginUKey"] = FrameDataBind::Instance()->registerFunction("UserLoginUKey", PINChanged);
    connect(m_ukeyAuth, &AuthenticationModule::lineEditTextChanged, this, [=](const QString &value) {
        if (m_model->getAuthProperty().PINLen > 0 && value.size() >= m_model->getAuthProperty().PINLen) {
            emit m_ukeyAuth->requestAuthenticate();
        }
        FrameDataBind::Instance()->updateValue("UserLoginUKey", value);

        if(value.length() > 0){
            m_lockButton->setEnabled(true);
        }else{
            m_lockButton->setEnabled(false);
        }
    });
    FrameDataBind::Instance()->refreshData("UserLoginUKey");
}

/**
 * @brief 指静脉认证
 */
void UserLoginWidget::initFingerVeinAuth(const int index)
{
    Q_UNUSED(index)
    // TODO
}

/**
 * @brief 虹膜认证
 */
void UserLoginWidget::initIrisAuth(const int index)
{
    Q_UNUSED(index)
    // TODO
}

/**
 * @brief PIN码认证
 */
void UserLoginWidget::initPINAuth(const int index)
{
    if (m_PINAuth != nullptr) {
        return;
    }
    m_PINAuth = new AuthenticationModule(AuthenticationModule::AuthTypePIN, this);
    m_PINAuth->setLineEditInfo(tr("Please enter the PIN code"), AuthenticationModule::PlaceHolderText);
    m_PINAuth->setAuthStatus(":/icons/dedpin/builtin/select.svg");
    m_userLoginLayout->insertWidget(index, m_PINAuth);

    std::function<void(QVariant)> PINChanged = std::bind(&UserLoginWidget::onOtherPagePINChanged, this, std::placeholders::_1);
    m_registerFunctionIndexs["UserLoginPIN"] = FrameDataBind::Instance()->registerFunction("UserLoginPIN", PINChanged);
    connect(m_PINAuth, &AuthenticationModule::lineEditTextChanged, this, [=] (const QString &value) {
        FrameDataBind::Instance()->updateValue("UserLoginPIN", value);
        if(value.length() > 0 )
            m_lockButton->setEnabled(true);
        else {
            m_lockButton->setEnabled(false);
        }
    });
    connect(m_PINAuth, &AuthenticationModule::activateAuthentication, this, [=] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthenticationModule::AuthTypePIN);
    });
    connect(m_PINAuth, &AuthenticationModule::requestAuthenticate, this, [=] {
        qDebug() << "PIN:" << m_PINAuth->lineEditText();
        if (m_PINAuth->lineEditText().isEmpty()) {
            return;
        }
        m_PINAuth->setAnimationState(true);
        QString account = m_accountEdit->text().isEmpty() ? m_model->currentUser()->name() : m_accountEdit->text();
        emit sendTokenToAuth(account, AuthenticationModule::AuthTypePIN, m_PINAuth->lineEditText());
    });
    connect(m_lockButton, &QPushButton::clicked, m_PINAuth, &AuthenticationModule::requestAuthenticate);
    connect(m_PINAuth, &AuthenticationModule::authFinished, this, &UserLoginWidget::checkAuthResult);
    connect(m_capslockMonitor, &KeyboardMonitor::capslockStatusChanged, m_PINAuth, &AuthenticationModule::setCapsStatus);
    FrameDataBind::Instance()->refreshData("UserLoginPIN");
}

/**
 * @brief 根据返回的认证结果更新界面显示。分别返回各个认证类型的结果，最后返回总的认证结果。
 * @param authType   认证类型
 * @param status     认证结果
 * @param message    返回的结果文案（成功，失败，等）
 */
void UserLoginWidget::updateAuthResult(const int authType, const int status, const QString &message)
{
    switch (authType) {
    case AuthenticationModule::AuthTypePassword:
        if (m_passwordAuth != nullptr) {
            m_passwordAuth->setAuthResult(status, message);
        }
        break;
    case AuthenticationModule::AuthTypeFingerprint:
        if (m_fingerprintAuth != nullptr) {
            m_fingerprintAuth->setAuthResult(status, message);
        }
        break;
    case AuthenticationModule::AuthTypeFace:
        if (m_faceAuth != nullptr) {
            m_faceAuth->setAuthResult(status, message);
        }
        break;
    case AuthenticationModule::AuthTypeActiveDirectory:
        if (m_activeDirectoryAuth != nullptr) {
            m_activeDirectoryAuth->setAuthResult(status, message);
        }
        break;
    case AuthenticationModule::AuthTypeUkey:
        if (m_ukeyAuth != nullptr) {
            m_ukeyAuth->setAuthResult(status, message);
        }
        break;
    case AuthenticationModule::AuthTypeFingerVein:
        if (m_fingerVeinAuth != nullptr) {
            m_fingerVeinAuth->setAuthResult(status, message);
        }
        break;
    case AuthenticationModule::AuthTypeIris:
        if (m_irisAuth != nullptr) {
            m_irisAuth->setAuthResult(status, message);
        }
        break;
    case AuthenticationModule::AuthTypeAll:
        checkAuthResult(authType, status);
        break;
    default:
        break;
    }
}

/**
 * @brief 更新展示的头像
 *
 * @param avatar 头像图片路径
 */
void UserLoginWidget::updateAvatar(const QString &path)
{
    if (m_userAvatar == nullptr) {
        return;
    }

    m_userAvatar->setIcon(path);
}

//密码连续输入错误5次，设置提示信息
void UserLoginWidget::setFaildMessage(const QString &message, SessionBaseModel::AuthFaildType type)
{
    if (!message.isEmpty()) {
        // m_passwordEdit->setPlaceholderText(message);
        if (type == SessionBaseModel::KEYBOARD) {
            // m_passwordEdit->hideLoadSlider();
        } else {
            // m_passwordEdit->hideAlertMessage();
        }
    }
}

/**
 * @brief 显示用户名输入错误
 *
 * @param message
 */
void UserLoginWidget::setFaildTipMessage(const QString &message)
{
    if (m_accountEdit->isVisible()) {
        m_accountEdit->showAlertMessage(message);
    }
}

/**
 * @brief 关机提示
 *
 * @param action
 */
void UserLoginWidget::ShutdownPrompt(SessionBaseModel::PowerAction action)
{
    m_action = action;

    QPalette lockPalatte;
    switch (m_action) {
    case SessionBaseModel::PowerAction::RequireRestart:
        m_lockButton->setIcon(QIcon(":/img/bottom_actions/reboot.svg"));
        lockPalatte.setColor(QPalette::Highlight, shutdownColor);
        break;
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_lockButton->setIcon(QIcon(":/img/bottom_actions/shutdown.svg"));
        lockPalatte.setColor(QPalette::Highlight, shutdownColor);
        break;
    default:
        if (m_authType == SessionBaseModel::LightdmType) {
            m_lockButton->setIcon(DStyle::SP_ArrowNext);
            return;
        } else {
            m_lockButton->setIcon(DStyle::SP_LockElement);
        }
    }
    m_lockButton->setPalette(lockPalatte);
}

/**
 * @brief 焦点同步
 *
 * @param value
 */
void UserLoginWidget::onOtherPageFocusChanged(const QVariant &value)
{
}

void UserLoginWidget::updateLoginEditLocale(const QLocale &locale)
{
    m_local = locale.name();
    if ("en_US" == locale.name()) {
        m_passwordEdit->lineEdit()->setPlaceholderText("Password");
        m_accountEdit->lineEdit()->setPlaceholderText("Account");
    } else {
        QTranslator translator;
        translator.load("/usr/share/dde-session-shell/translations/dde-session-shell_" + locale.name());
        qApp->installTranslator(&translator);
        m_passwordEdit->lineEdit()->setPlaceholderText(tr("Password"));
        m_accountEdit->lineEdit()->setPlaceholderText(tr("Account"));
    }
}

/**
 * @brief 用户名输入框数据同步
 *
 * @param value
 */
void UserLoginWidget::onOtherPageAccountChanged(const QVariant &value)
{
    int cursorIndex = m_accountEdit->lineEdit()->cursorPosition();
    m_accountEdit->setText(value.toString());
    m_accountEdit->lineEdit()->setCursorPosition(cursorIndex);
}

/**
 * @brief PIN码输入框数据同步
 *
 * @param value
 */
void UserLoginWidget::onOtherPagePINChanged(const QVariant &value)
{
    m_PINAuth->setLineEditInfo(value.toString(), AuthenticationModule::InputText);
}

/**
 * @brief ukey 输入框数据同步
 * @param value
 */
void UserLoginWidget::onOtherPageUKeyChanged(const QVariant &value)
{
    m_ukeyAuth->setLineEditInfo(value.toString(), AuthenticationModule::InputText);
}

/**
 * @brief 密码输入框数据同步
 *
 * @param value
 */
void UserLoginWidget::onOtherPagePasswordChanged(const QVariant &value)
{
    m_passwordAuth->setLineEditInfo(value.toString(), AuthenticationModule::InputText);
}

/**
 * @brief 键盘布局同步
 *
 * @param value
 */
void UserLoginWidget::onOtherPageKBLayoutChanged(const QVariant &value)
{
    if (value.toBool()) {
        m_kbLayoutBorder->setParent(window());
    }

    m_kbLayoutBorder->setVisible(value.toBool());

    if (m_kbLayoutBorder->isVisible()) {
        m_kbLayoutBorder->raise();
    }

    updateKeyboardListPosition();
}

/**
 * @brief 显示/隐藏键盘布局菜单
 */
void UserLoginWidget::showKeyboardList()
{
    if (m_kbLayoutBorder->isVisible()) {
        m_kbLayoutBorder->hide();
    } else {
        // 保证布局选择控件不会被其它控件遮挡
        // 必须要将它作为主窗口的子控件
        m_kbLayoutBorder->setParent(window());
        m_kbLayoutBorder->setVisible(true);
        m_kbLayoutBorder->raise();
        updateKeyboardListPosition();
    }
    FrameDataBind::Instance()->updateValue("UserLoginKBLayout", m_kbLayoutBorder->isVisible());
    updateClipPath();
}

/**
 * @brief 更新键盘布局菜单位置
 */
void UserLoginWidget::updateKeyboardListPosition()
{
    const QPoint &point = mapTo(m_kbLayoutBorder->parentWidget(), QPoint(m_blurEffectWidget->geometry().left() + m_blurEffectWidget->width() / 2,
                                                                         m_blurEffectWidget->geometry().bottom() - 10));
    m_kbLayoutBorder->move(point.x(), point.y());
    m_kbLayoutBorder->setArrowX(15);
    updateClipPath();
}

/**
 * @brief 输入5次错误密码后，显示提示信息，并更新各个输入框禁用状态
 *
 * @param disable
 * @param lockTime
 */
void UserLoginWidget::disablePassword(bool disable, uint lockTime)
{
    m_isLock = disable;

    //    if (m_accountEdit->isVisible()) {
    //        m_accountEdit->setDisabled(disable);
    //    }
    //    m_passwordAuth->setDisabled(disable);

    if (disable) {
        if ("en_US" == m_local) {
            if (lockTime > 1) {
                setFaildMessage(QString("Please try again %1 minutes later").arg(int(lockTime)));
            } else {
                setFaildMessage(QString("Please try again %1 minute later").arg(int(lockTime)));
            }
        } else {
            QTranslator translator;
            translator.load("/usr/share/dde-session-shell/translations/dde-session-shell_" + m_local);
            qApp->installTranslator(&translator);
            setFaildMessage(tr("Please try again %n minute(s) later", "", int(lockTime)));
        }
    } else {
        // m_passwordEdit->setFocus();
    }
}

/**
 * @brief 更新解锁按钮样式  TODO
 *
 * @param type
 */
void UserLoginWidget::updateAuthType(SessionBaseModel::AuthType type)
{
    m_authType = type;

    if (m_authType == SessionBaseModel::LightdmType) {
        m_lockButton->setIcon(DStyle::SP_ArrowNext);
    } else {
        m_lockButton->setIcon(DStyle::SP_LockElement);
    }
}

/**
 * @brief 更新模糊背景大小
 */
void UserLoginWidget::updateBlurEffectGeometry()
{
    QRect rect = layout()->geometry();
    rect.setTop(rect.top() + m_userAvatar->height() / 2);
    if (m_widgetType == LoginType) {
        rect.setBottom(rect.bottom() - m_lockButton->height() - layout()->spacing());
    } else {
        rect.setBottom(rect.bottom() - 15);
    }
    m_blurEffectWidget->setGeometry(rect);
}

/**
 * @brief 用户改变键盘布局后，触发这里同步更新菜单列表
 *
 * @param kbLayoutList
 */
void UserLoginWidget::updateKeyboardList(const QStringList &list)
{
    static QStringList tmpList;
    if (list == tmpList) {
        return;
    }
    tmpList = list;
    if (m_kbLayoutWidget != nullptr && m_kbLayoutBorder != nullptr) {
        m_kbLayoutWidget->updateButtonList(list);
        m_kbLayoutBorder->setContent(m_kbLayoutWidget);
        updateClipPath();
    }
    if (m_passwordAuth != nullptr) {
        m_passwordAuth->setKeyboardButtonVisible(list.size() > 1 ? true : false);
    }
}

/**
 * @brief 更新账户限制信息
 *
 * @param limitsInfo
 */
void UserLoginWidget::updateLimitsInfo(const QMap<int, User::LimitsInfo> *limitsInfo)
{
    AuthenticationModule::LimitsInfo limitsInfoTmpA;
    User::LimitsInfo limitsInfoTmpU;

    QMap<int, User::LimitsInfo>::const_iterator i = limitsInfo->constBegin();
    while (i != limitsInfo->end()) {
        limitsInfoTmpU = i.value();
        switch (i.key()) {
        case AuthenticationModule::AuthTypePassword:
            if (m_passwordAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_passwordAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        case AuthenticationModule::AuthTypeFingerprint:
            if (m_fingerprintAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_fingerprintAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        case AuthenticationModule::AuthTypeFace:
            if (m_faceAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_faceAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        case AuthenticationModule::AuthTypeActiveDirectory:
            if (m_activeDirectoryAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_activeDirectoryAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        case AuthenticationModule::AuthTypeUkey:
            if (m_ukeyAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_ukeyAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        case AuthenticationModule::AuthTypeFingerVein:
            if (m_fingerVeinAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_fingerVeinAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        case AuthenticationModule::AuthTypeIris:
            if (m_irisAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_irisAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        case AuthenticationModule::AuthTypePIN:
            if (m_PINAuth != nullptr) {
                limitsInfoTmpA.locked = limitsInfoTmpU.locked;
                limitsInfoTmpA.maxTries = limitsInfoTmpU.maxTries;
                limitsInfoTmpA.numFailures = limitsInfoTmpU.numFailures;
                limitsInfoTmpA.unlockSecs = limitsInfoTmpU.unlockSecs;
                limitsInfoTmpA.unlockTime = limitsInfoTmpU.unlockTime;
                m_PINAuth->setLimitsInfo(limitsInfoTmpA);
            }
            break;
        default:
            qWarning() << "Error! Authentication type is wrong." << i.key();
            break;
        }
        ++i;
    }
}

/**
 * @brief 设置当前键盘布局
 *
 * @param layout
 */
void UserLoginWidget::updateKeyboardInfo(const QString &text)
{
    m_kbLayoutBorder->hide();
    static QString tmpText;
    if (text == tmpText) {
        return;
    }
    tmpText = text;
    m_kbLayoutWidget->setDefault(text);
    if (m_passwordAuth != nullptr) {
        m_passwordAuth->setKeyboardButtonInfo(text);
    }
}

/**
 * @brief 设置用户的 uid 用于用户列表排序
 *
 * @param uid
 */
void UserLoginWidget::setUid(const uint uid)
{
    if (uid == m_uid) {
        return;
    }
    m_uid = uid;
}

/**
 * @brief 设置绘制用户列表选中标识
 *
 * @param isSelected
 */
void UserLoginWidget::setSelected(bool isSelected)
{
    m_isSelected = isSelected;
    update();
}

/**
 * @brief 设置绘制用户列表选中标识
 *
 * @param isSelected
 */
void UserLoginWidget::setFastSelected(bool isSelected)
{
    m_isSelected = isSelected;
    repaint();
}

/**
 * @brief 设置键盘布局下边缘的圆角
 */
void UserLoginWidget::updateClipPath()
{
    if (!m_kbLayoutClip)
        return;
    QRectF rc(0, 0, DDESESSIONCC::PASSWDLINEEIDT_WIDTH, m_kbLayoutBorder->height());
    int iRadius = 20;
    QPainterPath path;
    path.lineTo(0, 0);
    path.lineTo(rc.width(), 0);
    path.lineTo(rc.width(), rc.height() - iRadius);
    path.arcTo(rc.width() - iRadius, rc.height() - iRadius, iRadius, iRadius, 0, -90);
    path.lineTo(rc.width() - iRadius, rc.height());
    path.lineTo(iRadius, rc.height());
    path.arcTo(0, rc.height() - iRadius, iRadius, iRadius, -90, -90);
    path.lineTo(0, rc.height() - iRadius);
    path.lineTo(0, 0);
    m_kbLayoutClip->setClipPath(path);
}

/**
 * @brief 检查多因子认证结果
 *
 * @param type
 * @param succeed
 */
void UserLoginWidget::checkAuthResult(const int type, const int status)
{
    if (type == -1 && status == 0) {
        emit authFininshed(status);
    }
}

/**
 * @brief 设置用户名
 *
 * @param name
 */
void UserLoginWidget::updateName(const QString &name)
{
    if (name == m_name || m_nameLabel == nullptr) {
        return;
    }
    m_name = name;
    updateNameLabel(m_nameLabel->font());
}

/**
 * @brief 更新账户登录状态
 *
 * @param loginState
 */
void UserLoginWidget::updateLoginState(const bool loginState)
{
    if (loginState == m_loginState) {
        return;
    }
    m_loginState = loginState;
    m_loginStateLabel->setVisible(loginState);
    updateNameLabel(m_nameLabel->font());
}

/**
 * @brief 更新用户名的字体
 *
 * @param font
 */
void UserLoginWidget::updateNameLabel(const QFont &font)
{
    if (font != m_nameLabel->font()) {
        m_nameLabel->setFont(font);
    }
    int nameWidth = m_nameLabel->fontMetrics().width(m_name);
    int labelMaxWidth = width() - 10 * 2;
    if (m_loginStateLabel->isVisible()) {
        labelMaxWidth -= m_loginStateLabel->width() - m_nameWidget->layout()->spacing();
    }
    if (nameWidth > labelMaxWidth) {
        QString str = m_nameLabel->fontMetrics().elidedText(m_name, Qt::ElideRight, labelMaxWidth);
        m_nameLabel->setText(str);
    } else {
        m_nameLabel->setText(m_name);
    }
}

/**
 * @brief obsolete
 */
void UserLoginWidget::unlockSuccessAni()
{
    m_timerIndex = 0;
    m_lockButton->setIcon(DStyle::SP_LockElement);

    disconnect(m_connection);
    m_connection = connect(m_aniTimer, &QTimer::timeout, [&]() {
        if (m_timerIndex <= 11) {
            m_lockButton->setIcon(QIcon(QString(":/img/unlockTrue/unlock_%1.svg").arg(m_timerIndex)));
        } else {
            m_aniTimer->stop();
            emit unlockActionFinish();
            m_lockButton->setIcon(DStyle::SP_LockElement);
        }

        m_timerIndex++;
    });

    m_aniTimer->start(15);
}

/**
 * @brief obsolete
 */
void UserLoginWidget::unlockFailedAni()
{
    //    m_passwordEdit->lineEdit()->clear();
    //    m_passwordEdit->hideLoadSlider();

    m_timerIndex = 0;
    m_lockButton->setIcon(DStyle::SP_LockElement);

    disconnect(m_connection);
    m_connection = connect(m_aniTimer, &QTimer::timeout, [&]() {
        if (m_timerIndex <= 15) {
            m_lockButton->setIcon(QIcon(QString(":/img/unlockFalse/unlock_error_%1.svg").arg(m_timerIndex)));
        } else {
            m_aniTimer->stop();
        }

        m_timerIndex++;
    });

    m_aniTimer->start(15);
}

/**
 * @brief 用于过滤编辑框的快捷键操作
 *
 * @param watched   编辑框对象
 * @param event     键盘事件
 * @return true     过滤
 * @return false    放行
 */
bool UserLoginWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *key_event = static_cast<QKeyEvent *>(event);
        if (key_event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
            if ((key_event->modifiers() & Qt::ControlModifier) && key_event->key() == Qt::Key_A)
                return false;
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

/**
 * @brief 键盘事件
 *
 * @param event
 */
void UserLoginWidget::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_lockButton->isEnabled()) {
            // emit m_lockButton->clicked();
        }
        break;
    }
    QWidget::keyReleaseEvent(event);
}

void UserLoginWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
}

//窗体resize事件,更新阴影窗体的位置
void UserLoginWidget::resizeEvent(QResizeEvent *event)
{
    updateBlurEffectGeometry();
    //    refreshKBLayoutWidgetPosition();
    QWidget::resizeEvent(event);
}

void UserLoginWidget::mousePressEvent(QMouseEvent *event)
{
    emit clicked();

    QWidget::mousePressEvent(event);
}

/**
 * @brief 选中时，在窗体底端居中，绘制92*4尺寸的圆角矩形，样式数据来源于设计图
 *
 * @param event
 */
void UserLoginWidget::paintEvent(QPaintEvent *event)
{
    if (!m_isSelected) {
        return;
    }
    QPainter painter(this);
    painter.setPen(QColor(255, 255, 255, 76));
    painter.setBrush(QColor(255, 255, 255, 76));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawRoundedRect(QRect(width() / 2 - 46, rect().bottom() - 4, 92, 4), 2, 2);

    QWidget::paintEvent(event);
}
