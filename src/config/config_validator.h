#ifndef CONFIG_VALIDATOR_H
#define CONFIG_VALIDATOR_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QVariant>

/**
 * @brief 配置验证结果
 */
struct ValidationResult {
    bool isValid;
    QStringList errors;
    QStringList warnings;
    QString summary;
    
    ValidationResult() : isValid(true) {}
    
    void addError(const QString& error) {
        isValid = false;
        errors.append(error);
    }
    
    void addWarning(const QString& warning) {
        warnings.append(warning);
    }
    
    bool hasIssues() const {
        return !isValid || !warnings.isEmpty();
    }
};

/**
 * @brief 配置字段验证规则
 */
struct FieldRule {
    QString fieldName;
    QVariant::Type expectedType;
    bool required;
    QVariant defaultValue;
    QVariant minValue;
    QVariant maxValue;
    QStringList allowedValues;
    QString description;
    
    FieldRule(const QString& name, QVariant::Type type, bool req = true)
        : fieldName(name), expectedType(type), required(req) {}
};

/**
 * @brief 配置验证器类
 * 
 * 提供JSON配置文件的结构验证和数据校验功能
 */
class ConfigValidator : public QObject
{
    Q_OBJECT

public:
    explicit ConfigValidator(QObject* parent = nullptr);
    ~ConfigValidator() = default;

    /**
     * @brief 验证配置对象
     * @param config 配置JSON对象
     * @return 验证结果
     */
    ValidationResult validateConfig(const QJsonObject& config);

    /**
     * @brief 验证配置文件
     * @param configPath 配置文件路径
     * @return 验证结果
     */
    ValidationResult validateConfigFile(const QString& configPath);

    /**
     * @brief 添加字段验证规则
     * @param rule 验证规则
     */
    void addFieldRule(const FieldRule& rule);

    /**
     * @brief 移除字段验证规则
     * @param fieldName 字段名
     */
    void removeFieldRule(const QString& fieldName);

    /**
     * @brief 获取所有验证规则
     */
    QList<FieldRule> getAllRules() const;

    /**
     * @brief 设置严格模式
     * @param strict 是否启用严格模式（禁止未定义字段）
     */
    void setStrictMode(bool strict);

    /**
     * @brief 生成配置模板
     * @return 包含所有必需字段的模板JSON对象
     */
    QJsonObject generateTemplate();

    /**
     * @brief 修复配置（添加缺失的默认值）
     * @param config 要修复的配置
     * @return 修复后的配置
     */
    QJsonObject fixConfig(const QJsonObject& config);

    /**
     * @brief 获取字段描述文档
     * @return 字段说明文档
     */
    QString generateDocumentation();

private:
    // 验证方法
    bool validateField(const QString& fieldName, const QJsonValue& value, 
                      const FieldRule& rule, ValidationResult& result);
    bool validateType(const QJsonValue& value, QVariant::Type expectedType);
    bool validateRange(const QVariant& value, const QVariant& minVal, const QVariant& maxVal);
    bool validateAllowedValues(const QVariant& value, const QStringList& allowedValues);
    
    // 工具方法
    QString typeToString(QVariant::Type type);
    QVariant jsonValueToVariant(const QJsonValue& value);

private:
    QMap<QString, FieldRule> m_fieldRules;
    bool m_strictMode;
};

/**
 * @brief 默认配置验证器
 * 
 * 包含应用程序的标准配置验证规则
 */
class DefaultConfigValidator : public ConfigValidator
{
    Q_OBJECT

public:
    explicit DefaultConfigValidator(QObject* parent = nullptr);

private:
    void setupDefaultRules();
};

#endif // CONFIG_VALIDATOR_H