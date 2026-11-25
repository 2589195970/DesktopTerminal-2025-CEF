// Qt Network 的头文件会取消 Windows COM 使用的 interface 宏。
#include <windows.h>
#include <exdisp.h>
#include <shellapi.h>
#include <shldisp.h>
#include <shlobj.h>
#include <winsvc.h>
#include <wrl/client.h>

#include "monitor_windows.h"
#include "registration_monitor.h"
#include "terminal_core.h"
#include "win_handle.h"
#include "../app_version.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QStyledItemDelegate>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>
#include <QWindow>
#include <QtConcurrentRun>

#include <string_view>

namespace {

using namespace terminal;

constexpr qint64 Minute = 60 * 1000;
constexpr qint64 Day = 24 * 60 * Minute;
constexpr int VisibleLogLimit = 1000;

HRESULT launchTrayFromExplorer()
{
    using Microsoft::WRL::ComPtr;

    ComPtr<IShellWindows> windows;
    HRESULT result = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&windows));
    if (FAILED(result)) {
        return result;
    }

    VARIANT desktop{};
    desktop.vt = VT_I4;
    desktop.lVal = CSIDL_DESKTOP;
    VARIANT root{};
    long windowHandle = 0;
    ComPtr<IDispatch> window;
    result = windows->FindWindowSW(&desktop, &root, SWC_DESKTOP, &windowHandle, SWFO_NEEDDISPATCH, &window);
    if (result != S_OK) {
        return result == S_FALSE ? HRESULT_FROM_WIN32(ERROR_NOT_FOUND) : result;
    }

    ComPtr<IServiceProvider> services;
    ComPtr<IShellBrowser> browser;
    ComPtr<IShellView> view;
    ComPtr<IDispatch> viewAutomation;
    ComPtr<IShellFolderViewDual> folder;
    ComPtr<IDispatch> application;
    ComPtr<IShellDispatch2> shell;
    if (FAILED(result = window.As(&services))
            || FAILED(result = services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser)))
            || FAILED(result = browser->QueryActiveShellView(&view))
            || FAILED(result = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&viewAutomation)))
            || FAILED(result = viewAutomation.As(&folder))
            || FAILED(result = folder->get_Application(&application))
            || FAILED(result = application.As(&shell))) {
        return result;
    }

    wchar_t executable[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (!length || length == MAX_PATH) {
        return HRESULT_FROM_WIN32(length == MAX_PATH ? ERROR_INSUFFICIENT_BUFFER : GetLastError());
    }
    const std::unique_ptr<OLECHAR, decltype(&SysFreeString)> path(SysAllocString(executable), SysFreeString);
    const std::unique_ptr<OLECHAR, decltype(&SysFreeString)> trayArgument(SysAllocString(L"--tray"), SysFreeString);
    if (!path || !trayArgument) {
        return E_OUTOFMEMORY;
    }
    VARIANT arguments{};
    arguments.vt = VT_BSTR;
    arguments.bstrVal = trayArgument.get();
    VARIANT optional{};
    return shell->ShellExecute(path.get(), arguments, optional, optional, optional);
}

int startTrayForDesktopUser()
{
    if (!GetShellWindow()) {
        return ERROR_NOT_LOGGED_ON;
    }
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) {
        return initialized;
    }
    // 让桌面 Explorer 以登录用户的身份启动托盘。
    const HRESULT result = launchTrayFromExplorer();
    CoUninitialize();
    return result;
}

enum class ServiceState { Running, Starting, Stopping, Stopped, Missing, Unknown };

struct ServiceInfo
{
    ServiceState state = ServiceState::Unknown;
    DWORD pid = 0;
    DWORD exitCode = 0;
    DWORD serviceExitCode = 0;
};

ServiceInfo queryService()
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) {
        return {};
    }
    SC_HANDLE service = OpenServiceW(manager, TerminalServiceName, SERVICE_QUERY_STATUS);
    const DWORD openError = service ? ERROR_SUCCESS : GetLastError();
    CloseServiceHandle(manager);
    if (!service) {
        return {openError == ERROR_SERVICE_DOES_NOT_EXIST ? ServiceState::Missing : ServiceState::Unknown};
    }
    SERVICE_STATUS_PROCESS status{};
    DWORD needed = 0;
    const BOOL ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE *>(&status),
                                         sizeof(status), &needed);
    CloseServiceHandle(service);
    if (!ok) {
        return {};
    }
    ServiceState state = ServiceState::Unknown;
    switch (status.dwCurrentState) {
    case SERVICE_RUNNING: state = ServiceState::Running; break;
    case SERVICE_START_PENDING: state = ServiceState::Starting; break;
    case SERVICE_STOP_PENDING: state = ServiceState::Stopping; break;
    case SERVICE_STOPPED: state = ServiceState::Stopped; break;
    }
    return {state, status.dwProcessId, status.dwWin32ExitCode, status.dwServiceSpecificExitCode};
}

QString localTime(qint64 milliseconds, bool full = false)
{
    if (!milliseconds) {
        return QStringLiteral("暂无");
    }
    const auto time = QDateTime::fromMSecsSinceEpoch(milliseconds);
    if (full) {
        return time.toOffsetFromUtc(time.offsetFromUtc()).toString(Qt::ISODate).replace(QLatin1Char('T'), QLatin1Char(' '));
    }
    return time.toString(time.date() == QDate::currentDate() ? QStringLiteral("hh:mm:ss") : QStringLiteral("MM-dd hh:mm:ss"));
}

