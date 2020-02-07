// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <mapport.h>

#include <clientversion.h>
#include <logging.h>
#include <net.h>
#include <netaddress.h>
#include <netbase.h>
#include <threadinterrupt.h>
#include <util/system.h>

#ifdef USE_NATPMP
#include <natpmp.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif // #ifdef WIN32
#endif // #ifdef USE_NATPMP

#ifdef USE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
// The minimum supported miniUPnPc API version is set to 10. This keeps compatibility
// with Ubuntu 16.04 LTS and Debian 8 libminiupnpc-dev packages.
static_assert(MINIUPNPC_API_VERSION >= 10, "miniUPnPc API version >= 10 assumed");
#endif // #ifdef USE_UPNP

#include <cassert>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#if defined(USE_NATPMP) || defined(USE_UPNP)
static CThreadInterrupt g_mapport_interrupt;
static std::thread g_mapport_thread;

#ifdef USE_NATPMP
static void ThreadNatpmp()
{
    int r;
    natpmp_t natpmp;
    natpmpresp_t response;

    r = initnatpmp(&natpmp, /* detect gateway automatically */ 0, /* forced gateway - NOT APPLIED*/ 0);
    if (r < 0) {
        LogPrintf("NAT-PMP: initnatpmp() failed with %d error.\n", r);
        closenatpmp(&natpmp);
        return;
    }

    if (fDiscover) {
        r = sendpublicaddressrequest(&natpmp);
        if (r < 0) {
            LogPrintf("NAT-PMP: sendpublicaddressrequest() failed with %d error.\n", r);
        } else {
            do {
                r = readnatpmpresponseorretry(&natpmp, &response);
            } while (r == NATPMP_TRYAGAIN);
            if (r < 0 || response.type != NATPMP_RESPTYPE_PUBLICADDRESS) {
                LogPrintf("NAT-PMP: readnatpmpresponseorretry() for public address failed with %d error.\n", r);
            } else {
                const char* public_address = inet_ntoa(response.pnu.publicaddress.addr);
                if (public_address[0]) {
                    CNetAddr resolved;
                    if (LookupHost(public_address, resolved, false)) {
                        LogPrintf("NAT-PMP: public address = %s\n", resolved.ToString());
                        AddLocal(resolved, LOCAL_MAPPED);
                    }
                } else {
                    LogPrintf("NAT-PMP: sendpublicaddressrequest() failed.\n");
                }
            }
        }
    }

    const uint16_t port = GetListenPort();
    do {
        r = sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_TCP, port, port, 3600 /*seconds*/);
        if (r < 0) {
            LogPrintf("NAT-PMP: sendnewportmappingrequest() failed with %d error.\n", r);
            break;
        }

        do {
            r = readnatpmpresponseorretry(&natpmp, &response);
        } while (r == NATPMP_TRYAGAIN);
        if (r < 0 || response.type != NATPMP_RESPTYPE_TCPPORTMAPPING) {
            LogPrintf("NAT-PMP: readnatpmpresponseorretry() for port mapping failed with %d error.\n", r);
        } else {
            auto pm = response.pnu.newportmapping;
            if (port == pm.privateport && port == pm.mappedpublicport && pm.lifetime > 0) {
                LogPrintf("NAT-PMP: port mapping successful.\n");
            } else {
                LogPrintf("NAT-PMP: sendnewportmappingrequest() failed.\n");
            }
        }

    } while (g_mapport_interrupt.sleep_for(std::chrono::minutes(20)));

    r = sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_TCP, port, port, /* remove a port mapping */ 0);
    if (r < 0) {
        LogPrintf("NAT-PMP: sendnewportmappingrequest(0) failed with %d error.\n", r);
    }

    closenatpmp(&natpmp);
}
#endif // #ifdef USE_NATPMP

#ifdef USE_UPNP
static void ThreadUpnp()
{
    std::string port = strprintf("%u", GetListenPort());
    const char * multicastif = nullptr;
    const char * minissdpdpath = nullptr;
    struct UPNPDev * devlist = nullptr;
    char lanaddr[64];

    int error = 0;
#if MINIUPNPC_API_VERSION < 14
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#else
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, 2, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if (r != UPNPCOMMAND_SUCCESS) {
                LogPrintf("UPnP: GetExternalIPAddress() returned %d\n", r);
            } else {
                if (externalIPAddress[0]) {
                    CNetAddr resolved;
                    if (LookupHost(externalIPAddress, resolved, false)) {
                        LogPrintf("UPnP: ExternalIPAddress = %s\n", resolved.ToString());
                        AddLocal(resolved, LOCAL_MAPPED);
                    }
                } else {
                    LogPrintf("UPnP: GetExternalIPAddress failed.\n");
                }
            }
        }

        std::string strDesc = PACKAGE_NAME " " + FormatFullVersion();

        do {
            r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");

            if (r != UPNPCOMMAND_SUCCESS) {
                LogPrintf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n", port, port, lanaddr, r, strupnperror(r));
            } else {
                LogPrintf("UPnP Port Mapping successful.\n");
            }
        } while (g_mapport_interrupt.sleep_for(std::chrono::minutes(20)));

        r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);
        LogPrintf("UPNP_DeletePortMapping() returned: %d\n", r);
        freeUPNPDevlist(devlist); devlist = nullptr;
        FreeUPNPUrls(&urls);
    } else {
        LogPrintf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist); devlist = nullptr;
        if (r != 0)
            FreeUPNPUrls(&urls);
    }
}
#endif // #ifdef USE_UPNP

void StartMapPort(MapPort proto)
{
    if (!g_mapport_thread.joinable()) {
        assert(!g_mapport_interrupt);
        switch (proto) {
        case MapPort::NAT_PMP: {
#ifdef USE_NATPMP
            g_mapport_thread = std::thread((std::bind(&TraceThread<void (*)()>, "natpmp", &ThreadNatpmp)));
#endif // #ifdef USE_NATPMP
            return;
        }
        case MapPort::UPNP: {
#ifdef USE_UPNP
            g_mapport_thread = std::thread((std::bind(&TraceThread<void (*)()>, "upnp", &ThreadUpnp)));
#endif // #ifdef USE_UPNP
            return;
        }
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    }
}

void InterruptMapPort()
{
    if (g_mapport_thread.joinable()) {
        g_mapport_interrupt();
    }
}

void StopMapPort()
{
    if (g_mapport_thread.joinable()) {
        g_mapport_thread.join();
        g_mapport_interrupt.reset();
    }
}

#else  // #if defined(USE_NATPMP) || defined(USE_UPNP)
void StartMapPort(MapPort proto)
{
    // Intentionally left blank.
}
void InterruptMapPort()
{
    // Intentionally left blank.
}
void StopMapPort()
{
    // Intentionally left blank.
}
#endif // #if defined(USE_NATPMP) || defined(USE_UPNP)
