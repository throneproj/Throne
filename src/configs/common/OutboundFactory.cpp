#include "include/configs/common/OutboundFactory.h"

#include "include/configs/outbounds/socks.h"
#include "include/configs/outbounds/http.h"
#include "include/configs/outbounds/shadowsocks.h"
#include "include/configs/outbounds/chain.h"
#include "include/configs/outbounds/autoselector.h"
#include "include/configs/outbounds/vmess.h"
#include "include/configs/outbounds/trojan.h"
#include "include/configs/outbounds/vless.h"
#include "include/configs/outbounds/xrayVless.h"
#include "include/configs/outbounds/hysteria.h"
#include "include/configs/outbounds/tuic.h"
#include "include/configs/outbounds/juicity.h"
#include "include/configs/outbounds/trusttunnel.h"
#include "include/configs/outbounds/anyTLS.h"
#include "include/configs/outbounds/mieru.h"
#include "include/configs/outbounds/snell.h"
#include "include/configs/outbounds/shadowtls.h"
#include "include/configs/outbounds/wireguard.h"
#include "include/configs/outbounds/openvpn.h"
#include "include/configs/outbounds/openconnect.h"
#include "include/configs/outbounds/tailscale.h"
#include "include/configs/outbounds/ssh.h"
#include "include/configs/outbounds/custom.h"
#include "include/configs/outbounds/extracore.h"
#include "include/configs/outbounds/naive.h"
#include "include/configs/outbounds/direct.h"

namespace Configs
{
    outbound* NewOutboundByType(const QString& type)
    {
        if (type == "socks") return new socks();
        if (type == "http") return new http();
        if (type == "shadowsocks") return new shadowsocks();
        if (type == "chain") return new chain();
        if (type == "autoselector") return new autoSelector();
        if (type == "vmess") return new vmess();
        if (type == "trojan") return new Trojan();
        if (type == "vless") return new vless();
        if (type == "xrayvless") return new xrayVless();
        if (type == "hysteria" || type == "hysteria2") return new hysteria();
        if (type == "tuic") return new tuic();
        if (type == "juicity") return new juicity();
        if (type == "trusttunnel") return new trusttunnel();
        if (type == "anytls") return new anyTLS();
        if (type == "mieru") return new mieru();
        if (type == "snell") return new snell();
        if (type == "shadowtls") return new shadowtls();
        if (type == "wireguard") return new wireguard();
        if (type == "openvpn") return new openvpn();
        if (type == "openconnect") return new openconnect();
        if (type == "tailscale") return new tailscale();
        if (type == "ssh") return new ssh();
        if (type == "custom") return new Custom();
        if (type == "extracore") return new extracore();
        if (type == "naive") return new naive();
        if (type == "direct") return new direct();
        auto* ob = new outbound();
        ob->invalid = true;
        return ob;
    }
}