QString serviceText(const ServiceInfo &service)
{
    switch (service.state) {
    case ServiceState::Running: return QStringLiteral("运行中");
    case ServiceState::Starting: return QStringLiteral("正在启动");
    case ServiceState::Stopping: return QStringLiteral("正在停止");
    case ServiceState::Missing: return QStringLiteral("未安装");
    case ServiceState::Unknown: return QStringLiteral("无法读取");
    case ServiceState::Stopped:
        if (service.serviceExitCode) {
            return QStringLiteral("已停止（退出码 %1，服务错误 %2）").arg(service.exitCode).arg(service.serviceExitCode);
        }
        return service.exitCode ? QStringLiteral("已停止（退出码 %1）").arg(service.exitCode) : QStringLiteral("已停止");
    }
    return {};
}

struct Presentation
{
    QString title;
    QString description;
    QString nextStep;
    QColor color;
    QColor surface;
};

Presentation presentation(const ServiceInfo &service, const RegistrationStatus &status, qint64 now)
{
    const QColor green(QStringLiteral("#176452"));
    const QColor greenSurface(QStringLiteral("#ecf5f1"));
    const QColor amber(QStringLiteral("#89520f"));
    const QColor amberSurface(QStringLiteral("#fbf4e7"));
    const QColor red(QStringLiteral("#a1352d"));
    const QColor redSurface(QStringLiteral("#fcf0ec"));
    const QColor slate(QStringLiteral("#46545e"));
    const QColor slateSurface(QStringLiteral("#f1f4f6"));
    const QString admin = QStringLiteral("请管理员运行安装目录中的 diagnose-terminal-service.cmd。");
    switch (service.state) {
    case ServiceState::Starting:
        return {QStringLiteral("服务正在启动"), QStringLiteral("正在初始化电脑注册服务。"), QStringLiteral("稍候查看注册进度。"), amber, amberSurface};
    case ServiceState::Stopping:
        return {QStringLiteral("服务正在停止"), serviceText(service), admin, slate, slateSurface};
    case ServiceState::Stopped:
    case ServiceState::Missing: {
        QString reason = serviceText(service);
        if (service.state == ServiceState::Stopped && status.phase == RegistrationPhase::StartupFailed && status.recentFailure) {
            reason += QStringLiteral("。") + registrationProblemText(status.recentFailure->problem);
        }
        return {QStringLiteral("服务未启动"), reason, admin, slate, slateSurface};
    }
    case ServiceState::Unknown:
        return {QStringLiteral("状态不可用"), QStringLiteral("无法查询 Windows 服务状态。"), admin, slate, slateSurface};
    case ServiceState::Running:
        break;
    }
    if (!status.isCurrent(service.pid, now)) {
        return {QStringLiteral("状态不可用"), QStringLiteral("服务正在运行，等待当前进程发布状态。"), admin, slate, slateSurface};
    }
    switch (status.phase) {
    case RegistrationPhase::Starting:
    case RegistrationPhase::Registering:
        return {QStringLiteral("正在注册"), QStringLiteral("终端正在向学校服务器注册。"), QStringLiteral("等待本次注册结果。"), amber, amberSurface};
    case RegistrationPhase::WaitingForBinding:
        return {QStringLiteral("待绑定"), QStringLiteral("学校服务器已收到注册，等待分配座位。"),
                QStringLiteral("打开「考场维护 → 绑定终端」，按电脑名查找。"), amber, amberSurface};
    case RegistrationPhase::WaitingForHeartbeat:
        return {QStringLiteral("已绑定，等待心跳"), QStringLiteral("已取得绑定，正在等待首次有效心跳。"),
                QStringLiteral("查看首次心跳是否成功。"), amber, amberSurface};
    case RegistrationPhase::Online:
        if (status.heartbeatIsCurrent(now)) {
            return {QStringLiteral("在线"), QStringLiteral("学校服务器已确认有效心跳。"),
                    QStringLiteral("可在考场维护中核对这台电脑的座位。"), green, greenSurface};
        }
        return {QStringLiteral("状态不可用"), QStringLiteral("最近的有效心跳已过期。"), admin, slate, slateSurface};
    case RegistrationPhase::RegistrationFailed:
    case RegistrationPhase::HeartbeatFailed: {
        const auto problem = status.recentFailure ? status.recentFailure->problem : RegistrationProblem::None;
        const int http = status.recentFailure ? status.recentFailure->httpStatus : 0;
        const QString advice = problem == RegistrationProblem::State ? admin
                : QStringLiteral("请检查本机网络和学校服务器地址。");
        return {status.phase == RegistrationPhase::RegistrationFailed ? QStringLiteral("注册失败") : QStringLiteral("心跳失败"),
                registrationProblemText(problem, http), advice, red, redSurface};
    }
    case RegistrationPhase::StartupFailed:
    case RegistrationPhase::Stopped:
        return {QStringLiteral("状态不可用"), QStringLiteral("等待 Windows 服务状态更新。"), admin, slate, slateSurface};
    }
    return {};
}

QString eventSummary(const RegistrationLogEntry &entry)
{
    if (entry.event == RegistrationEvent::RegistrationRecovered || entry.event == RegistrationEvent::HeartbeatRecovered) {
        const auto duration = (entry.lastAt - entry.firstAt) / 1000;
        const QString elapsed = duration < 60 ? QStringLiteral("%1 秒").arg(duration)
                : QStringLiteral("%1 分 %2 秒").arg(duration / 60).arg(duration % 60);
        return QStringLiteral("持续 %1，期间失败 %2 次").arg(elapsed).arg(entry.count);
    }
    if (entry.problem != RegistrationProblem::None) {
        return registrationProblemText(entry.problem, entry.httpStatus);
    }
    switch (entry.event) {
    case RegistrationEvent::Started: return QStringLiteral("Windows 服务已进入启动流程");
    case RegistrationEvent::Stopped: return QStringLiteral("Windows 服务已停止");
    case RegistrationEvent::WaitingForBinding: return QStringLiteral("学校服务器已收到注册");
    case RegistrationEvent::Bound: return QStringLiteral("已保存绑定，等待有效心跳");
    case RegistrationEvent::FirstHeartbeat: return QStringLiteral("学校服务器已确认心跳");
    case RegistrationEvent::CodeInvalid: return QStringLiteral("学校服务器返回绑定无效，将重新注册");
    default: return {};
    }
}

