#include "platform_win.h"

#ifdef Q_OS_WIN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ipifcons.h>
#include <netioapi.h>

#include <vector>

namespace terminal {
namespace {

bool usableIpv4(const QHostAddress &address)
{
    return address.protocol() == QAbstractSocket::IPv4Protocol && address.toIPv4Address() != 0
            && !address.isLoopback() && !address.isInSubnet(QHostAddress(QStringLiteral("169.254.0.0")), 16);
}

QString adapterMac(const IP_ADAPTER_ADDRESSES &adapter)
{
    if (adapter.PhysicalAddressLength != 6) {
        return {};
    }
    const auto *bytes = reinterpret_cast<const char *>(adapter.PhysicalAddress);
    return QString::fromLatin1(QByteArray(bytes, 6).toHex(':').toUpper());
}

QString enabledEthernetMac(const IP_ADAPTER_ADDRESSES *adapters)
{
    QString enabledMac;
    for (const IP_ADAPTER_ADDRESSES *adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->IfType != IF_TYPE_ETHERNET_CSMACD) {
            continue;
        }
        MIB_IF_ROW2 info{};
        info.InterfaceLuid = adapter->Luid;
        if (GetIfEntry2(&info) != NO_ERROR || info.AdminStatus != NET_IF_ADMIN_STATUS_UP
                || !info.InterfaceAndOperStatusFlags.HardwareInterface) {
            continue;
        }
        const QString mac = adapterMac(*adapter);
        if (mac.isEmpty()) {
            continue;
        }
        if (adapter->OperStatus == IfOperStatusUp) {
            return mac;
        }
        if (enabledMac.isEmpty()) {
            enabledMac = mac;
        }
    }
    return enabledMac;
}

NetworkIdentity identityForRoute(const MIB_IPFORWARD_ROW2 &route, const QHostAddress &localIp,
                                 const IP_ADAPTER_ADDRESSES *adapters)
{
    for (const IP_ADAPTER_ADDRESSES *adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->IfIndex != route.InterfaceIndex) {
            continue;
        }
        for (const auto *entry = adapter->FirstUnicastAddress; entry; entry = entry->Next) {
            const SOCKADDR *address = entry->Address.lpSockaddr;
            if (!address || address->sa_family != AF_INET || entry->OnLinkPrefixLength > 32) {
                continue;
            }
            const auto *ipv4 = reinterpret_cast<const SOCKADDR_IN *>(address);
            if (ntohl(ipv4->sin_addr.S_un.S_addr) == localIp.toIPv4Address()) {
                const QString mac = adapter->IfType == IF_TYPE_ETHERNET_CSMACD
                        ? adapterMac(*adapter) : enabledEthernetMac(adapters);
                return {localIp, entry->OnLinkPrefixLength, mac};
            }
        }
    }
    return {};
}

} // namespace

NetworkIdentity findNetworkIdentity(const QList<QHostAddress> &serverAddresses)
{
    MIB_IPFORWARD_ROW2 route{};
    QHostAddress localIp;
    for (const QHostAddress &server : serverAddresses) {
        if (server.protocol() != QAbstractSocket::IPv4Protocol) {
            continue;
        }
        SOCKADDR_INET destination{};
        destination.Ipv4.sin_family = AF_INET;
        destination.Ipv4.sin_addr.S_un.S_addr = htonl(server.toIPv4Address());
        SOCKADDR_INET source{};
        if (GetBestRoute2(nullptr, 0, nullptr, &destination, 0, &route, &source) == NO_ERROR
                && source.si_family == AF_INET) {
            const QHostAddress candidate(ntohl(source.Ipv4.sin_addr.S_un.S_addr));
            if (usableIpv4(candidate)) {
                localIp = candidate;
                break;
            }
        }
    }
    if (localIp.isNull()) {
        return {};
    }

    constexpr ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES | GAA_FLAG_SKIP_ANYCAST
            | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG length = 15 * 1024;
    // 以结构体为单位分配，确保传给 Windows 的缓冲区满足对齐要求。
    std::vector<IP_ADAPTER_ADDRESSES> buffer;
    for (int attempt = 0; attempt < 3; ++attempt) {
        buffer.resize((length + sizeof(IP_ADAPTER_ADDRESSES) - 1) / sizeof(IP_ADAPTER_ADDRESSES));
        const ULONG status = GetAdaptersAddresses(AF_INET, flags, nullptr, buffer.data(), &length);
        if (status == NO_ERROR) {
            return identityForRoute(route, localIp, buffer.data());
        }
        if (status != ERROR_BUFFER_OVERFLOW) {
            break;
        }
    }
    return {};
}

} // namespace terminal

#endif
