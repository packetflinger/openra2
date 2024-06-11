/**
 * IP address helper functions.
 */
#include "g_local.h"

/**
 * Check whether 2 IPs are the same
 */
qboolean net_addressesMatch(netadr_t *a1, netadr_t *a2)
{
    int len;
    if (a1->type != a2->type) {
        return false;
    }
    len = (a1->type == NA_IP6) ? IP6_LEN : IP4_LEN;
    return memcmp(a1->ip.u8, a2->ip.u8, len);
}

/**
 * Get a string representation of a netadr_t.
 *
 * wrapv6 arg controls whether IPv6 addresses are sandwiched in between brackets or not.
 * incport arg controls whether ":portnum" will be appended.
 * incmask arg controls whether "/xx" cidr mask will be appended.
 */
char *net_addressToString(netadr_t *address, qboolean wrapv6, qboolean incport, qboolean incmask)
{
    char temp[INET6_ADDRSTRLEN];
    static char dest[INET6_ADDRSTRLEN];

    memset(temp, 0, sizeof(temp));
    memset(dest, 0, sizeof(dest));

    if (address->type == NA_IP6) {
        inet_ntop(AF_INET6, &address->ip.u8, temp, INET6_ADDRSTRLEN);
        if (wrapv6) {
            strcpy(dest, va("[%s]", temp));
        } else {
            strcpy(dest, temp);
        }
    } else {
        inet_ntop(AF_INET, &address->ip.u8, temp, INET_ADDRSTRLEN);
        strcpy(dest, temp);
    }

    if (incmask) {
        strcat(dest, va("/%d", address->mask_bits));
    }

    if (incport) {
        strcat(dest, va(":%d", address->port));
    }
    return dest;
}

/**
 * The version in math.h was weird with values between 0-1
 */
int net_ceil(float x)
{
    float temp;

    temp = x - (int)x;
    if (temp > 0) {
        return ((int)x) + 1;
    } else {
        return (int)x;
    }
}

/**
 * Get a netadr_t representing the subnet mask based on
 * CIDR notation (number of bits in the mask).
 *
 * Calculating this for IPv6 was for serious...
 */
netadr_t net_cidrToMask(int cidr, netadrtype_t t)
{
    int i;
    uint32_t mask = 0;
    netadr_t addr;
    int quarterbits;
    int sixteenth;
    int quarter;
    int qstart;

    memset(&addr, 0, sizeof(netadr_t));
    if (t != NA_IP6) {
        addr.type = NA_IP;
        for (i=1; i<=cidr; i++) {
            mask += 1 << (32-i);
        }
        addr.ip.u8[3] = mask & 0xff;
        addr.ip.u8[2] = (mask >> 8) & 0xff;
        addr.ip.u8[1] = (mask >> 16) & 0xff;
        addr.ip.u8[0] = (mask >> 24) & 0xff;
    } else {
        addr.type = NA_IP6;
        if (cidr == 0) {
            return addr;
        }
        if (cidr > 128) {
            cidr = 128;
        }

        sixteenth = net_ceil((float)cidr/8);
        sixteenth--;
        quarter = net_ceil((float)cidr/32);
        quarter--;
        quarterbits = cidr - (quarter * 32);
        qstart = quarter * 4;

        memset(&addr.ip.u8, 0xff, sixteenth);

        for (i=1; i<=quarterbits; i++) {
            mask += 1 << (32-i);
        }

        addr.ip.u8[qstart+3] = mask & 0xff;
        addr.ip.u8[qstart+2] = (mask >> 8) & 0xff;
        addr.ip.u8[qstart+1] = (mask >> 16) & 0xff;
        addr.ip.u8[qstart+0] = (mask >> 24) & 0xff;
    }
    return addr;
}

/**
 * Tests if a network address is in a particular subnet
 */
