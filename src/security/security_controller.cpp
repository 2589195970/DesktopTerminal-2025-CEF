#include "security_controller.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QApplication>
#include <QMessageBox>

SecurityController::SecurityController(QObject *parent)
    : QObject(parent)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_violationCount(0)
{
    m_logger->appEvent("SecurityController创建");
}

SecurityController::~SecurityController()
{
    m_logger->appEvent("SecurityController销毁");
}

bool SecurityController::initialize()
{
    m_logger->appEvent("SecurityController初始化开始");

    m_logger->appEvent("SecurityController初始化完成");
    return true;
}

void SecurityController::handleSecurityViolation(SecurityViolationType violation, const QString& description)
{
    m_violationCount++;
    QString violationDesc = getViolationDescription(violation);
    QString fullDescription = QString("%1: %2").arg(violationDesc).arg(description);
    
    logSecurityEvent("安全违规", fullDescription);
    
    emit securityViolationDetected(violation, fullDescription);
}

int SecurityController::getViolationCount() const
{
    return m_violationCount;
}

void SecurityController::resetViolationCount()
{
    m_violationCount = 0;
    m_logger->appEvent("安全违规计数已重置");
}

void SecurityController::performSecurityCheck()
{
}

QString SecurityController::getViolationDescription(SecurityViolationType type)
{
    switch (type) {
        case UnauthorizedURL:
            return "未授权URL访问";
        case ForbiddenKeyboard:
            return "禁止的键盘操作";
        case WindowManipulation:
            return "窗口操作违规";
        case ProcessViolation:
            return "进程安全违规";
        default:
            return "未知安全违规";
    }
}

void SecurityController::logSecurityEvent(const QString& event, const QString& details)
{
    m_logger->logEvent(event, details, "security.log", L_WARNING);
}
