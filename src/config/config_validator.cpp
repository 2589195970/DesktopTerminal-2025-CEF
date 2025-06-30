#include "config_validator.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QUrl>

ConfigValidator::ConfigValidator(QObject* parent)
    : QObject(parent), m_strictMode(false)
{
}

ValidationResult ConfigValidator::validateConfig(const QJsonObject& config)
{
    ValidationResult result;
    
    // 检查必需字段
    for (auto it = m_fieldRules.begin(); it != m_fieldRules.end(); ++it) {
        const FieldRule& rule = it.value();
        
        if (rule.required && !config.contains(rule.fieldName)) {
            result.addError(QString("缺少必需字段: %1").arg(rule.fieldName));
            continue;
        }
        
        if (config.contains(rule.fieldName)) {
            QJsonValue value = config[rule.fieldName];
            validateField(rule.fieldName, value, rule, result);
        }
    }
    
    // 严格模式下检查未定义字段
    if (m_strictMode) {
        for (auto it = config.begin(); it != config.end(); ++it) {
            if (!m_fieldRules.contains(it.key())) {
                result.addWarning(QString("未定义的字段: %1").arg(it.key()));
            }
        }
    }
    
    // 生成摘要
    if (result.isValid) {
        if (result.warnings.isEmpty()) {
            result.summary = "配置验证通过";
        } else {
            result.summary = QString("配置验证通过，但有 %1 个警告").arg(result.warnings.size());
        }
    } else {
        result.summary = QString("配置验证失败，有 %1 个错误").arg(result.errors.size());
        if (!result.warnings.isEmpty()) {
            result.summary += QString("和 %1 个警告").arg(result.warnings.size());
        }
    }
    
    return result;
}

ValidationResult ConfigValidator::validateConfigFile(const QString& configPath)
{
    ValidationResult result;
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.addError(QString("无法打开配置文件: %1").arg(configPath));
        return result;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    
    if (doc.isNull()) {
        result.addError(QString("JSON解析失败: %1").arg(parseError.errorString()));
        return result;
    }
    
    if (!doc.isObject()) {
        result.addError("配置文件根节点必须是JSON对象");
        return result;
    }
    
    return validateConfig(doc.object());
}

void ConfigValidator::addFieldRule(const FieldRule& rule)
{
    m_fieldRules[rule.fieldName] = rule;
}

void ConfigValidator::removeFieldRule(const QString& fieldName)
{
    m_fieldRules.remove(fieldName);
}

QList<FieldRule> ConfigValidator::getAllRules() const
{
    return m_fieldRules.values();
}

void ConfigValidator::setStrictMode(bool strict)
{
    m_strictMode = strict;
}

QJsonObject ConfigValidator::generateTemplate()
{
    QJsonObject template_;
    
    for (auto it = m_fieldRules.begin(); it != m_fieldRules.end(); ++it) {
        const FieldRule& rule = it.value();
        
        if (rule.required || !rule.defaultValue.isNull()) {
            QVariant value = rule.defaultValue.isNull() ? 
                QVariant(rule.expectedType) : rule.defaultValue;
            
            switch (rule.expectedType) {
                case QVariant::String:
                    template_[rule.fieldName] = value.toString();
                    break;
                case QVariant::Int:
                    template_[rule.fieldName] = value.toInt();
                    break;
                case QVariant::Double:
                    template_[rule.fieldName] = value.toDouble();
                    break;
                case QVariant::Bool:
                    template_[rule.fieldName] = value.toBool();
                    break;
                default:
                    template_[rule.fieldName] = QJsonValue::Null;
                    break;
            }
        }
    }
    
    return template_;
}

QJsonObject ConfigValidator::fixConfig(const QJsonObject& config)
{
    QJsonObject fixed = config;
    
    for (auto it = m_fieldRules.begin(); it != m_fieldRules.end(); ++it) {
        const FieldRule& rule = it.value();
        
        if (!fixed.contains(rule.fieldName) && !rule.defaultValue.isNull()) {
            // 添加默认值
            switch (rule.expectedType) {
                case QVariant::String:
                    fixed[rule.fieldName] = rule.defaultValue.toString();
                    break;
                case QVariant::Int:
                    fixed[rule.fieldName] = rule.defaultValue.toInt();
                    break;
                case QVariant::Double:
                    fixed[rule.fieldName] = rule.defaultValue.toDouble();
                    break;
                case QVariant::Bool:
                    fixed[rule.fieldName] = rule.defaultValue.toBool();
                    break;
                default:
                    break;
            }
        }
    }
    
    return fixed;
}

