#ifndef DESKTOP_CRYPTO_H
#define DESKTOP_CRYPTO_H

#include <QByteArray>
#include <QString>

class DesktopCrypto
{
public:
    static QString encryptAesCbcBase64(const QString &plainText, const QString &base64AesKey);
    static QString hmacSha256Hex(const QString &payload, const QString &secret);
    static QByteArray randomBytes(int length);
};

#endif
