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
static MapPortProto g_mapport_current_proto = MapPortProto::NONE;
static unsigned int g_mapport_target_proto = g_mapport_current_proto;
static constexpr auto PORT_MAPPING_REANNOUNCE_PERIOD = std::chrono::minutes(20);
static constexpr auto PORT_MAPPING_RETRY_PERIOD = std::chrono::minutes(5);

#ifdef USE_NATPMP
static bool NatpmpInit(natpmp_t* natpmp)
{
    const int r_init = initnatpmp(natpmp, /* detect gateway automatically */ 0, /* forced gateway - NOT APPLIED*/ 0);
    if (r_init == 0) return true;
    LogPrintf("NAT-PMP: initnatpmp() failed with %d error.\n", r_init);
    return false;
}

static bool NatpmpDiscover(natpmp_t* natpmp)
{
    if (!fDiscover) return true;

    const int r_send = sendpublicaddressrequest(natpmp);
    if (r_send == 2 /* OK */) {
        int r_read;
        natpmpresp_t response;
        do {
            r_read = readnatpmpresponseorretry(natpmp, &response);
        } while (r_read == NATPMP_TRYAGAIN);

        if (r_read == 0) {
            const char* public_address = inet_ntoa(response.pnu.publicaddress.addr);
            if (public_address[0]) {
                CNetAddr resolved;
                if (LookupHost(public_address, resolved, false)) {
                    LogPrintf("NAT-PMP: Public address = %s\n", resolved.ToString());
                    AddLocal(resolved, LOCAL_MAPPED);
                }
                return true;
            } else {
                LogPrintf("NAT-PMP: Discovery of own public IP address failed.\n");
            }
        } else if (r_read == NATPMP_ERR_NOGATEWAYSUPPORT) {
            LogPrintf("NAT-PMP: The gateway does not support NAT-PMP.\n");
        } else {
            LogPrintf("NAT-PMP: readnatpmpresponseorretry() for public address failed with %d error.\n", r_read);
        }
    } else {
        LogPrintf("NAT-PMP: sendpublicaddressrequest() failed with %d error.\n", r_send);
    }

    return false;
}

static bool NatpmpMapping(natpmp_t* natpmp, uint16_t port)
{
    const int r_send = sendnewportmappingrequest(natpmp, NATPMP_PROTOCOL_TCP, port, port, 3600 /*seconds*/);
    if (r_send == 12 /* OK */) {
        int r_read;
        natpmpresp_t response;
        do {
            r_read = readnatpmpresponseorretry(natpmp, &response);
        } while (r_read == NATPMP_TRYAGAIN);

        if (r_read == 0) {
            auto pm = response.pnu.newportmapping;
            if (port == pm.privateport && port == pm.mappedpublicport && pm.lifetime > 0) {
                g_mapport_current_proto = MapPortProto::NAT_PMP;
                LogPrintf("NAT-PMP: Port mapping successful.\n");
                return true;
            } else {
                LogPrintf("NAT-PMP: Port mapping failed.\n");
            }
        } else if (r_read == NATPMP_ERR_NOGATEWAYSUPPORT) {
            LogPrintf("NAT-PMP: The gateway does not support NAT-PMP.\n");
        } else {
            LogPrintf("NAT-PMP: readnatpmpresponseorretry() for port mapping failed with %d error.\n", r_read);
        }
    } else {
        LogPrintf("NAT-PMP: sendnewportmappingrequest() failed with %d error.\n", r_send);
    }

    return false;
}

static bool ThreadNatpmp()
{
    bool ret = false;
    natpmp_t natpmp;
    if (NatpmpInit(&natpmp) && NatpmpDiscover(&natpmp)) {
        const uint16_t port = GetListenPort();
        do {
            ret = NatpmpMapping(&natpmp, port);
        } while (ret && g_mapport_interrupt.sleep_for(PORT_MAPPING_REANNOUNCE_PERIOD));

        const int r_send = sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_TCP, port, port, /* remove a port mapping */ 0);
        if (r_send == 12 /* OK */) {
            LogPrintf("NAT-PMP: Port mapping removed successfully.\n");
        } else {
            LogPrintf("NAT-PMP: sendnewportmappingrequest(0) failed with %d error.\n", r_send);
        }
    }

    closenatpmp(&natpmp);
    return ret;
}
#endif // #ifdef USE_NATPMP

#ifdef USE_UPNP
static bool ThreadUpnp()
{
    bool ret = false;
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
                ret = false;
                LogPrintf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n", port, port, lanaddr, r, strupnperror(r));
                break;
            } else {
                ret = true;
                g_mapport_current_proto = MapPortProto::UPNP;
                LogPrintf("UPnP Port Mapping successful.\n");
            }
        } while (g_mapport_interrupt.sleep_for(PORT_MAPPING_REANNOUNCE_PERIOD));

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

    return ret;
}
#endif // #ifdef USE_UPNP

static void ThreadMapPort()
{
    bool ok;
    do {
        ok = false;

#ifdef USE_NATPMP
        if (g_mapport_target_proto & MapPortProto::NAT_PMP) {
            ok = ThreadNatpmp();
            if (ok) continue;
        }
#endif // #ifdef USE_NATPMP

#ifdef USE_UPNP
        if (g_mapport_target_proto & MapPortProto::UPNP) {
            ok = ThreadUpnp();
            if (ok) continue;
        }
#endif // #ifdef USE_UPNP

        if (g_mapport_target_proto == MapPortProto::NONE) {
            g_mapport_current_proto = MapPortProto::NONE;
            return;
        }

    } while (ok || g_mapport_interrupt.sleep_for(PORT_MAPPING_RETRY_PERIOD));
}

void StartMapPort(bool use_natpmp, bool use_upnp)
{
    if (use_natpmp) {
        g_mapport_target_proto |= MapPortProto::NAT_PMP;
    } else {
        g_mapport_target_proto &= ~MapPortProto::NAT_PMP;
    }

    if (use_upnp) {
        g_mapport_target_proto |= MapPortProto::UPNP;
    } else {
        g_mapport_target_proto &= ~MapPortProto::UPNP;
    }

    if (g_mapport_current_proto == MapPortProto::NONE) {
        if (g_mapport_target_proto == MapPortProto::NONE) {
            return;
        }
    } else if (g_mapport_target_proto & g_mapport_current_proto) {
        return;
    }

    if (g_mapport_target_proto == MapPortProto::NONE) {
        InterruptMapPort();
        StopMapPort();
        return;
    }

    if (g_mapport_thread.joinable()) {
        g_mapport_interrupt();
    } else {
        assert(!g_mapport_interrupt);
        g_mapport_thread = std::thread((std::bind(&TraceThread<void (*)()>, "mapport", &ThreadMapPort)));
    }
}

void InterruptMapPort()
{
    g_mapport_target_proto = MapPortProto::NONE;
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
void StartMapPort(bool use_natpmp, bool use_upnp)
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
