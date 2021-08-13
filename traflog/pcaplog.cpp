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

#include <traflog/pcaplog.hpp>
#include <traflog/filewriter.hpp>
#include <xorshift.hpp>

namespace socle::traflog {

    PcapLog::PcapLog (baseProxy *parent, const char* d_dir, const char* f_prefix, const char* f_suffix, bool create_dirs) :
        parent(parent),
        FS(parent, d_dir, f_prefix, f_suffix, create_dirs) {

        if(not parent or not parent->com()) {
            _war("pcaplog::ctor: parent or parent com is nullptr");
            parent = nullptr; return;
        }

        auto &ls = parent->ls().empty() ? parent->lda() : parent->ls();
        auto &rs = parent->rs().empty() ? parent->rda() : parent->rs();

        if (ls.empty() or rs.empty()) {
            _war("pcaplog::ctor: ls or rs is empty");
            parent = nullptr; return;
        }

        SocketInfo s;
        s.str_src_host = ls[0]->host();
        s.str_dst_host = rs[0]->host();
        s.sport = safe_val(ls[0]->port(), 0);
        s.dport = safe_val(rs[0]->port(), 0);
        s.pack_dst_ss();
        s.pack_src_ss();

        if(not s.src_ss.has_value()) {
            _war("pcaplog::ctor: src info not created");
            return;
        } else {
            details.source = s.src_ss.value();
            _deb("pcaplog::ctor: src info: %s", s.src_ss_str().c_str());
        }



        if(not s.src_ss.has_value()) {
            _war("pcaplog::ctor: dst info not created");
            return;
        }
        else {
            _deb("pcaplog::ctor: dst info: %s", s.dst_ss_str().c_str());
            details.destination = s.dst_ss.value();
        }

        // this could become more complex in the (probably far) future
        parent->com()->l3_proto() == AF_INET6 ? details.ip_version = 6
                                              : details.ip_version = 4;

        parent->com()->l4_proto() == SOCK_STREAM ? details.next_proto = connection_details::TCP
                                                 : details.next_proto = connection_details::UDP;

        // some tcp specific values
        if(details.next_proto == connection_details::TCP) {
            details.seq_in = xorshift::rand();
            details.seq_out = xorshift::rand();
        }

        init_writer();
    }

    PcapLog::~PcapLog() {
        if(writer_) {
            if(details.next_proto == connection_details::TCP) {
                if(tcp_start_written) {
                    PcapLog* self = this;
                    if(single_only) self = &single_instance();
                    auto* writer = self->writer_;
                    auto const& fs = self->FS;

                    buffer out;
                    pcapng::pcapng_epb f1;
                    if(comment_frame(f1)) { _deb("comment inserted on close"); };

                    f1.append_TCP(out, "", 0, 0, TCPFLAG_FIN|TCPFLAG_ACK, details);
                    pcapng::pcapng_epb f2;
                    f1.append_TCP(out, "", 0, 1, TCPFLAG_FIN|TCPFLAG_ACK, details);
                    writer->write(fs.filename_full, out);
                }
            }
            writer_->flush(FS.filename_full);
            writer_->close(FS.filename_full);

            // do not delete threaded pool writer
            if(not use_pool_writer)
                delete writer_;
        }
    }

    void PcapLog::init_writer () {
        if(!use_pool_writer) {
            writer_ = new fileWriter();
        } else {
            writer_ = threadedPoolFileWriter::instance();
        }
    }


    bool PcapLog::prepare_file() {

        PcapLog *self = this;
        if (single_only) self = &single_instance();

        auto l_ = std::lock_guard(self->fs_lock_);
        auto *writer = self->writer_;
        auto const &fs = self->FS;

        // rotating logs is only possible for pcap_single mode
        if (single_only) {

            // don't allow rotating insanely low sizes (<10MiB). If 10MiB is too big for somebody, let me know.
            if (self->stat_bytes_quota > 0 and self->stat_bytes_quota <= 10000000LL) {
                self->stat_bytes_quota = 10000000LL;
            }


            if ((self->stat_bytes_quota > 0LL and self->stat_bytes_written >= self->stat_bytes_quota) or
                self->rotate_now) {

                _dia("pcaplog::write: rotating based on %s", self->rotate_now ? "request" : "quota limit");
                self->rotate_now = false;

                std::stringstream ss;
                ss << self->FS.data_dir << "/" << self->FS.file_prefix << "smithproxy.old." << self->FS.file_suffix;
                std::string renamed_fnm = ss.str();

                struct stat st{};
                int result = stat(renamed_fnm.c_str(), &st);
                auto file_exists = result == 0;

                _deb("pcaplog::write: old file %s exist", file_exists ? "does" : "doesn't");

                if (file_exists) {
                    auto ret = ::remove(renamed_fnm.c_str());
                    if (ret == 0) {
                        _deb("pcaplog::write: old file deleted");

                        bool closed = self->writer_->close(fs.filename_full);
                        _deb("pcaplog::write: current file closed: %s", closed ? "yes" : "no");

                        if (::rename(fs.filename_full.c_str(), renamed_fnm.c_str()) == 0) {
                            _dia("pcaplog::write: moving current file to backup: ok");
                            self->FS.filename_full = self->FS.generate_filename_single("smithproxy", true);
                            self->stat_bytes_written = 0LL;
                        } else {
                            _err("pcaplog::write: moving current file to backup: %s", string_error().c_str());
                        }
                    } else {
                        _err("pcaplog::write: old file not deleted: %s", string_error().c_str());
                    }
                } else {
                    bool closed = self->writer_->close(fs.filename_full);
                    _deb("pcaplog::write: current file closed: %s", closed ? "yes" : "no");

                    if (::rename(fs.filename_full.c_str(), renamed_fnm.c_str()) == 0) {
                        _dia("pcaplog::write: moving current file to backup: ok");
                        self->FS.filename_full = self->FS.generate_filename_single("smithproxy", true);
                        self->stat_bytes_written = 0LL;
                    } else {
                        _err("pcaplog::write: moving current file to backup: %s", string_error().c_str());
                    }
                }
            }
        }

        if(writer->recreate(fs.filename_full)) {
            auto fd = creat(fs.filename_full.c_str(),O_CREAT|O_WRONLY|O_TRUNC);
            if(fd >= 0) {
                _not("new file %s created", fs.filename_full.c_str());
                ::close(fd);
            }

            return true;
        }

        return false;
    }

