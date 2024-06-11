#ifndef G_NET_H
#define G_NET_H

#include <arpa/inet.h>
#define IP4_LEN  4
#define IP6_LEN 16

#define IP(x)     (net_addressToString(x, false, false, false))
#define IPMASK(x) (net_addressToString(x, false, false, true))
#define IPPORT(x) (net_addressToString(x, false, true, false))
#define HASIP(x)  (x.ip.u8[0] != 0)

typedef enum {
    NA_UNSPECIFIED,
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
    NA_IP6
} netadrtype_t;

typedef union {
    uint8_t u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];
} netadrip_t;

typedef struct netadr_s {
    netadrtype_t type;
    netadrip_t ip;
    uint16_t port;
    uint32_t scope_id;
    uint8_t mask_bits;
} netadr_t;

qboolean net_addressesMatch(netadr_t *a1, netadr_t *a2);
char *net_addressToString(netadr_t *address, qboolean wrapv6, qboolean incport, qboolean incmask);
int net_ceil(float x);
netadr_t net_cidrToMask(int cidr, netadrtype_t t);
qboolean net_contains(netadr_t *network, netadr_t *host);
int net_floor(float x);
netadr_t net_parseIP(const char *ip);
netadr_t net_parseIPAddressBase(const char *ip);
netadr_t net_parseIPAddressMask(const char *ip);

#endif // G_NET_H
