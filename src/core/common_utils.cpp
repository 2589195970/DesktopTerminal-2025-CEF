#include "common_utils.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"
#include <QMessageBox>
#include <QWidget>

namespace CommonUtils {

    Managers::Managers()
        : logger(&Logger::instance())
        , configManager(&ConfigManager::instance())
    {
    }

    Managers getManagers()
    {
        return Managers();
    }

    bool logErrorAndReturnFalse(Logger* logger, const QString& message)
    {
        if (logger) {
            logger->errorEvent(message);
        }
        return false;
    }

    void showErrorDialog(QWidget* parent, const QString& title, const QString& message, Logger* logger)
    {
        if (logger) {
            logger->errorEvent(QString("%1: %2").arg(title).arg(message));
        }
        QMessageBox::critical(parent, title, message);
    }

    bool safeExecute(const std::function<bool()>& operation, const QString& errorMessage, Logger* logger)
    {
        try {
            return operation();
        } catch (const std::exception& e) {
            QString fullMessage = QString("%1: %2").arg(errorMessage).arg(e.what());
            if (logger) {
                logger->errorEvent(fullMessage);
            }
            return false;
        } catch (...) {
            if (logger) {
                logger->errorEvent(errorMessage + ": 未知异常");
            }
            return false;
        }
    }

    bool safeExecuteVoid(const std::function<void()>& operation, const QString& errorMessage, Logger* logger)
    {
        try {
            operation();
            return true;
        } catch (const std::exception& e) {
            QString fullMessage = QString("%1: %2").arg(errorMessage).arg(e.what());
            if (logger) {
                logger->errorEvent(fullMessage);
            }
            return false;
        } catch (...) {
            if (logger) {
                logger->errorEvent(errorMessage + ": 未知异常");
            }
            return false;
        }
    }

    bool checkInitCondition(bool condition, const QString& successMessage, const QString& errorMessage, Logger* logger)
    {
        if (condition) {
            if (logger && !successMessage.isEmpty()) {
                logger->appEvent(successMessage);
            }
            return true;
        } else {
            if (logger) {
                logger->errorEvent(errorMessage);
            }
            return false;
        }
    }

    QString formatError(const QString& operation, const QString& details)
    {
        if (details.isEmpty()) {
            return QString("%1失败").arg(operation);
        } else {
            return QString("%1失败: %2").arg(operation).arg(details);
        }
    }

    QString formatSuccess(const QString& operation, const QString& details)
    {
        if (details.isEmpty()) {
            return QString("%1成功").arg(operation);
        } else {
            return QString("%1成功: %2").arg(operation).arg(details);
        }
    }

} // namespace CommonUtils