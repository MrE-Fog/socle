/*
    Socle Library Ecosystem
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

#include "hostcx.hpp"
#include "log/logger.hpp"
#include "display.hpp"
#include "crc32.hpp"
#include "iproxy.hpp"

bool baseHostCX::socket_in_name = false;
bool baseHostCX::online_name = false;


namespace std
{
    size_t hash<Host>::operator()(const Host& h) const
        {
            const std::string hs = h.chost();
            const std::string hp = h.cport();
            // Compute individual hash values for two data members and combine them using XOR and bit shifting
            return ((hash<string>()(hs) ^ (hash<string>()(hp) << 1)) >> 1);
        }
}

bool operator==(const Host& h, const Host& hh) {
    std::string s = h.chost() + ":" + h.cport();
    std::string ss = hh.chost() + ":" + hh.cport();

    return s == ss;
}


baseHostCX::baseHostCX(baseCom* c, const char* h, const char* p): Host(h, p) {

    permanent_ = false;
    last_reconnect_ = 0;
    reconnect_delay_ = 7;
    fds_ = 0;
    error_ = false;

    writebuf_ = lockbuffer(HOSTCX_BUFFSIZE);
    writebuf_.clear();

    readbuf_ = lockbuffer(HOSTCX_BUFFSIZE);
    readbuf_.clear();
    processed_bytes_ = 0;
    next_read_limit_ = 0;
    auto_finish_ = true;
    read_waiting_for_peercom_ = false;
    write_waiting_for_peercom_ = false;

    meter_read_count = 0;
    meter_write_count = 0;
    meter_read_bytes = 0;
    meter_write_bytes = 0;

    //whenever we initialize object with socket, we will be already opening!
    opening(true);

    if(!c) {
        throw socle::com_is_null();
    }

    com_ = c;
    com()->init(this);
}

baseHostCX::baseHostCX(baseCom* c, int s) {

    permanent_ = false;
    last_reconnect_ = 0;
    reconnect_delay_ = 7;
    fds_ = s;
    error_ = false;

    writebuf_ = lockbuffer(HOSTCX_BUFFSIZE);
    writebuf_.clear();

    readbuf_ = lockbuffer(HOSTCX_BUFFSIZE);
    readbuf_.clear();
    processed_bytes_ = 0;
    next_read_limit_ = 0;
    auto_finish_ = true;
    read_waiting_for_peercom_ = false;
    write_waiting_for_peercom_ = false;

    meter_read_count = 0;
    meter_write_count = 0;
    meter_read_bytes = 0;
    meter_write_bytes = 0;

    //whenever we initialize object with socket, we will be already opening!
    opening(true);

    if(!c) {
        throw socle::com_is_null();
    }

    com_ = c;
    com()->init(this);
}

baseHostCX::~baseHostCX() {
    com()->cleanup();

    if(fds_ > 0) {
        com()->set_poll_handler(fds_,nullptr);
        com()->close(fds_);
    }

    if(closing_fds_ > 0) {
        com()->set_poll_handler(closing_fds_,nullptr);
        com()->close(closing_fds_);
    }
    delete com_;
}


int baseHostCX::connect() {

    if(! com()) {
        return -1;
    }

    opening(true);

    _deb("HostCX::connect[%s]: blocking=%d",c_name(), com()->GLOBAL_IO_BLOCKING());
    fds_ = com()->connect(host_.c_str(),port_.c_str());
    error_ = false;

    if (fds_ > 0 && com()->GLOBAL_IO_BLOCKING()) {
        _deb("HostCX::connect[%s]: blocking, connected successfully, socket %d",c_name(),fds_);
        opening(false);
    }
    else if (com()->GLOBAL_IO_BLOCKING()) {
        _deb("HostCX::connect[%s]: blocking, failed!",c_name());
        opening(false);
    }

    return fds_;
}


bool baseHostCX::opening_timeout() {

    if (!opening()) {
        _dum("baseHostCX::opening_timeout: already opened");
        return false;
    } else {
        time_t now = time(nullptr);
        if (now - t_connected > reconnect_delay()) {
            _dia("opening_timeout: timeout!");
            return true;
        }
    }

    return false;
}


bool baseHostCX::idle_timeout() {
    time_t now = time(nullptr);
    if (now - w_activity > idle_delay() && now - w_activity) {
        _dia("baseHostCX::idle_timeout: timeout");
        return true;
    }

    return false;
}


bool baseHostCX::read_waiting_for_peercom () {

    if(read_waiting_for_peercom_ && peercom()) {
        if(peercom()->com_status()) {
            _dia("baseHostCX::read_waiting_for_peercom: peer's com status is OK, un-pausing");
            read_waiting_for_peercom(false);
        }
    }
    else if(read_waiting_for_peercom_) {
        // peer() == NULL !
        _dum("baseHostCX::read_waiting_for_peercom: no peer set => no peer to wait for => manual mode");
    }

    return read_waiting_for_peercom_;
}

bool baseHostCX::write_waiting_for_peercom () {

    if(write_waiting_for_peercom_ && peercom()) {
        if(peercom()->com_status()) {
            _dia("baseHostCX::write_waiting_for_peercom[%s]: peer's com status ok, un-pausing write", c_name());
            write_waiting_for_peercom(false);
        }
    }
    else if(write_waiting_for_peercom_) {
        // peer() == NULL !
        _dum("baseHostCX::write_waiting_for_peercom: no peer set => no peer to wait for => manual mode");
    }

    return write_waiting_for_peercom_;
}



bool baseHostCX::is_connected() {
    bool status = com()->is_connected(socket());
    _dia("baseHostCX::is_connected[%s]: getsockopt(%d,SOL_SOCKET,SO_ERROR,..,..) reply %d", c_name(), socket(), status);

    return status;
}

void baseHostCX::shutdown() {

    parent_proxy(nullptr, '-');

    if(fds_ != 0) {
        com()->shutdown(fds_);
        _deb("baseHostCX::shutdown[%s]: socket shutdown",c_name());
        closing_fds_ = fds_;
        fds_ = 0;

        if(com()) {
            com()->master()->unset_monitor(com()->translate_socket(closing_fds_));
        }
    } else {
        _deb("baseHostCX::shutdown[%s]: no-op, cannot be shutdown",c_name());
    }
}

std::string& baseHostCX::name(bool force) const {

    if(name__.empty() || online_name || force) {

        std::scoped_lock<std::mutex> l(name_mutex_);

        if (reduced()) {
            std::string com_name = "?";
            if(com() != nullptr) {
                com_name = com()->name();
            }

            std::string res_host;
            std::string res_port;

            if (valid()) {

                if(com() != nullptr) {
                    com()->resolve_socket_src(fds_, &res_host, &res_port);
                    host(res_host);
                    port(res_port);
                }

                if(socket_in_name) {
                    name__ = string_format("%d::%s_%s:%s",socket(), com()->shortname().c_str() , chost().c_str(),cport().c_str());
                } else {
                    name__ = string_format("%s_%s:%s",com()->shortname().c_str() , chost().c_str(),cport().c_str());
                }

                //name__ = string_format("%d:<reduced>",socket());
            }
            else {
                name__ = std::string("?:<reduced>");
            }

        } else {

            if(socket_in_name) {
                name__ = string_format("%d::%s_%s:%s",socket(), com()->shortname().c_str() ,chost().c_str(),cport().c_str());
            } else {
                name__ = string_format("%s_%s:%s",com()->shortname().c_str() ,chost().c_str(),cport().c_str());
            }
        }
    }

    return name__;
}


const char* baseHostCX::c_name() const {
    name();
    return name__.c_str();
}

bool baseHostCX::reconnect(int delay) {

    if (should_reconnect_now() and permanent()) {
        shutdown();
        connect();

        _deb("baseHostCX::reconnect[%s]: reconnect attempt (previous at %u)",c_name(),last_reconnect_);
        last_reconnect_ = time(nullptr);

        return true;
    }
    else if (!permanent()) {
        _not("baseHostCX::reconnect: attempt to reconnect non-permanent CX: %s",c_name());
        return false;
    }
    else if (reduced() ) {
        _err("baseHostCX::reconnect[%s]: reconnecting reduced CX is not possible",c_name());
        last_reconnect_ = time(nullptr);
        return false;
    }


    return false;
}

int baseHostCX::read() {

    if(io_disabled()) {
        _war("io is disabled, but read() called");
    }

    if(read_waiting_for_peercom()) {
        _dum("baseHostCX::read[%s]: read operation is waiting_for_peercom, returning -1",c_name());
        return -1;
    }

    if(peer() && peer()->writebuf()->size() > 200000) {
        _deb("baseHostCX::read[%d]: deferring read operation",socket());
        com()->rescan_read(socket());
        return -1;
    }

    buffer_guard bg(readbuf());


    _dum("HostCX::read[%s]: calling pre_read",c_name());
    pre_read();

    if(next_read_limit_ < 0) {
        next_read_limit_ = 0;
        return -1;
    }

    _dum("HostCX::read[%s]: readbuf_ size=%d, capacity=%d, previously processed=%d finished",c_name(),
            readbuf_.size(), readbuf_.capacity(), processed_bytes_);

    if (auto_finish()) {
        finish();
    }

    ssize_t l = 0;

    while(true) {

        // append-like behavior: append to the end of the buffer, don't exceed max. capacity!
        void *cur_read_ptr = &(readbuf_.data()[readbuf_.size()]);

        // read only amount of bytes fitting the buffer capacity
        ssize_t cur_read_max = readbuf_.capacity()-readbuf_.size();

        if (cur_read_max + l > next_read_limit() && next_read_limit() > 0) {
            _deb("HostCX::read[%s]: read buffer limiter: %d",c_name(), next_read_limit() - l);
            cur_read_max = next_read_limit() - l;
        }

        _ext("HostCX::read[%s]: readbuf_ base=%x, wr at=%x, maximum to write=%d", c_name(),
                readbuf_.data(), cur_read_ptr,cur_read_max);


        //read on last position in buffer
        int cur_l = com()->read(socket(), cur_read_ptr, cur_read_max, 0);

        // no data to read!
        if(cur_l < 0) {

            // if this is first attempt, l is still zero. Fix it.
            if(l == 0) {
                l = -1;
            }
            break;
        }
        else if(cur_l == 0) {
            _dia("baseHostCX::read[%s]: error while reading. %d bytes read.", c_name(), l);
            error(true);

            break;
        }


        // change size of the buffer accordingly
        readbuf_.size(readbuf_.size()+cur_l);

        //increment read counter
        l += cur_l;

        if(next_read_limit_ > 0 &&  l >= next_read_limit_) {
            _dia("baseHostCX::read[%s]: read limiter hit on %d bytes.", c_name(), l);
            break;
        }

        // in case next_read_limit_ is large and we read less bytes than it, we need to decrement also next_read_limit_

        next_read_limit_ -= cur_l;

        // if buffer is full, let's reallocate it and try read again (to save system resources)

        // testing break
        // break;

        if(readbuf_.size() >= readbuf_.capacity()) {
            _dia("baseHostCX::read[%s]: read buffer reached it's current capacity %d/%d bytes", c_name(),
                    readbuf_.size(), readbuf_.capacity());

            if(readbuf_.capacity() * 2  <= HOSTCX_BUFFMAXSIZE) {

                if (readbuf_.capacity(readbuf_.capacity() * 2)) {
                    _dia("baseHostCX::read[%s]: read buffer resized capacity %d/%d bytes", c_name(),
                            readbuf_.size(), readbuf_.capacity());

                } else {
                    _not("baseHostCX::read[%s]: memory tension: read buffer cannot be resized!", c_name());
                }
            }
            else {
                _dia("baseHostCX::read[%s]: buffer already reached it's maximum capacity.", c_name());
            }
        }

        // reaching code here means that we don't want other iterations
        break;

    }

    if (l > 0) {

        meter_read_bytes += l;
        meter_read_count++;
        time(&r_activity);

        // claim opening socket already opened
        if (opening()) {
            _dia("baseHostCX::read[%s]: connection established", c_name());
            opening(false);
        }



        _ext("baseHostCX::read[%s]: readbuf_ read %d bytes", c_name(), l);

        processed_bytes_ = process_();
        _deb("baseHostCX::read[%s]: readbuf_ read %d bytes, process()-ed %d bytes, incomplete readbuf_ %d bytes",
                c_name(), l, processed_bytes_, l - processed_bytes_);


        // data are already processed
        _deb("baseHostCX::read[%s]: calling post_read",c_name());
        post_read();

        if(com()->debug_log_data_crc) {
            _deb("baseHostCX::read[%s]: after: buffer crc = %X", c_name(),
                     socle_crc32(0, readbuf()->data(), readbuf()->size()));
        }

    } else if (l == 0) {
        _dia("baseHostCX::read[%s]: error while reading", c_name());
        error(true);
    } else {
        processed_bytes_ = 0;
    }

    // before return, don't forget to reset read limiter
    next_read_limit_ = 0;

    return l;
}

void baseHostCX::pre_read() {
}

void baseHostCX::post_read() {
}

int baseHostCX::write() {

    if(io_disabled()) {
        _war("io is disabled, but write() called");
    }


    if(write_waiting_for_peercom()) {
        _deb("baseHostCX::write[%s]: write operation is waiting_for_peercom, returning 0", c_name());
        return 0;
    }

    buffer_guard bg(writebuf());


    int tx_size_orig = writebuf_.size();
    pre_write();

    int tx_size = writebuf_.size();

    if (tx_size != tx_size_orig) {
        _deb("baseHostCX::write[%s]: calling pre_write modified data, size %d -> %d",c_name(),tx_size_orig,tx_size);
    }

    if (tx_size <= 0) {
        _deb("baseHostCX::write[%s]: writebuf_ %d bytes pending %s", c_name(), tx_size, opening() ? "(opening)" : "");
        // return 0; // changed @ 20.9.2014 by astib.
        // Let com() decide what to do if we want to send 0 (or less :) bytes
        // keep it here for studying purposes.
        // For example, if we stop here, no SSL_connect won't happen!
    }
    else {
        _deb("baseHostCX::write[%s]: writebuf_ %d bytes pending",c_name(),tx_size);
    }

    ssize_t l = com()->write(socket(), writebuf_.data(), tx_size, MSG_NOSIGNAL);

    if (l > 0) {
        meter_write_bytes += l;
        meter_write_count++;
        time(&w_activity);

        if (opening()) {
            _deb("baseHostCX::write[%s]: connection established", c_name());
            opening(false);
        }
        _deb("baseHostCX::write[%s]: %d from %d bytes sent from tx buffer at %x", c_name(), l, tx_size, writebuf_.data());
        if (l < tx_size) {
            // rather log this: not a big deal, but we couldn't have sent all data!
            _dia("baseHostCX::write[%s]: only %d from %d bytes sent from tx buffer!", c_name(), l, tx_size);
        }

        _dum("baseHostCX::write[%s]: calling post_write", c_name());
        post_write();

        if(l < static_cast<ssize_t>(writebuf_.size())) {
            _dia("baseHostCX::write[%s]: %d bytes written out of %d -> setting socket write monitor",
                    c_name(), l, writebuf_.size());
            // we need to check once more when socket is fully writable

            com()->set_write_monitor(socket());
            //com()->rescan_write(socket());
            rescan_out_flag_ = true;

        } else {
            // write buffer is empty
            if(rescan_out_flag_) {
                rescan_out_flag_ = false;

                // stop monitoring write which results in loop an unnecesary write() calls
                com()->change_monitor(socket(), EPOLLIN);
            }
        }

        writebuf_.flush(l);

        if(com()->debug_log_data_crc) {
            _deb("baseHostCX::write[%s]: after: buffer crc = %X", c_name(),
                    socle_crc32(0, writebuf()->data(), writebuf()->size()));
        }

        if(close_after_write() && writebuf()->size() == 0) {
            shutdown();
        }
    }
    else if(l == 0 && writebuf()->size() > 0) {
        // write unsuccessful, we have to try immediately socket is writable!
        _dia("baseHostCX::write[%s]: %d bytes written out of %d -> setting socket write monitor",
                c_name(), l, writebuf_.size());
        //com()->set_write_monitor(socket());

        // write was not successful, wait a while
        com()->rescan_write(socket());
        rescan_out_flag_ = true;
    }
    else if(l < 0) {
        _dia("baseHostCX::write[%s] write failed: %s, unrecoverable.", c_name(), string_error().c_str());
    }

    return l;
}


void baseHostCX::pre_write() {
}


void baseHostCX::post_write() {
}

int baseHostCX::process() {
    return readbuf()->size();
}


ssize_t baseHostCX::finish() {
    if( readbuf()->size() >= (unsigned int)processed_bytes_ && processed_bytes_ > 0) {
        _deb("baseHostCX::finish[%s]: flushing %d bytes in readbuf_ size %d", c_name(), processed_bytes_, readbuf()->size());
        readbuf()->flush(processed_bytes_);
        return processed_bytes_;
    } else if (readbuf()->empty()) {
        _dum("baseHostCX::finish[%s]: already flushed",c_name());
        return 0;
    } else {
        _war("baseHostCX::finish[%s]: attempt to flush more data than in buffer", c_name());
        _war("baseHostCX::finish[%s]: best-effort recovery: flushing all", c_name());
        auto s = readbuf()->size();
        readbuf()->flush(s);
        return s;
    }
}

buffer baseHostCX::to_read() {
    _deb("baseHostCX::to_read[%s]: returning buffer::view for %d bytes", c_name(), processed_bytes_);
    return readbuf()->view(0,processed_bytes_);
}

void baseHostCX::to_write(buffer b) {
    writebuf_.append(b);
    com()->set_write_monitor(socket());
    _deb("baseHostCX::to_write(buf)[%s]: appending %d bytes, buffer size now %d bytes", c_name(), b.size(), writebuf_.size());
}

void baseHostCX::to_write(const std::string& s) {

    writebuf_.append((unsigned char*)s.data(), s.size());
    com()->set_write_monitor(socket());
    _deb("baseHostCX::to_write(ptr)[%s]: appending %d bytes, buffer size now %d bytes", c_name(), s.size(), writebuf_.size());

}

void baseHostCX::to_write(unsigned char* c, unsigned int l) {
    writebuf_.append(c,l);
    com()->set_write_monitor(socket());
    _deb("baseHostCX::to_write(ptr)[%s]: appending %d bytes, buffer size now %d bytes", c_name(), l, writebuf_.size());
}

void baseHostCX::on_accept_socket(int fd) {
    com()->accept_socket(fd);

    if(reduced()) {
        com()->resolve_socket_src(fd, &host_,&port_);
    }
}

void baseHostCX::on_delay_socket(int fd) {
    com()->delay_socket(fd);
}

std::string baseHostCX::to_string(int verbosity) const {

    std::stringstream r_str;

    r_str << name();

    if(verbosity > INF) {
        r_str << string_format(" | fd=%d | rx_cnt=%d rx_b=%d / tx_cnt=%d tx_b=%d",
                               com() ? com()->translate_socket(socket()) : socket(),
                               meter_read_count, meter_read_bytes,
                               meter_write_count, meter_write_bytes);
    }
    return r_str.str();
}

std::string baseHostCX::full_name(unsigned char side) {
    std::string self = host();
    std::string self_p = port();
    int self_s = socket();

    std::string  self_ss;
    if(socket_in_name) self_ss  = string_format("::%d:", self_s);

    std::string self_c;
    if (com())  self_c = com()->shortname();

    std::string  peeer = "?";
    std::string  peeer_p = "?";
    int          peeer_s = 0;
    std::string  peeer_c = "?";
    std::string  peeer_ss;

    if (peer()) {
        peeer =  peer()->host();
        peeer_p =  peer()->port();
        peeer_s = peer()->socket();
        if(socket_in_name) peeer_ss  = string_format("::%d:", peeer_s);

        if (peer()->com()) {
            peeer_c = peer()->com()->shortname();
        }

    } else {
        return string_format("%s_%s%s:%s", self_c.c_str(), self_ss.c_str(), self.c_str(), self_p.c_str());
    }

    if ( (side == 'l') || ( side == 'L') ) {
        return string_format("%s_%s%s:%s to %s_%s%s:%s", self_c.c_str(), self_ss.c_str(), self.c_str(), self_p.c_str(),
                             peeer_c.c_str(), peeer_ss.c_str(), peeer.c_str(), peeer_p.c_str());
    }

    //else
    return string_format("%s_%s%s:%s to %s_%s%s:%s", peeer_c.c_str(), peeer_ss.c_str(), peeer.c_str(), peeer_p.c_str(),
                                                        self_c.c_str(), self_ss.c_str(), self, self_p.c_str());

}

