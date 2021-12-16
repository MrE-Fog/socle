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

#ifndef PCAPLOG_HPP
#define PCAPLOG_HPP

#include <baseproxy.hpp>

#include <socketinfo.hpp>
#include <traflog/pcapapi.hpp>
#include <traflog/basetraflog.hpp>
#include <traflog/fsoutput.hpp>
#include <traflog/threadedpoolwriter.hpp>

#include <memory>

using namespace socle::pcap;

namespace socle::traflog {

    int raw_socket_gre(int family);

    class PcapLog : public baseTrafficLogger {

        PcapLog() = default; // just for singleton, which is set up later
    public:
        explicit PcapLog (baseProxy *parent, const char* d_dir, const char* f_prefix, const char* f_suffix, bool create_dirs);
        ~PcapLog() override;

        bool prepare_file();

        std::optional<std::function<void(connection_details const&, buffer const&)>> ip_packet_hook;

        void write_pcap_header(bool is_recreated);

        void write_tcp_start(tcp_details& real_details);
        void write_tcp_data(side_t side, buffer const& b, tcp_details& real_details);

        void write_udp_data(side_t side, buffer const& b, tcp_details& real_details);

        void write(side_t side, const buffer &b) override;
        void write(side_t side, std::string const& s) override;

        baseProxy *parent = nullptr;
        tcp_details details;

        static const bool use_pool_writer = true;
        baseFileWriter* writer_ = nullptr;
        void init_writer();

        FsOutput FS;
        mutable std::mutex fs_lock_;

        bool single_only = false;             // write using single-file instance?
        std::atomic_bool pcap_header_written = false; // is PCAP file initialized (opened and preamble written)?
        bool tcp_start_written = false;   // if TCP, is SYNs written, so rest is just data?

        long long stat_bytes_written = 0LL;
        long long stat_bytes_quota = 0LL;
        bool rotate_now = false;

        bool comment_frame(pcapng::pcapng_epb& frame);
        std::string comlog;

        static PcapLog& single_instance() {
            static PcapLog s;
            return s;
        }

        logan_lite log {"socle.pcaplog"};
        logan_lite log_write {"socle.pcaplog.write"};

    };



    struct GreExporter {
        bool operator()(connection_details const& det, buffer const& buf) {

            if(sock < 0) sock = traflog::raw_socket_gre(target.dst_family);

            if(not target.dst_ss) return false;
            if(sock < 0) return false;

            buffer s(buf.size() + sizeof(grehdr));
            pcapng::append_GRE_header(s, det);

            s.append(buf);

            int r = sendto(sock, s.data(), s.size(), 0, (sockaddr*)&target.dst_ss.value(), sizeof(sockaddr_storage));
            if(r <= 0) {
                return false;
            }

            return true;
        }

        GreExporter(int family, std::string_view host) {
            target.dst_family = family;
            target.str_dst_host = host;
            target.pack_dst_ss();
        }
        GreExporter(GreExporter const& other) : target(other.target), sock(-1) {};
        GreExporter(GreExporter&& other) noexcept : target(std::move(other.target)), sock(other.sock) { other.sock = -1; };

        GreExporter& operator=(GreExporter const& other) {
            if(&other != this) {
                target = other.target;
                sock = -1;
            }

            return *this;
        };
        GreExporter& operator=(GreExporter&& other) noexcept {
            if(&other != this) {
                target = std::move(other.target);
                sock = other.sock;

                other.sock = -1;
            }

            return *this;
        };


        ~GreExporter() { if(sock > 0) ::close(sock); }

    private:
        SocketInfo target{};
        int sock {-1};
    };

}

#endif //PCAPLOG_HPP
