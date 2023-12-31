/*
    Socle - Socket Library Ecosystem
    Copyright (c) 2014, Ales Stibal <astib@mag0.net>, All rights reserved.

    This library  is free  software;  you can redistribute  it and/or
    modify  it  under   the  terms of the  GNU Lesser  General Public
    License  as published by  the   Free Software Foundation;  either
    version 3.0 of the License, or (at your option) any later version.
    This library is  distributed  in the hope that  it will be useful,
    but WITHOUT ANY WARRANTY;  without  even  the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the GNU Lesser General Public License for more details.

    You  should have received a copy of the GNU Lesser General Public
    License along with this library.
*/

#include <fcntl.h>
#include <random>

#include <socketinfo.hpp>
#include <common/internet.hpp>

sockaddr_storage pack_ss(int family, const char* host, unsigned short port);

void AddressInfo::unpack() {

    if(! ss.has_value()) throw socket_info_error("cannot obtain source details");

    family = SockOps::ss_address_unpack(& ss.value(), &str_host, &port);
}


sockaddr_storage pack_ss(int family, const char* host, unsigned short port) {

    sockaddr_storage ss{};

    if(family == AF_INET6) {
        auto p_ip6_src = (sockaddr_in6 *) &ss;

        inet_pton(AF_INET6, host, &p_ip6_src->sin6_addr);
        ss.ss_family = AF_INET6;
        p_ip6_src->sin6_port = htons(port);
    }
    else {
        auto p_ip4_src = (sockaddr_in *) &ss;

        inet_pton(AF_INET, host, &p_ip4_src->sin_addr);
        ss.ss_family = AF_INET;
        p_ip4_src->sin_port = htons(port);
    }

    return ss;
}

bool AddressInfo::pack() {
    ss = std::make_optional(pack_ss(family, str_host.c_str(), port));
    return ss.has_value();
}

std::string SockOps::family_str(int fa) {
    switch(fa) {
        case AF_INET:
            return std::string("ip4");
        case AF_INET6:
            return std::string("ip6");

        default:
            return string_format("p%d",fa);
    }
}



uint32_t SocketInfo::create_session_key(bool negative) {


    if(not src) {
        src.pack();
    }

    if(not dst) {
        dst.pack();
    }

    switch (src.family) {
        case AF_INET6:
            return create_session_key6(&src.ss.value(), &dst.ss.value(), negative);
        default:
            return create_session_key4(&src.ss.value(), &dst.ss.value(), negative);
    }
}

uint32_t SocketInfo::create_session_key4(sockaddr_storage* from, sockaddr_storage* orig, bool negative) {

    uint32_t s = inet::to_sockaddr_in(from)->sin_addr.s_addr;
    uint32_t d = inet::to_sockaddr_in(orig)->sin_addr.s_addr;
    uint32_t sp = ntohs(inet::to_sockaddr_in(from)->sin_port);
    uint32_t sd = ntohs(inet::to_sockaddr_in(orig)->sin_port);

    std::seed_seq seed1{ s, d, sp, sd };
    std::mt19937 e(seed1);
    std::uniform_int_distribution<> dist;

    uint32_t mirand = dist(e);


    if(negative)
        mirand |= (1UL << 31); //this will produce negative number, which should determine  if it's normal socket or not
    else
        mirand &= ~(1UL << 31); //this will explicitly remove sign bit


    return mirand; // however we return it as the key, therefore cast to unsigned int
}

uint32_t SocketInfo::create_session_key6(sockaddr_storage* from, sockaddr_storage* orig, bool negative) {

    uint32_t s0 = ((uint32_t*)&inet::to_sockaddr_in6(from)->sin6_addr)[0];
    uint32_t s1 = ((uint32_t*)&inet::to_sockaddr_in6(from)->sin6_addr)[1];
    uint32_t s2 = ((uint32_t*)&inet::to_sockaddr_in6(from)->sin6_addr)[2];
    uint32_t s3 = ((uint32_t*)&inet::to_sockaddr_in6(from)->sin6_addr)[3];

    uint32_t d0 = ((uint32_t*)&inet::to_sockaddr_in6(orig)->sin6_addr)[0];
    uint32_t d1 = ((uint32_t*)&inet::to_sockaddr_in6(orig)->sin6_addr)[1];
    uint32_t d2 = ((uint32_t*)&inet::to_sockaddr_in6(orig)->sin6_addr)[2];
    uint32_t d3 = ((uint32_t*)&inet::to_sockaddr_in6(orig)->sin6_addr)[3];

    uint32_t sp = ntohs(inet::to_sockaddr_in6(from)->sin6_port);
    uint32_t dp = ntohs(inet::to_sockaddr_in6(orig)->sin6_port);

    std::seed_seq seed1{ s0, d0, s1, d1, s2, d2, s3, d3, sp, dp };
    std::mt19937 e(seed1);
    std::uniform_int_distribution<> dist;

    uint32_t mirand = dist(e);

    if(negative)
        mirand |= (1 << 31); //this will produce negative number, which should determine  if it's normal socket or not
    else
        mirand &= ~(1UL << 31); //this will explicitly remove sign bit

    return mirand; // however we return it as the key, therefore cast to unsigned int
}