QString ConfigValidator::generateDocumentation()
{
    QString doc;
    doc += "配置文件字段说明\n";
    doc += "==================\n\n";
    
    for (auto it = m_fieldRules.begin(); it != m_fieldRules.end(); ++it) {
        const FieldRule& rule = it.value();
        
        doc += QString("**%1**\n").arg(rule.fieldName);
        doc += QString("- 类型: %1\n").arg(typeToString(rule.expectedType));
        doc += QString("- 必需: %1\n").arg(rule.required ? "是" : "否");
        
        if (!rule.defaultValue.isNull()) {
            doc += QString("- 默认值: %1\n").arg(rule.defaultValue.toString());
        }
        
        if (!rule.minValue.isNull() || !rule.maxValue.isNull()) {
            doc += "- 取值范围: ";
            if (!rule.minValue.isNull()) {
                doc += QString("最小 %1").arg(rule.minValue.toString());
            }
            if (!rule.maxValue.isNull()) {
                if (!rule.minValue.isNull()) doc += ", ";
                doc += QString("最大 %1").arg(rule.maxValue.toString());
            }
            doc += "\n";
        }
        
        if (!rule.allowedValues.isEmpty()) {
            doc += QString("- 允许的值: %1\n").arg(rule.allowedValues.join(", "));
        }
        
        if (!rule.description.isEmpty()) {
            doc += QString("- 说明: %1\n").arg(rule.description);
        }
        
        doc += "\n";
    }
    
    return doc;
}

bool ConfigValidator::validateField(const QString& fieldName, const QJsonValue& value, 
                                   const FieldRule& rule, ValidationResult& result)
{
    // 检查类型
    if (!validateType(value, rule.expectedType)) {
        result.addError(QString("字段 %1 类型错误，期望 %2")
            .arg(fieldName, typeToString(rule.expectedType)));
        return false;
    }
    
    QVariant varValue = jsonValueToVariant(value);
    
    // 检查取值范围
    if (!rule.minValue.isNull() || !rule.maxValue.isNull()) {
        if (!validateRange(varValue, rule.minValue, rule.maxValue)) {
            QString rangeStr;
            if (!rule.minValue.isNull() && !rule.maxValue.isNull()) {
                rangeStr = QString("必须在 %1 到 %2 之间")
                    .arg(rule.minValue.toString(), rule.maxValue.toString());
            } else if (!rule.minValue.isNull()) {
                rangeStr = QString("不能小于 %1").arg(rule.minValue.toString());
            } else {
                rangeStr = QString("不能大于 %1").arg(rule.maxValue.toString());
            }
            result.addError(QString("字段 %1 值超出范围，%2").arg(fieldName, rangeStr));
            return false;
        }
    }
    
    // 检查允许的值
    if (!rule.allowedValues.isEmpty()) {
        if (!validateAllowedValues(varValue, rule.allowedValues)) {
            result.addError(QString("字段 %1 值无效，允许的值: %2")
                .arg(fieldName, rule.allowedValues.join(", ")));
            return false;
        }
    }
    
    // 特殊验证逻辑
    if (fieldName == "url") {
        QUrl url(varValue.toString());
        if (!url.isValid()) {
            result.addError(QString("字段 %1 不是有效的URL").arg(fieldName));
            return false;
        }
        if (url.scheme() != "http" && url.scheme() != "https") {
            result.addWarning(QString("字段 %1 建议使用HTTP或HTTPS协议").arg(fieldName));
        }
    }
    
    return true;
}

bool ConfigValidator::validateType(const QJsonValue& value, QVariant::Type expectedType)
{
    switch (expectedType) {
        case QVariant::String:
            return value.isString();
        case QVariant::Int:
            return value.isDouble() && (value.toDouble() == static_cast<int>(value.toDouble()));
        case QVariant::Double:
            return value.isDouble();
        case QVariant::Bool:
            return value.isBool();
        default:
            return true;
    }
}