QString failureDetail(const RegistrationLogEntry &entry, bool full = false)
{
    return QStringLiteral("首次 %1　最近 %2　共 %3 次")
            .arg(localTime(entry.firstAt, full), localTime(entry.lastAt, full)).arg(entry.count);
}

QIcon statusIcon(const QColor &color)
{
    QIcon icon;
    for (const int size : {16, 24, 32, 48}) {
        QPixmap pixels(size, size);
        pixels.fill(Qt::transparent);
        QPainter painter(&pixels);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::white, size / 12.0));
        painter.setBrush(color);
        const qreal inset = size / 8.0;
        painter.drawEllipse(QRectF(inset, inset, size - 2 * inset, size - 2 * inset));
        painter.end();
        icon.addPixmap(pixels);
    }
    return icon;
}

QLabel *plainLabel(const QString &text = {})
{
    auto *label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QString displayValue(const QString &value)
{
    return value.isEmpty() ? QStringLiteral("暂无") : value;
}

const QString &panelStyle()
{
    static const QString style = QStringLiteral(R"qss(
        QLabel { color: #17282b; }
        QLabel[muted="true"] { color: #53676a; }
        QLabel[strong="true"] { font-weight: 600; }
        QFrame#panel { background: #ffffff; border: 1px solid #a1b6b5; border-radius: 13px; }
        QFrame#titleBar {
            background: #f7faf9; border-bottom: 1px solid #dce6e5;
            border-top-left-radius: 12px; border-top-right-radius: 12px;
        }
        QLabel#panelTitle { font-size: 15px; font-weight: 700; }
        QFrame#footer {
            background: #f7faf9; border-top: 1px solid #dce6e5;
            border-bottom-left-radius: 12px; border-bottom-right-radius: 12px;
        }
        QFrame#divider { background: #ccdad4; border: 0; }
        QFrame#overviewRule { background: #dce6e5; border: 0; }
        QPushButton, QComboBox {
            min-height: 30px; border: 1px solid #cbd9d7; border-radius: 6px;
            background: #ffffff; color: #17282b; padding: 0 10px;
        }
        QPushButton:hover, QComboBox:hover { background: #edf4f1; }
        QPushButton:focus, QComboBox:focus { border-color: #247c70; }
        QPushButton:disabled { color: #687b7d; background: #f5f8f7; }
        QPushButton#closePanel { background: transparent; border-color: transparent; color: #53676a; }
        QPushButton#closePanel:hover { background: #e9f0ed; }
        QPushButton#closePanel:focus { border-color: #247c70; }
        QPushButton[primary="true"] { color: #ffffff; background: #195e53; border-color: #195e53; }
        QPushButton[primary="true"]:hover { background: #114c43; border-color: #114c43; }
        QPushButton[primary="true"]:focus { border-color: #89c9ab; }
        QDialog#serverSettings { background: #ffffff; }
        QLineEdit {
            min-height: 30px; padding: 0 8px; border: 1px solid #cbd9d7;
            border-radius: 6px; background: #ffffff; color: #17282b;
        }
        QLineEdit:focus { border-color: #247c70; }
        QLabel#settingsError { color: #a1352d; }
        QComboBox { padding-right: 26px; min-width: 84px; }
        QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 24px; border: 0; }
        QComboBox::down-arrow { width: 0; height: 0; }
        QComboBox QAbstractItemView {
            background: #ffffff; color: #17282b; border: 1px solid #cbd9d7;
            selection-background-color: #e6f2ec; selection-color: #17282b;
        }
        QCheckBox { color: #53676a; spacing: 7px; min-height: 30px; }
        QTabWidget::pane { background: #ffffff; border: 0; border-top: 1px solid #dce6e5; top: -1px; }
        QTabBar::tab {
            border: 0; border-bottom: 2px solid transparent; color: #53676a;
            background: #ffffff; min-height: 34px; padding: 0 2px; margin-right: 20px;
        }
        QTabBar::tab:hover { color: #176452; }
        QTabBar::tab:selected { border-bottom-color: #176452; color: #176452; font-weight: 700; }
        QTreeWidget#registrationLogs {
            border: 1px solid #dce6e5; border-radius: 8px; background: #ffffff;
            outline: 0; selection-background-color: #e6f2ec; selection-color: #17282b;
        }
        QTreeWidget#registrationLogs:focus { border-color: #8eb3a9; }
        QTreeWidget#registrationLogs::item {
            min-height: 33px; padding: 0 10px; border-bottom: 1px solid #e8eeee;
            background: #ffffff;
        }
        QTreeWidget#registrationLogs::item:hover { background: #f2f7f5; }
        QTreeWidget#registrationLogs::item:selected { background: #e6f2ec; color: #17282b; }
        QTreeWidget QScrollBar:vertical { background: #f5f8f7; width: 8px; margin: 3px 2px; }
        QTreeWidget QScrollBar::handle:vertical { background: #cadbd7; min-height: 25px; border-radius: 4px; }
        QTreeWidget QScrollBar::handle:vertical:hover { background: #a8c5be; }
        QTreeWidget QScrollBar::add-line:vertical, QTreeWidget QScrollBar::sub-line:vertical { height: 0; }
        QTreeWidget QScrollBar::add-page:vertical, QTreeWidget QScrollBar::sub-page:vertical { background: transparent; }
        QHeaderView { background: #f5f8f7; }
        QHeaderView::section {
            background: #f5f8f7; color: #53676a; border: 0; border-bottom: 1px solid #dce6e5;
            min-height: 31px; padding: 0 10px; font-weight: 600;
        }
    )qss");
    return style;
}

bool isAdministrator()
{
    alignas(DWORD) BYTE sid[SECURITY_MAX_SID_SIZE];
    DWORD sidSize = sizeof(sid);
    BOOL member = FALSE;
    return CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sid, &sidSize)
            && CheckTokenMembership(nullptr, sid, &member) && member;
}

void launchServerSettings(QWidget *parent)
{
    const auto executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
    const auto directory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
    SHELLEXECUTEINFOW request{};
    request.cbSize = sizeof(request);
    request.fMask = SEE_MASK_FLAG_NO_UI;
    request.hwnd = parent ? reinterpret_cast<HWND>(parent->winId()) : nullptr;
    request.lpVerb = L"runas";
    request.lpFile = executable.c_str();
    request.lpParameters = L"--configure-service";
    request.lpDirectory = directory.c_str();
    request.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&request) && GetLastError() != ERROR_CANCELLED) {
        QMessageBox::warning(parent, QStringLiteral("无法打开配置"),
                             QStringLiteral("请使用 Windows 管理员账户打开终端服务配置。"));
    }
}

bool isRegularConfigFile(const QString &path)
{
    const auto nativePath = QDir::toNativeSeparators(path).toStdWString();
    const WinHandle file(CreateFileW(nativePath.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    return file && GetFileInformationByHandle(file.get(), &info) && info.nNumberOfLinks == 1
            && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

int showServerSettings()
{
    const QString title = QStringLiteral("终端服务配置");
    if (!isAdministrator()) {
        QMessageBox::warning(nullptr, title, QStringLiteral("请在托盘右键菜单中选择「配置参数」，并授权管理员权限。"));
        return 1;
    }

    QString dataDirectory;
    const WinHandle directory = openProtectedServiceDataDirectory(dataDirectory);
    const QString path = QDir(dataDirectory).filePath(QStringLiteral("service.ini"));
    if (!directory || !isRegularConfigFile(path)) {
        QMessageBox::warning(nullptr, title, QStringLiteral("无法访问终端服务配置，请检查安装和数据目录权限。"));
        return 1;
    }
    QLockFile lock(path + QStringLiteral(".edit.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        QMessageBox::warning(nullptr, title, lock.error() == QLockFile::LockFailedError
            ? QStringLiteral("配置窗口已打开，请在已打开的窗口中修改。")
            : QStringLiteral("无法锁定配置文件，请检查数据目录权限。"));
        return 1;
    }

    QString serverUrl;
    QString schoolInstance;
    QString error;
    if (!Config::loadServerSettings(path, serverUrl, schoolInstance, error)) {
        QMessageBox::warning(nullptr, title, QStringLiteral("无法读取 service.ini（%1）。").arg(error));
        return 1;
    }

    QDialog dialog;
    dialog.setWindowTitle(title);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setObjectName(QStringLiteral("serverSettings"));
    dialog.setStyleSheet(panelStyle());
    auto palette = dialog.palette();
    palette.setColor(QPalette::HighlightedText, Qt::white);
    dialog.setPalette(palette);
    dialog.setMinimumWidth(460);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    auto *url = new QLineEdit(serverUrl);
    auto *school = new QLineEdit(schoolInstance);
    url->setPlaceholderText(QStringLiteral("https://exam.example.org/um"));
    form->addRow(QStringLiteral("终端管理地址"), url);
    form->addRow(QStringLiteral("学校实例标识"), school);
    layout->addLayout(form);

    auto *hint = plainLabel(QStringLiteral("配置在终端服务下次启动时生效。"));
    hint->setProperty("muted", true);
    layout->addWidget(hint);
    auto *feedback = plainLabel();
    feedback->setObjectName(QStringLiteral("settingsError"));
    feedback->setWordWrap(true);
    feedback->hide();
    layout->addWidget(feedback);

    auto *buttons = new QDialogButtonBox;
    auto *save = buttons->addButton(QStringLiteral("保存"), QDialogButtonBox::AcceptRole);
    save->setProperty("primary", true);
    save->setDefault(true);
    buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QString saveError;
        if (!isRegularConfigFile(path)) {
            saveError = QStringLiteral("配置文件不可用，请检查安装和数据目录权限。");
        } else if (Config::saveServerSettings(path, url->text(), school->text(), saveError)) {
            QMessageBox::information(&dialog, title,
                QStringLiteral("配置已保存。需要立即生效时，请以管理员身份运行安装目录中的 restart-terminal-service.bat。"));
            dialog.accept();
            return;
        }
        feedback->setText(saveError);
        feedback->show();
    });
    url->setFocus();
    dialog.exec();
    return 0;
}

class PanelTitleBar : public QFrame
{
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            if (auto *handle = window()->windowHandle(); handle && handle->startSystemMove()) {
                return;
            }
            m_dragOffset = event->globalPos() - window()->frameGeometry().topLeft();
            return;
        }
        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragOffset && event->buttons().testFlag(Qt::LeftButton)) {
            window()->move(event->globalPos() - *m_dragOffset);
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_dragOffset.reset();
        QFrame::mouseReleaseEvent(event);
    }

    std::optional<QPoint> m_dragOffset;
};

void drawChevron(QPainter &painter, const QPoint &center, bool expanded)
{
    const int direction = expanded ? -1 : 1;
    const QPoint points[]{center + QPoint(-3, -direction), center + QPoint(0, 2 * direction),
                          center + QPoint(3, -direction)};
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(QStringLiteral("#53676a")));
    painter.drawPolyline(points, 3);
    painter.restore();
}

class LogRangeBox : public QComboBox
{
    void paintEvent(QPaintEvent *event) override
    {
        QComboBox::paintEvent(event);
        QPainter painter(this);
        drawChevron(painter, QPoint(width() - 13, height() / 2), view()->isVisible());
    }
};

class LogRowDelegate : public QStyledItemDelegate
{
public:
    explicit LogRowDelegate(QTreeWidget *tree) : QStyledItemDelegate(tree), m_tree(tree) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        const auto firstColumn = index.sibling(index.row(), 0);
        if (index.column() != 2 || !index.model()->hasChildren(firstColumn)) {
            return;
        }
        const int textEnd = option.rect.left() + 17 + option.fontMetrics.horizontalAdvance(index.data().toString());
        const QPoint center(qMin(textEnd, option.rect.right() - 12), option.rect.center().y());
        drawChevron(*painter, center, m_tree->isExpanded(firstColumn));
    }

private:
    QTreeWidget *m_tree;
};

class StatusWindow : public QDialog
{
public:
    explicit StatusWindow(const QString &directory)
        : m_directory(directory), m_tray(this)
    {
        buildUi();
        auto *menu = new QMenu(this);
        menu->addAction(QStringLiteral("查看状态"), this, [this] { showPanel(0); });
        menu->addAction(QStringLiteral("查看运行日志"), this, [this] { showPanel(1); });
        menu->addSeparator();
        menu->addAction(QStringLiteral("配置参数"), this, [] { launchServerSettings(QApplication::activeWindow()); });
        m_tray.setContextMenu(menu);
        connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                showPanel();
            }
        });
        connect(&m_refresh, &QTimer::timeout, this, [this] { refreshStatus(); });
        connect(&m_logRefresh, &QTimer::timeout, this, [this] { refreshLogs(); });
        connect(&m_logReader, &QFutureWatcher<QList<RegistrationLogEntry>>::finished, this, [this] {
            m_events = m_logReader.result();
            m_logsLoaded = true;
            renderLogs();
        });
        refreshStatus();
        m_tray.show();
        m_refresh.start(2000);
        m_logRefresh.start(6000);
    }

    void showPanel(int tab = -1)
    {
        if (tab >= 0) {
            m_tabs->setCurrentIndex(tab);
        }
        if (!isVisible()) {
            const auto *screen = QGuiApplication::screenAt(QCursor::pos());
            if (screen) {
                const QRect area = screen->availableGeometry();
                move(qMax(area.left(), area.right() - width() - 16),
                     qMax(area.top(), area.bottom() - height() - 36));
            }
        }
        showNormal();
        raise();
        activateWindow();
        refreshStatus();
        refreshLogs();
    }

private:
    void buildUi()
    {
        setWindowTitle(QStringLiteral("终端注册状态"));
        setWindowFlag(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        resize(750, 552);
        setMinimumSize(540, 400);
        setStyleSheet(panelStyle());

        auto *windowLayout = new QVBoxLayout(this);
        windowLayout->setContentsMargins(0, 0, 0, 0);
        auto *panel = new QFrame;
        panel->setObjectName(QStringLiteral("panel"));
        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(1, 1, 1, 1);
        panelLayout->setSpacing(0);
        windowLayout->addWidget(panel);

        auto *titleBar = new PanelTitleBar;
        titleBar->setObjectName(QStringLiteral("titleBar"));
        auto *header = new QHBoxLayout(titleBar);
        header->setContentsMargins(17, 8, 12, 8);
        titleBar->setMinimumHeight(51);
        auto *title = plainLabel(QStringLiteral("终端注册状态"));
        title->setObjectName(QStringLiteral("panelTitle"));
        auto *close = new QPushButton(QStringLiteral("关闭"));
        close->setObjectName(QStringLiteral("closePanel"));
        close->setAutoDefault(false);
        header->addWidget(title, 1);
        header->addWidget(close);
        panelLayout->addWidget(titleBar);

        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(18, 15, 18, 0);
        layout->setSpacing(9);
        panelLayout->addWidget(content, 1);

        m_banner = new QFrame;
        m_banner->setObjectName(QStringLiteral("statusBanner"));
        auto *banner = new QHBoxLayout(m_banner);
        banner->setContentsMargins(15, 11, 14, 11);
        banner->setSpacing(13);
        auto *headline = new QVBoxLayout;
        headline->setSpacing(3);
        auto *summary = new QHBoxLayout;
        summary->setSpacing(9);
        m_statusDot = new QFrame;
        m_statusDot->setFixedSize(9, 9);
        m_title = plainLabel();
        QFont heading = m_title->font();
        heading.setPixelSize(23);
        heading.setBold(true);
        m_title->setFont(heading);
        summary->addWidget(m_statusDot, 0, Qt::AlignVCenter);
        summary->addWidget(m_title, 1);
        m_description = plainLabel();
        m_description->setProperty("muted", true);
        m_description->setWordWrap(true);
        headline->addLayout(summary);
        headline->addWidget(m_description);
        banner->addLayout(headline, 1);
        auto *divider = new QFrame;
        divider->setObjectName(QStringLiteral("divider"));
        divider->setFixedSize(1, 38);
        banner->addWidget(divider);
        auto *last = new QWidget;
        last->setMinimumWidth(110);
        auto *lastLayout = new QVBoxLayout(last);
        lastLayout->setContentsMargins(0, 0, 0, 0);
        lastLayout->setSpacing(2);
        m_lastCaption = plainLabel();
        m_lastCaption->setProperty("muted", true);
        m_lastTime = plainLabel();
        m_lastTime->setProperty("strong", true);
        lastLayout->addWidget(m_lastCaption);
        lastLayout->addWidget(m_lastTime);
        banner->addWidget(last);
        layout->addWidget(m_banner);

        auto *identity = new QGridLayout;
        identity->setVerticalSpacing(2);
        identity->setContentsMargins(0, 2, 0, 4);
        m_computer = addField(identity, 0, 0, QStringLiteral("电脑名"));
        m_ip = addField(identity, 0, 1, QStringLiteral("本机出站 IP"));
        m_school = addField(identity, 0, 2, QStringLiteral("学校实例"));
        layout->addLayout(identity);

        m_tabs = new QTabWidget;
        auto *overview = new QWidget;
        auto *fields = new QGridLayout(overview);
        fields->setContentsMargins(0, 5, 0, 12);
        fields->setVerticalSpacing(8);
        fields->setHorizontalSpacing(12);
        fields->setColumnMinimumWidth(0, 102);
        fields->setColumnStretch(1, 1);
        m_serviceText = addOverview(fields, 0, QStringLiteral("Windows 服务"));
        m_seat = addOverview(fields, 1, QStringLiteral("座位号"));
        m_attempt = addOverview(fields, 2, QStringLiteral("下次操作"));
        m_advice = addOverview(fields, 3, QStringLiteral("现场排查"));
        m_version = addOverview(fields, 4, QStringLiteral("终端版本"));
        fields->setRowStretch(9, 1);
        m_tabs->addTab(overview, QStringLiteral("状态概览"));

        auto *logs = new QWidget;
        auto *logLayout = new QVBoxLayout(logs);
        logLayout->setContentsMargins(0, 11, 0, 12);
        logLayout->setSpacing(8);
        auto *filter = new QHBoxLayout;
        filter->setSpacing(12);
        m_range = new LogRangeBox;
        m_range->setAccessibleName(QStringLiteral("日志时间范围"));
        m_range->addItem(QStringLiteral("最近 1 小时"), 60 * Minute);
        m_range->addItem(QStringLiteral("最近 24 小时"), Day);
        m_range->addItem(QStringLiteral("最近 7 天"), 7 * Day);
        m_range->setCurrentIndex(1);
        m_onlyErrors = new QCheckBox(QStringLiteral("只看异常"));
        m_export = new QPushButton(QStringLiteral("导出日志"));
        m_export->setAutoDefault(false);
        m_export->setEnabled(false);
        filter->addWidget(m_range);
        filter->addWidget(m_onlyErrors);
        filter->addStretch();
        filter->addWidget(m_export);
        logLayout->addLayout(filter);
        m_tree = new QTreeWidget;
        m_tree->setObjectName(QStringLiteral("registrationLogs"));
        m_tree->setAccessibleName(QStringLiteral("运行日志"));
        m_tree->setHeaderLabels({QStringLiteral("时间"), QStringLiteral("事件"), QStringLiteral("说明")});
        m_tree->setRootIsDecorated(false);
        m_tree->setIndentation(0);
        m_tree->setExpandsOnDoubleClick(false);
        m_tree->setMouseTracking(true);
        m_tree->setAllColumnsShowFocus(true);
        m_tree->setItemDelegate(new LogRowDelegate(m_tree));
        m_tree->setColumnWidth(0, 99);
        m_tree->setColumnWidth(1, 118);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Fixed);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
        m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
        logLayout->addWidget(m_tree, 1);
        m_tabs->addTab(logs, QStringLiteral("运行日志"));
        m_tabs->setCurrentIndex(1);
        layout->addWidget(m_tabs, 1);

        auto *footer = new QFrame;
        footer->setObjectName(QStringLiteral("footer"));
        auto *actions = new QHBoxLayout(footer);
        actions->setContentsMargins(18, 8, 18, 8);
        actions->setSpacing(10);
        m_logHint = plainLabel(QStringLiteral("正在读取日志…"));
        m_logHint->setProperty("muted", true);
        m_copyHint = plainLabel();
        m_copyHint->setProperty("muted", true);
        auto *copy = new QPushButton(QStringLiteral("复制诊断信息"));
        copy->setProperty("primary", true);
        copy->setAutoDefault(false);
        actions->addWidget(m_logHint);
        actions->addWidget(m_copyHint);
        actions->addStretch();
        actions->addWidget(copy);
        panelLayout->addWidget(footer);

        connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
            m_logHint->setVisible(index == 1);
            refreshLogs();
        });
        connect(m_tree, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem *item) {
            if (item->childCount() > 0) {
                item->setExpanded(!item->isExpanded());
            }
        });
        connect(m_range, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { renderLogs(); });
        connect(m_onlyErrors, &QCheckBox::toggled, this, [this] { renderLogs(); });
        connect(m_export, &QPushButton::clicked, this, [this] { exportLogs(); });
        connect(close, &QPushButton::clicked, this, &QWidget::hide);
        connect(copy, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(diagnosticHeader());
            m_copyHint->setText(QStringLiteral("诊断信息已复制"));
        });
    }

    static QLabel *addField(QGridLayout *layout, int row, int column, const QString &name)
    {
        auto *label = plainLabel(name);
        label->setProperty("muted", true);
        auto *value = plainLabel();
        value->setProperty("strong", true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(label, row, column);
        layout->addWidget(value, row + 1, column);
        layout->setColumnStretch(column, 1);
        return value;
    }

    static QLabel *addOverview(QGridLayout *layout, int row, const QString &name)
    {
        auto *label = plainLabel(name);
        label->setProperty("muted", true);
        auto *value = plainLabel();
        value->setProperty("strong", true);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (row > 0) {
            auto *rule = new QFrame;
            rule->setObjectName(QStringLiteral("overviewRule"));
            rule->setFixedHeight(1);
            layout->addWidget(rule, row * 2 - 1, 0, 1, 2);
        }
        layout->addWidget(label, row * 2, 0, Qt::AlignTop);
        layout->addWidget(value, row * 2, 1);
        return value;
    }

    void refreshStatus()
    {
        m_service = queryService();
        m_status = registrationStatusDirectoryIsTrusted(m_directory)
                ? RegistrationMonitor::readStatus(m_directory).value_or(RegistrationStatus{}) : RegistrationStatus{};
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool current = m_service.state == ServiceState::Running && m_status.isCurrent(m_service.pid, now);
        const bool failed = current && (m_status.phase == RegistrationPhase::RegistrationFailed
                                        || m_status.phase == RegistrationPhase::HeartbeatFailed);
        const auto view = presentation(m_service, m_status, now);
        if (m_title->text() != view.title) {
            const auto icon = statusIcon(view.color);
            m_tray.setIcon(icon);
            setWindowIcon(icon);
            m_tray.setToolTip(QStringLiteral("终端注册 · %1").arg(view.title));
            m_banner->setStyleSheet(QStringLiteral(
                "QFrame#statusBanner { background: %1; border: 1px solid %2; border-radius: 9px; }")
                .arg(view.surface.name(), view.surface.darker(105).name()));
            m_statusDot->setStyleSheet(QStringLiteral("background: %1; border: 0; border-radius: 4px;").arg(view.color.name()));
            m_title->setText(view.title);
            m_copyHint->clear();
        }
        QString next = QStringLiteral("暂无");
        if (current && m_status.nextAttempt > now) {
            next = QStringLiteral("%1 %2").arg(localTime(m_status.nextAttempt),
                failed ? QStringLiteral("自动重试") : QStringLiteral("自动检查"));
        } else if (failed) {
            next = QStringLiteral("正在重试");
        }
        m_description->setText(failed ? view.description + QStringLiteral("。") + next : view.description);
        const bool heartbeat = m_status.lastHeartbeat >= m_status.lastRegistration && m_status.lastHeartbeat > 0;
        m_lastCaption->setText(heartbeat ? QStringLiteral("上次成功心跳") : QStringLiteral("上次注册成功"));
        m_lastTime->setText(localTime(heartbeat ? m_status.lastHeartbeat : m_status.lastRegistration));
        m_computer->setText(m_status.computerName.isEmpty() ? QSysInfo::machineHostName() : m_status.computerName);
        m_ip->setText(displayValue(m_status.localIp));
        m_school->setText(m_status.schoolInstance > 0 ? QString::number(m_status.schoolInstance) : QStringLiteral("暂无"));
        m_serviceText->setText(serviceText(m_service));
        m_seat->setText(m_status.seat.isEmpty() ? QStringLiteral("暂无")
                   : current ? m_status.seat : QStringLiteral("%1（上次记录）").arg(m_status.seat));
        m_version->setText(displayValue(m_status.version));
        m_advice->setText(view.nextStep);
        m_attempt->setText(next);
    }

    void refreshLogs()
    {
        if (!isVisible() || m_tabs->currentIndex() != 1 || m_logReader.isRunning()) {
            return;
        }
        m_logReader.setFuture(QtConcurrent::run([directory = m_directory] {
            return registrationStatusDirectoryIsTrusted(directory)
                    ? RegistrationMonitor::readLogs(directory, QDateTime::currentMSecsSinceEpoch() - 7 * Day)
                    : QList<RegistrationLogEntry>{};
        }));
    }

    QList<RegistrationLogEntry> filteredLogs() const
    {
        const auto since = QDateTime::currentMSecsSinceEpoch() - m_range->currentData().toLongLong();
        QList<RegistrationLogEntry> entries;
        for (const auto &entry : m_events) {
            if (entry.lastAt >= since && (!m_onlyErrors->isChecked() || entry.isFailure())) {
                entries.append(entry);
            }
        }
        return entries;
    }

    void renderLogs()
    {
        const auto entries = filteredLogs();
        const int scroll = m_tree->verticalScrollBar()->value();
        QSet<QString> expanded;
        for (int index = 0; index < m_tree->topLevelItemCount(); ++index) {
            const auto *item = m_tree->topLevelItem(index);
            if (item->isExpanded()) {
                expanded.insert(item->data(0, Qt::UserRole).toString());
            }
        }
        m_tree->setUpdatesEnabled(false);
        m_tree->clear();
        const int shown = qMin(entries.size(), VisibleLogLimit);
        const QColor muted(QStringLiteral("#53676a"));
        const QColor success(QStringLiteral("#176452"));
        const QColor warning(QStringLiteral("#89520f"));
        const QColor failure(QStringLiteral("#a1352d"));
        QFont eventFont = m_tree->font();
        eventFont.setWeight(QFont::DemiBold);
        for (int index = 0; index < shown; ++index) {
            const auto &entry = entries.at(index);
            const QString summary = eventSummary(entry);
            auto *item = new QTreeWidgetItem(m_tree, {localTime(entry.lastAt), registrationEventText(entry.event), summary});
            item->setData(0, Qt::UserRole, entry.id);
            item->setToolTip(2, summary);
            item->setForeground(0, muted);
            item->setForeground(2, muted);
            item->setFont(1, eventFont);
            if (entry.isFailure()) {
                item->setForeground(1, failure);
                const QString times = failureDetail(entry);
                auto *detail = new QTreeWidgetItem(item, {times});
                detail->setFirstColumnSpanned(true);
                detail->setToolTip(0, times);
                detail->setForeground(0, muted);
                item->setExpanded(expanded.contains(entry.id));
            } else if (entry.event == RegistrationEvent::WaitingForBinding) {
                item->setForeground(1, warning);
            } else if (entry.event == RegistrationEvent::Bound || entry.event == RegistrationEvent::FirstHeartbeat
                       || entry.event == RegistrationEvent::RegistrationRecovered || entry.event == RegistrationEvent::HeartbeatRecovered) {
                item->setForeground(1, success);
            }
        }
        m_tree->verticalScrollBar()->setValue(scroll);
        m_tree->setUpdatesEnabled(true);
        m_export->setEnabled(!entries.isEmpty());
        const QString count = shown < entries.size() ? QStringLiteral("显示最近 %1 条，导出包含全部 %2 条").arg(shown).arg(entries.size())
                : QStringLiteral("显示 %1 条记录").arg(shown);
        m_logHint->setText(!m_logsLoaded ? QStringLiteral("正在读取日志…")
                           : shown == 0 ? QStringLiteral("所选范围内暂无符合条件的日志。") : count);
    }

    QString diagnosticHeader() const
    {
        QStringList lines{QStringLiteral("电脑名 %1").arg(m_computer->text()),
                          QStringLiteral("本机出站 IP %1").arg(m_ip->text()),
                          QStringLiteral("学校实例 %1").arg(m_school->text()),
                          QStringLiteral("终端版本 %1").arg(m_version->text()),
                          QStringLiteral("注册状态 %1").arg(m_title->text()),
                          QStringLiteral("Windows 服务 %1").arg(m_serviceText->text()),
                          QStringLiteral("上次注册成功 %1").arg(localTime(m_status.lastRegistration, true)),
                          QStringLiteral("上次成功心跳 %1").arg(localTime(m_status.lastHeartbeat, true))};
        if (m_status.recentFailure) {
            const auto &failure = *m_status.recentFailure;
            lines.append(QStringLiteral("最近异常 %1 %2　%3").arg(registrationEventText(failure.event),
                eventSummary(failure), failureDetail(failure, true)));
        }
        if (m_status.nextAttempt > QDateTime::currentMSecsSinceEpoch() && m_status.isCurrent(m_service.pid, QDateTime::currentMSecsSinceEpoch())) {
            lines.append(QStringLiteral("下次尝试 %1").arg(localTime(m_status.nextAttempt, true)));
        }
        return lines.join(QLatin1Char('\n'));
    }

    void exportLogs()
    {
        const auto entries = filteredLogs();
        if (entries.isEmpty()) {
            return;
        }
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出终端注册日志"),
            QStringLiteral("终端注册日志-%1.txt").arg(QDate::currentDate().toString(Qt::ISODate)), QStringLiteral("文本文件 (*.txt)"));
        if (path.isEmpty()) {
            return;
        }
        QString text = diagnosticHeader() + QStringLiteral("\n\n");
        for (const auto &entry : entries) {
            text += QStringLiteral("%1　%2　%3").arg(localTime(entry.lastAt, true), registrationEventText(entry.event), eventSummary(entry));
            if (entry.isFailure()) {
                text += QStringLiteral("　") + failureDetail(entry, true);
            }
            text += QLatin1Char('\n');
        }
        const QByteArray bytes = QByteArray::fromHex("efbbbf") + text.toUtf8();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法保存日志。请检查目录权限或选择其他位置。"));
            return;
        }
        m_copyHint->setText(QStringLiteral("日志已导出"));
    }

    QString m_directory;
    ServiceInfo m_service;
    RegistrationStatus m_status;
    QList<RegistrationLogEntry> m_events;
    QSystemTrayIcon m_tray;
    QTimer m_refresh;
    QTimer m_logRefresh;
    QFutureWatcher<QList<RegistrationLogEntry>> m_logReader;
    QFrame *m_banner = nullptr;
    QFrame *m_statusDot = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_description = nullptr;
    QLabel *m_lastCaption = nullptr;
    QLabel *m_lastTime = nullptr;
    QLabel *m_computer = nullptr;
    QLabel *m_ip = nullptr;
    QLabel *m_school = nullptr;
    QLabel *m_serviceText = nullptr;
    QLabel *m_seat = nullptr;
    QLabel *m_attempt = nullptr;
    QLabel *m_advice = nullptr;
    QLabel *m_version = nullptr;
    QLabel *m_logHint = nullptr;
    QLabel *m_copyHint = nullptr;
    QTabWidget *m_tabs = nullptr;
    QComboBox *m_range = nullptr;
    QCheckBox *m_onlyErrors = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_export = nullptr;
    bool m_logsLoaded = false;
};

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--start-for-desktop-user") {
        return startTrayForDesktopUser();
    }

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DesktopTerminalStatus"));
    app.setApplicationVersion(QStringLiteral(DESKTOP_TERMINAL_VERSION));
    app.setStyle(QStringLiteral("Fusion"));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPixelSize(13);
    app.setFont(font);
    auto palette = app.palette();
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#195e53")));
    app.setPalette(palette);
    app.setQuitOnLastWindowClosed(false);
    if (argc == 2 && std::string_view(argv[1]) == "--configure-service") {
        return showServerSettings();
    }

    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) {
        return 1;
    }
    const QString endpoint = QStringLiteral("UMTerminalStatus-%1").arg(session);
    const WinHandle instance(CreateMutexW(nullptr, FALSE, L"Local\\UMTerminalStatus"));
    const DWORD mutexResult = GetLastError();
    if (!instance) {
        return 1;
    }
    const bool quiet = app.arguments().contains(QStringLiteral("--tray"));
    if (mutexResult == ERROR_ALREADY_EXISTS) {
        if (quiet) {
            return 0;
        }
        QLocalSocket socket;
        for (int attempt = 0; attempt < 5; ++attempt) {
            socket.connectToServer(endpoint);
            if (socket.waitForConnected(200)) {
                return 0;
            }
            QThread::msleep(100);
        }
        return 1;
    }

    StatusWindow window(registrationStatusDirectory());
    QLocalServer activation;
    activation.setSocketOptions(QLocalServer::UserAccessOption);
    QObject::connect(&activation, &QLocalServer::newConnection, &window, [&] {
        while (auto *connection = activation.nextPendingConnection()) {
            connection->deleteLater();
        }
        window.showPanel();
    });
    activation.listen(endpoint);
    if (!quiet) {
        window.showPanel();
    }
    return app.exec();
}