    bool PcapLog::comment_frame(pcapng::pcapng_epb& frame) {
        if(not comlog.empty()) {
            frame.comment(comlog);
            comlog.clear();

            return true;
        }

        return false;
    }

    void PcapLog::write (side_t side, const buffer &b) {
        PcapLog *self = this;
        if (single_only) self = &single_instance();

        if (not self->writer_) self->init_writer();

        auto *writer = self->writer_;
        auto const &fs = self->FS;


        if (not writer->opened()) {
            if (writer->open(fs.filename_full)) {
                _dia("writer '%s' created", fs.filename_full.c_str());
            } else {
                _err("write '%s' failed to open dump file!", fs.filename_full.c_str());
            }
        }

        if (not writer->opened()) return;

        bool is_recreated = prepare_file();

        // reset bytes written
        if(is_recreated) {
            _err("pcaplog::write: current file recreated");
            self->stat_bytes_written = 0LL;
        }

        // PCAP HEADER
        if(not self->pcap_header_written or is_recreated) {
            buffer out;
            pcapng::pcapng_shb mag;
            mag.append(out);

            // interface block
            pcapng::pcapng_ifb hdr;
            hdr.append(out);

            _deb("pcaplog::write[%s]/magic+ifb : about to write %dB", fs.filename_full.c_str(), out.size());
            _dum("pcaplog::write[%s]/magic+ifb : \r\n%s", fs.filename_full.c_str(), hex_dump(out, 4, 0, true).c_str());
            auto wr = writer->write(fs.filename_full, out);
            _dia("pcaplog::write[%s]/magic+ifb : written %dB", fs.filename_full.c_str(), wr);

            self->stat_bytes_written += wr;

            self->pcap_header_written = true;

            // don't write tcp handshake if the stream was recreated in the meantime
            if(is_recreated) tcp_start_written = true;
        }

        // TCP handshake

        if(details.next_proto == connection_details::TCP) {

            auto& log = log_write;

            if (not tcp_start_written) {
                buffer out;

                pcapng::pcapng_epb syn1;
                syn1.append_TCP(out,"", 0, 0, TCPFLAG_SYN, details);

                pcapng::pcapng_epb syn_ack;
                syn_ack.append_TCP(out, "", 0, 1, TCPFLAG_SYN|TCPFLAG_ACK, details);

                pcapng::pcapng_epb ack;
                ack.append_TCP(out, "", 0, 0, TCPFLAG_ACK, details);

                _deb("pcaplog::write[%s]/tcp-hs : about to write %dB", fs.filename_full.c_str(), out.size());
                _dum("pcaplog::write[%s]/tcp-hs : \r\n%s", fs.filename_full.c_str(), hex_dump(out, 4, 0, true).c_str());

                auto wr = writer->write(fs.filename_full, out);
                writer->flush(fs.filename_full);

                self->stat_bytes_written += wr;

                _dia("pcaplog::write[%s]/tcp-hs : written %dB", fs.filename_full.c_str(), wr);

                tcp_start_written = true;
            }

            buffer out;
            pcapng::pcapng_epb data;
            if(comment_frame(data)) { _dia("comment inserted into data"); };


            data.append_TCP(out, (const char*)b.data(), b.size(), side == side_t::RIGHT, TCPFLAG_ACK, details);

            _deb("pcaplog::write[%s]/tcp-data : about to write %dB", fs.filename_full.c_str(), out.size());
            _dum("pcaplog::write[%s]/tcp-data : \r\n%s", fs.filename_full.c_str(), hex_dump(out, 4, 0, true).c_str());

            auto wr = writer->write(fs.filename_full, out);
            _dia("pcaplog::write[%s]/tcp-data : written %dB", fs.filename_full.c_str(), wr);

            self->stat_bytes_written += wr;
        }
        else {
            auto& log = log_write;

            buffer out;

            pcapng::pcapng_epb u1;
            if(comment_frame(u1)) { _dia("comment inserted"); };
            u1.append_UDP(out, (const char*)b.data(), b.size(), side == side_t::RIGHT, details);

            _deb("pcaplog::write[%s]/udp : about to write %dB", fs.filename_full.c_str(), out.size());
            _dum("pcaplog::write[%s]/tcp : \r\n%s", fs.filename_full.c_str(), hex_dump(out, 4, 0, true).c_str());

            auto wr = writer->write(fs.filename_full, out);
            _dia("pcaplog::write[%s]/udp : written %dB", fs.filename_full.c_str(), wr);
            self->stat_bytes_written += wr;
        }

    }
    void PcapLog::write (side_t side, const std::string &s) {
        comlog.append(s);
    }
}