bool ConfigValidator::validateRange(const QVariant& value, const QVariant& minVal, const QVariant& maxVal)
{
    if (!minVal.isNull() && value < minVal) {
        return false;
    }
    if (!maxVal.isNull() && value > maxVal) {
        return false;
    }
    return true;
}

bool ConfigValidator::validateAllowedValues(const QVariant& value, const QStringList& allowedValues)
{
    return allowedValues.contains(value.toString());
}

QString ConfigValidator::typeToString(QVariant::Type type)
{
    switch (type) {
        case QVariant::String: return "字符串";
        case QVariant::Int: return "整数";
        case QVariant::Double: return "数字";
        case QVariant::Bool: return "布尔值";
        default: return "未知";
    }
}

QVariant ConfigValidator::jsonValueToVariant(const QJsonValue& value)
{
    switch (value.type()) {
        case QJsonValue::String:
            return value.toString();
        case QJsonValue::Double:
            return value.toDouble();
        case QJsonValue::Bool:
            return value.toBool();
        default:
            return QVariant();
    }
}

// DefaultConfigValidator 实现
DefaultConfigValidator::DefaultConfigValidator(QObject* parent)
    : ConfigValidator(parent)
{
    setupDefaultRules();
}

void DefaultConfigValidator::setupDefaultRules()
{
    // 基础配置字段
    FieldRule urlRule("url", QVariant::String, true);
    urlRule.description = "考试系统的URL地址";
    addFieldRule(urlRule);
    
    FieldRule passwordRule("exitPassword", QVariant::String, true);
    passwordRule.description = "安全退出密码";
    addFieldRule(passwordRule);
    
    FieldRule appNameRule("appName", QVariant::String, true);
    appNameRule.description = "应用程序名称";
    addFieldRule(appNameRule);
    
    // 性能配置字段
    FieldRule memoryRule("maxMemoryMB", QVariant::Int, false);
    memoryRule.defaultValue = 512;
    memoryRule.minValue = 128;
    memoryRule.maxValue = 2048;
    memoryRule.description = "最大内存使用量（MB）";
    addFieldRule(memoryRule);
    
    FieldRule lowMemoryRule("lowMemoryMode", QVariant::Bool, false);
    lowMemoryRule.defaultValue = false;
    lowMemoryRule.description = "是否启用低内存模式";
    addFieldRule(lowMemoryRule);
    
    // CEF配置字段
    FieldRule cefLogLevelRule("cefLogLevel", QVariant::String, false);
    cefLogLevelRule.defaultValue = "WARNING";
    cefLogLevelRule.allowedValues = QStringList() << "VERBOSE" << "INFO" << "WARNING" << "ERROR" << "FATAL";
    cefLogLevelRule.description = "CEF日志级别";
    addFieldRule(cefLogLevelRule);
    
    FieldRule cefCacheRule("cefCacheSizeMB", QVariant::Int, false);
    cefCacheRule.defaultValue = 128;
    cefCacheRule.minValue = 32;
    cefCacheRule.maxValue = 512;
    cefCacheRule.description = "CEF缓存大小（MB）";
    addFieldRule(cefCacheRule);
    
    // 安全配置字段
    FieldRule strictModeRule("strictSecurityMode", QVariant::Bool, false);
    strictModeRule.defaultValue = true;
    strictModeRule.description = "是否启用严格安全模式";
    addFieldRule(strictModeRule);
    
    FieldRule keyboardFilterRule("keyboardFilterEnabled", QVariant::Bool, false);
    keyboardFilterRule.defaultValue = true;
    keyboardFilterRule.description = "是否启用键盘过滤";
    addFieldRule(keyboardFilterRule);
    
    // 日志配置字段
    FieldRule logLevelRule("logLevel", QVariant::String, false);
    logLevelRule.defaultValue = "INFO";
    logLevelRule.allowedValues = QStringList() << "DEBUG" << "INFO" << "WARNING" << "ERROR";
    logLevelRule.description = "日志级别";
    addFieldRule(logLevelRule);
}