qboolean net_contains(netadr_t *network, netadr_t *host)
{
    if (network->type != host->type) {
        return false;
    }
    netadr_t mask = net_cidrToMask(network->mask_bits, network->type);
    for (int i=0; i<IP6_LEN; i++) {
        if ((network->ip.u8[i] & mask.ip.u8[i]) != (host->ip.u8[i] & mask.ip.u8[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Just to complement q2a_ceil
 */
int net_floor(float x)
{
   return (int)x;
}

/**
 * Parse a string representation of the player's IP address
 * into a netadr_t struct. This support both IPv4 and IPv6
 * addresses.
 *
 * This does not do input format checking, so be careful.
 *
 * Input format: "192.2.0.4:1234" or "[2001:db8::face]:23456"
 */
netadr_t net_parseIP(const char *ip)
{
    char *delim;
    int addrlen;           // number of characters in IP string
    char addr[40];         // temporarily hold just the IP part
    struct in6_addr addr6; // use for both versions
    netadr_t address;

    memset(addr, 0, 40);
    memset(&address, 0, sizeof(netadr_t));
    memset(&addr6, 0, sizeof(struct in6_addr));

    // Look for IPv6
    delim = strstr(ip, "]:");
    if (delim) {
        address.type = NA_IP6;
        address.port = (uint16_t) atoi(delim + 2);
        addrlen = (int) (delim - (ip + 1));
        memcpy(addr, ip + 1, addrlen);
        inet_pton(AF_INET6, addr, &addr6);
        memcpy(&address.ip.u8, &addr6.s6_addr, 16);
        return address;
    }

    // assume it's an IPv4 address
    delim = strstr(ip, ":");
    if (delim) {
        address.type = NA_IP;
        address.port = (uint16_t) atoi(delim + 1);
        addrlen = (int) (delim - ip);
        memcpy(addr, ip, addrlen);
        inet_pton(AF_INET, addr, &addr6);
        memcpy(&address.ip.u8, &addr6.s6_addr, sizeof(in_addr_t));
    }
    return address;
}

/**
 * Parse a string representation of an IP address into a netadr_t.
 * This support both IPv4 and IPv6 addresses. Address should not have
 * a port appended and IPv6 addresses shouldn't be surrounded by
 * square brackets.
 *
 * This does not do input format checking, so be careful.
 *
 * Input format: "192.2.0.4" or "2001:db8::face"
 */
netadr_t net_parseIPAddressBase(const char *ip)
{
    netadr_t address;
    char *delim;
    char addr[40];         // temporarily hold just the IP part
    struct in6_addr addr6; // use for both versions

    memset(addr, 0, 40);
    memset(&address, 0, sizeof(netadr_t));
    memset(&addr6, 0, sizeof(struct in6_addr));

    // Look for IPv6
    delim = strstr(ip, ":");
    if (delim) {
        address.type = NA_IP6;
        inet_pton(AF_INET6, ip, &addr6);
        memcpy(address.ip.u8, addr6.s6_addr, 16);
        return address;
    }

    // assume it's an IPv4 address
    delim = strstr(ip, ".");
    if (delim) {
        address.type = NA_IP;
        inet_pton(AF_INET, ip, &addr6);
        memcpy(address.ip.u8, addr6.s6_addr, sizeof(in_addr_t));
        return address;
    }
    return address;
}

/**
 * Parse a basic string IP address with or without a CIDR mask.
 *
 * Examples:
 * 192.0.2.5
 * 192.0.2.5/27
 * 2002:db8::4
 * 2002:db8::4/64
 */
netadr_t net_parseIPAddressMask(const char *ip)
{
    netadr_t address;
    char *delim;
    char addr[40];         // temporarily hold just the IP part
    struct in6_addr addr6; // use for both versions

    memset(addr, 0, 40);
    memset(&address, 0, sizeof(netadr_t));
    memset(&addr6, 0, sizeof(struct in6_addr));

    // Look for IPv6
    delim = strstr(ip, ":");
    if (delim) {
        address.type = NA_IP6;
        delim = strstr(ip, "/");
        if (delim) {
            memcpy(addr, ip, (delim-ip));
            address.mask_bits = atoi(delim+1);
        } else {
            strcpy(addr, ip);
            address.mask_bits = 128;
        }
        inet_pton(AF_INET6, addr, &addr6);
        memcpy(address.ip.u8, addr6.s6_addr, 16);
        return address;
    }

    // assume it's an IPv4 address
    delim = strstr(ip, ".");
    if (delim) {
        address.type = NA_IP;
        delim = strstr(ip, "/");
        if (delim) {
            memcpy(addr, ip, (delim-ip));
            address.mask_bits = atoi(delim+1);
        } else {
            strcpy(addr, ip);
            address.mask_bits = 32;
        }
        inet_pton(AF_INET, addr, &addr6);
        memcpy(address.ip.u8, addr6.s6_addr, sizeof(in_addr_t));
        return address;
    }
    return address;
}