int SockOps::socket_create(int family ,int l4proto, int protocol) {
    int fd = socket(family, l4proto, protocol);

    if (fd < 0) {
        throw socket_info_error("socket call failed");

    }
    int n;

    if (n = 1; 0 != ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(int))) {
        throw socket_info_error(string_format("cannot set socket %d option SO_REUSEADDR\n", fd).c_str());
    }

    if (n = 1; 0 != ::setsockopt(fd, SOL_IP, IP_RECVORIGDSTADDR, &n, sizeof(int))) {
        throw socket_info_error(string_format("cannot set socket %d option IP_RECVORIGDSTADDR\n", fd).c_str());
    }

    if (n = 1; 0 != ::setsockopt(fd, SOL_IP, SO_BROADCAST, &n, sizeof(int))) {
        throw socket_info_error(string_format("cannot set socket %d option SO_BROADCAST\n", fd).c_str());
    }

    if (int oldf = fcntl(fd, F_GETFL, 0) ; ! (oldf & O_NONBLOCK)) {
        if (fcntl(fd, F_SETFL, oldf | O_NONBLOCK) < 0) {
            throw socket_info_error(string_format("Error setting socket %d as non-blocking\n", fd).c_str());

            return -1;
        }
    }

    return fd;
}

void SockOps::socket_transparent(int fd, int family) {

    int n = 1;

    if(family == AF_INET) {
        if (n = 1; 0 != ::setsockopt(fd, SOL_IP, IP_TRANSPARENT, &n, sizeof(int))) {
            throw socket_info_error(string_format("cannot set socket %d option IP_TRANSPARENT\n", fd).c_str());
        }
    }
    else if (family == AF_INET6) {
        if (n = 1; 0 != ::setsockopt(fd, SOL_IPV6, IPV6_TRANSPARENT, &n, sizeof(int))) {
            throw socket_info_error(string_format("cannot set socket %d option IPV6_TRANSPARENT\n", fd).c_str());
        }
    }
    else {
        throw socket_info_error("cannot set transparency for unknown family");
    }
}

int SocketInfo::create_socket_left(int l4_proto) {

    int fd_left = SockOps::socket_create(src.family, l4_proto, 0);
    SockOps::socket_transparent(fd_left, src.family);

    src.pack();
    dst.pack();

    auto plug_socket = [&](int fd, sockaddr* bind_ss, sockaddr* connect_ss) {

        static std::mutex like_a_barrier;

        constexpr const char* bind_connect_race_hack_iface = "lo";
        constexpr size_t bcrhi_sz = 2;

        bool is_six = connect_ss->sa_family == AF_INET6;

        if(not is_six) {
            if (0 != ::setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, bind_connect_race_hack_iface, bcrhi_sz)) {
                throw socket_info_error("cannot bind to device - bind-connect races may occur");
            }
        }

        {
            // don't allow internal race condition between binding to server address and connecting to originator.
            // Note this is reverse direction "connection", so it's probably quite common binding to public DNS IP/port.

            // Also note this won't prevent system-wide binding race condition, ie in case of multi-tenant configurations.

            auto l_ = std::scoped_lock(like_a_barrier);
            if (::bind(fd, bind_ss, sizeof(struct sockaddr_storage))) {
                throw socket_info_error(
                        string_format("cannot bind socket %d to %s:%d - %s", fd, dst.str_host.c_str(), dst.port,
                                      string_error().c_str()).c_str());
            }

            if (::connect(fd, connect_ss, sizeof(struct sockaddr_storage))) {
                throw socket_info_error(
                        string_format("cannot connect socket %d to %s:%d - %s", fd, src.str_host.c_str(), src.port,
                                      string_error().c_str()).c_str());
            }
        }

        if(not is_six) {
            if (0 != ::setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, "", 0)) {
                throw socket_info_error("cannot bind to 'any' device - socket inoperable");
            }
        }
    };

    plug_socket(fd_left, (sockaddr *) &dst.ss.value(), (sockaddr *) &src.ss.value());

    return fd_left;
}



int SockOps::ss_address_unpack(const sockaddr_storage *ptr, std::string* dst, unsigned short* port) {

    constexpr size_t buf_sz = 64;

    char b[buf_sz]; memset(b,0,buf_sz);
    int family = ptr->ss_family;
    unsigned short val_port = 0;

    if(family == AF_INET6) {
        inet_ntop(ptr->ss_family,&(((struct sockaddr_in6*) ptr)->sin6_addr),b,buf_sz);
        val_port = ((struct sockaddr_in6*) ptr)->sin6_port;
    }
    else if(family == AF_INET) {
        inet_ntop(ptr->ss_family,&(((struct sockaddr_in*) ptr)->sin_addr),b,buf_sz);
        val_port = ((struct sockaddr_in*) ptr)->sin_port;
    }

    std::string mapped4_temp = b;
    if(mapped4_temp.find("::ffff:") == 0) {
        mapped4_temp = mapped4_temp.substr(7);
        family = AF_INET;
    }

    if(dst != nullptr) {
        // function can be useful just to detect mapped IP
        dst->assign(mapped4_temp);
    }
    if(port != nullptr) {
        *port = ntohs(val_port);
    }
    return family;
}


int SockOps::ss_address_remap(const sockaddr_storage *orig, sockaddr_storage* mapped) {
    std::string ip_part;
    unsigned short port_part;

    int fa = ss_address_unpack(orig,&ip_part,&port_part);

    if(fa == AF_INET) {
        inet_pton(fa,ip_part.c_str(),&((struct sockaddr_in*)mapped)->sin_addr);
        ((struct sockaddr_in*)mapped)->sin_port = htons(port_part);
        mapped->ss_family = fa;
    }
    else if(fa == AF_INET6) {
        inet_pton(fa,ip_part.c_str(),&((struct sockaddr_in6*)mapped)->sin6_addr);
        ((struct sockaddr_in6*)mapped)->sin6_port = htons(port_part);
        mapped->ss_family = fa;
    }

    return fa;
}

std::string SockOps::ss_str(const sockaddr_storage *s) {
    std::string ip;
    unsigned short port;

    int fa = ss_address_unpack(s,&ip,&port);

    return string_format("%s/%s:%d", SockOps::family_str(fa).c_str(),ip.c_str(),port);
}