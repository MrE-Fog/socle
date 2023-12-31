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

#include <vector>
#include <string>
#include <unistd.h>
#include <sys/socket.h>

#include <cstring>
#include <cerrno>
#include <ctime>

#include <baseproxy.hpp>
#include <hostcx.hpp>

#include <log/logger.hpp>
#include "udpcom.hpp"

#include <vars.hpp>

baseProxy::~baseProxy() {
    try {
        baseProxy::shutdown();
    }
    catch(std::exception const& e) {
        _err("Proxy: d-tor exception: %s", e.what());
    }
    
    if (com_ != nullptr) {
        _dum("Proxy: deleting com");
        delete com_;
    }
}



void baseProxy::ladd(baseHostCX* cs) {
    cs->unblock();
    
    int s = cs->socket();
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);
    left_sockets.push_back(cs);
    cs->parent_proxy(this, 'L');
    _dia("baseProxy::ladd: added socket: %s", cs->c_type());
}


void baseProxy::radd(baseHostCX* cs) {
    cs->unblock();
    
    //int s = cs->com()->translate_socket(cs->socket());
    int s = cs->socket();
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);
    right_sockets.push_back(cs);
    cs->parent_proxy(this, 'R');
    _dia("baseProxy::radd: added socket: %s", cs->c_type());
}


void baseProxy::lbadd(baseHostCX* cs) {
    
    int s = cs->com()->translate_socket(cs->socket());
    
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);
    left_bind_sockets.push_back(cs);
    cs->parent_proxy(this, 'L');
	_dia("baseProxy::lbadd: added bound socket: %s", cs->c_type());
}


void baseProxy::rbadd(baseHostCX* cs) {
    
    int s = cs->com()->translate_socket(cs->socket());
    
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);
    right_bind_sockets.push_back(cs);
    cs->parent_proxy(this, 'R');
	_dia("baseProxy::rbadd: added bound socket: %s", cs->c_type());
}


void baseProxy::lpcadd(baseHostCX* cx) {
    cx->permanent(true);
    int s = cx->com()->translate_socket(cx->socket());
    
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);
    left_pc_cx.push_back(cx);
    cx->parent_proxy(this, 'L');
    _dia("baseProxy::lpcadd: added perma socket: %s", cx->c_type());
}


void baseProxy::rpcadd(baseHostCX* cx) {
    cx->permanent(true);
    int s = cx->com()->translate_socket(cx->socket());
    
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);
    
    right_pc_cx.push_back(cx);
    cx->parent_proxy(this,'R');
    _dia("baseProxy::rpcadd: added perma socket %s", cx->c_type());
}


void baseProxy::ldaadd(baseHostCX* cs) {
    cs->unblock();
    int s = cs->com()->translate_socket(cs->socket());
    
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);

    left_delayed_accepts.push_back(cs);
    cs->parent_proxy(this,'l');
    _dia("baseProxy::ldaadd: added delayed socket: %s", cs->c_type());
}


void baseProxy::rdaadd(baseHostCX* cs) {
    cs->unblock();
    int s = cs->com()->translate_socket(cs->socket());
    
    com()->set_monitor(s);
    com()->set_poll_handler(s,this);
    
    right_delayed_accepts.push_back(cs);
    cs->parent_proxy(this,'r');
    _dia("baseProxy::rdaadd: added delayed socket: %s", cs->c_type());
}

void baseProxy::drop_cx(baseHostCX* cx) {
    cx->peer(nullptr);
    trashcan.emplace_back(cx);
}

void baseProxy::left_shutdown() {
	auto lb = left_bind_sockets.size();
	auto ls = left_sockets.size();
	auto lp = left_pc_cx.size();
	
	auto ld = left_delayed_accepts.size();
	
	for(auto* ii: left_bind_sockets) { ii->shutdown(); }
	for(auto* ii: left_sockets)       { ii->shutdown(); }
	for(auto* ii: left_pc_cx)          { ii->shutdown(); }
    for(auto* ii: left_delayed_accepts) { ii->shutdown(); }


    for(auto* ii: left_bind_sockets) { drop_cx(ii); }
    left_bind_sockets.clear();

    for(auto* ii: left_sockets) {  drop_cx(ii); }
    left_sockets.clear();

    for(auto* ii: left_pc_cx) {  drop_cx(ii); }
    left_pc_cx.clear();

    for(auto* ii: left_delayed_accepts) { drop_cx(ii); }
    left_delayed_accepts.clear();

 	_deb("baseProxy::left_shutdown: bind=%d(delayed=%d), sock=%d, perm=%d", lb, ld, ls, lp);
}


void baseProxy::right_shutdown() {
	auto rb = right_bind_sockets.size();
	auto rs = right_sockets.size();
	auto rp = right_pc_cx.size();
    
    auto rd = right_delayed_accepts.size();
	
	for(auto ii: right_bind_sockets) { ii->shutdown(); }
	for(auto ii: right_sockets)       { ii->shutdown(); }
	for(auto ii: right_pc_cx)          { ii->shutdown(); }
    for(auto ii: right_delayed_accepts) { ii->shutdown(); }


    for(auto ii: right_bind_sockets) { drop_cx(ii); }
    right_bind_sockets.clear();

    for(auto ii: right_sockets) {  drop_cx(ii); }
    right_sockets.clear();

    for(auto ii: right_pc_cx) { drop_cx(ii); }
    right_pc_cx.clear();

    for(auto ii: right_delayed_accepts) {  drop_cx(ii); }
    right_delayed_accepts.clear();      

	_deb("baseProxy::right_shutdown: bind=%d(delayed=%d), sock=%d, perm=%d", rb, rd, rs, rp);
}


void baseProxy::shutdown() {
    _dia("baseProxy::shutdown");
	left_shutdown();
	right_shutdown();
    _deb("baseProxy::shutdown finished");
}



int baseProxy::lsize() {
	return (left_sockets.size()+left_bind_sockets.size()+left_pc_cx.size()+left_delayed_accepts.size());
}


int baseProxy::rsize() {
	return (right_sockets.size()+right_bind_sockets.size()+right_pc_cx.size()+right_delayed_accepts.size());
}


bool baseProxy::on_cx_timer(baseHostCX* cx) {
    cx->on_timer();
	return true;
}


// return true if clicked, false otherwise.

bool baseProxy::clicker::reset_timer() {

    time(&clock_);

	if( static_cast<long unsigned int>(clock_) - static_cast<long unsigned int>(last_tick_) > timer_interval) {
		time(&last_tick_);

		return true;
	}

	return false;
}


// (re)set socket set and calculate max socket no

bool baseProxy::run_timers () {

    if(clicker_.reset_timer()) {

        auto cx_check = [&](auto* cx, bool idle_check=false) {
            on_cx_timer(cx);
            if(idle_check && cx->idle_timeout()) {
                state().dead(true);

                _dia("%s: timed out!", hr().c_str());
            }
        };

        auto for_each_check = [&](auto what, bool idle_check=false) {
            for(auto it: what) {
                cx_check(it, idle_check);
            }
        };


        for_each_check(left_sockets, true);
        for_each_check(left_delayed_accepts, true);
        for_each_check(left_bind_sockets);
        for_each_check(left_pc_cx, true);

        for_each_check(right_sockets, true);
        for_each_check(right_delayed_accepts, true);
        for_each_check(right_bind_sockets);
        for_each_check(right_pc_cx, true);

        return true;
    }

    return false;
}

// (re)set socket set and calculate max socket no

int baseProxy::prepare_sockets(baseCom* fdset_owner) {
     int max = 1;


     return max;
}


bool baseProxy::handle_cx_events(unsigned char side, baseHostCX* cx) {

    // treat non-blocking still opening sockets
        if( cx->opening_timeout() ) {
            _dia("baseProxy::handle_cx_events[%d]: opening timeout!", cx->socket());
            
            if     (side == 'l')  { on_left_error(cx);  }
            else if(side == 'r')  { on_right_error(cx); }
            else if(side == 'x')  { on_left_pc_error(cx); }
            else if(side == 'y')  { on_right_pc_error(cx); }

            cx->shutdown();
            return false;
        }

        if( cx->idle_timeout() ) {
            _dia("baseProxy::handle_cx_events[%d]: idle timeout!", cx->socket());

            if     (side == 'l')  { on_left_error(cx);  }
            else if(side == 'r')  { on_right_error(cx); }
            else if(side == 'x')  { on_left_pc_error(cx); }
            else if(side == 'y')  { on_right_pc_error(cx); }

            cx->shutdown();
            return false;
        }

        if( cx->error() ) {
            _dia("baseProxy::handle_cx_events[%d]: error!", cx->socket());

            if     (side == 'l')  { on_left_error(cx);  }
            else if(side == 'r')  { on_right_error(cx); }
            else if(side == 'x')  { on_left_pc_error(cx); }
            else if(side == 'y')  { on_right_pc_error(cx); }

            cx->shutdown();
            return false;
        }
        
        //process new messages before waiting_for_peercom check
        if( cx->new_message() ) {
            _dia("baseProxy::handle_cx_events[%d]: new message!", cx->socket());

            if     (side == 'l')  {  on_left_message(cx); }
            else if(side == 'r')  { on_right_message(cx); }
            else if(side == 'x')  { on_left_message(cx); }
            else if(side == 'y')  { on_right_message(cx); }
            return false;
        }    
        
        return true;
}

bool baseProxy::handle_cx_read(unsigned char side, baseHostCX* cx) {
    
    _ext("%c in R fdset: %d", side, cx->socket());
    
    bool proceed = cx->readable();
    if(cx->com()->forced_read_on_write_reset()) {
        _dia("baseProxy::handle_cx_read[%c]: read overridden on write socket event", side);
        proceed = true;
    }
    
    if (proceed) {
        _ext("%c in R fdset and readable: %d", side, cx->socket());
        int red = cx->read();
        
        if (red == 0) {
            cx->shutdown();
            //left_sockets.erase(i);


            if(side == 'l' || side == 'x') {
                handle_last_status |= HANDLE_LEFT_ERROR;
                state().error_on_left_read = true;
            } else {
                handle_last_status |= HANDLE_RIGHT_ERROR;
                state().error_on_right_read = true;
            }


            if     (side == 'l') { on_left_error(cx); }
            else if(side == 'r') { on_right_error(cx); }
            else if(side == 'x')  { on_left_pc_error(cx); }
            else if(side == 'y')  { on_right_pc_error(cx); }
           
            _deb("baseProxy::handle_cx_read[%c]: error processed", side);
           
            return false;
        }
        
        if (red > 0) {
            stats_.last_read += red;
            if     (side == 'l') { on_left_bytes(cx); }
            else if(side == 'r') { on_right_bytes(cx); }
            else if(side == 'x')  { on_left_bytes(cx); }
            else if(side == 'y')  { on_right_bytes(cx); }
            
            _deb("baseProxy::handle_cx_read[%c]: %d bytes processed", side, red);
        }
    }
    
    return true;
}

bool baseProxy::handle_cx_write(unsigned char side, baseHostCX* cx) {
    
    _ext("baseProxy::handle_cx_write[%c]: in write fdset: %d",side, cx->socket());
    
    bool proceed = cx->writable();
    if(cx->com()->forced_write_on_read_reset()) {
        _dia("baseProxy::handle_cx_read[%c]: write overridden on read socket event", side);
        proceed = true;
    }
    
    if (proceed) {
        _ext("baseProxy::handle_cx_write[%c]: writable: %d", side, cx->socket());
        int wrt = cx->write();
        if (wrt < 0) {
            cx->shutdown();
            //left_sockets.erase(i);

            if(side == 'l' || side == 'x') {
                handle_last_status |= HANDLE_LEFT_ERROR;
                state().error_on_left_write = true;
            } else {
                handle_last_status |= HANDLE_RIGHT_ERROR;
                state().error_on_right_write = true;
            }

            if     (side == 'l') { on_left_error(cx); }
            else if(side == 'r') { on_right_error(cx); }
            else if(side == 'x') { on_left_pc_error(cx); }
            else if(side == 'y') { on_right_pc_error(cx); }
            
            _deb("baseProxy::handle_cx_write[%c]: error processed", side);
            
            return false;
        } else {
            stats_.last_write += wrt;

            if(wrt > 0 and stats_.do_rate_meter) {
                if (stats_.do_rate_meter) {
                    if (side == 'l' or side == 'x') {
                        stats_.mtr_down.update(wrt);
                    } else if (side == 'r' or side == 'y') {
                        stats_.mtr_up.update(wrt);
                    }
                }
                _deb("baseProxy::handle_cx_write[%c]: %d bytes processed", side, wrt);
            }
        }
    }

    return true;
}

bool baseProxy::handle_cx_read_once(unsigned char side, baseCom* xcom, baseHostCX* cx) {

    bool ret = true;
    bool dont_read = false;

    _ext("%c: %d",side, cx->socket());
    if(cx->socket() == 0) {
        _dia("baseProxy::handle_cx_read_once[%c]: monitored socket changed to zero - terminating.", side);
        cx->error(true);
        ret = false;
        goto failure;
    }


    if ((side == 'l' || side == 'x') && state().write_right_bottleneck()) dont_read = true;
    else {
        if ((side == 'r' || side == 'y') && state().write_left_bottleneck()) dont_read = true;
    }

    if(dont_read){
        _dia("baseProxy::handle_cx_read_once[%c]: bottleneck, not reading", side);
    }


    // waiting_for_peercom cx is subject to timeout only, no r/w is done on it ( it would return -1/0 anyway, so spare some cycles)
    if( (!cx->read_waiting_for_peercom()) && (!dont_read) ) {
        bool forced_read = cx->com()->forced_read_reset();
        bool in_read_set = xcom->in_readset(cx->socket());

        if(in_read_set || forced_read) {

            if(forced_read) {
                if(! in_read_set) {
                    _dia("baseProxy::handle_cx_read_once[%c]: forced read, NOT in read set", side);
                } else {
                    _deb("baseProxy::handle_cx_read_once[%c]: forced read, but in read set too", side);
                }
            }
            
            if(! handle_cx_read(side,cx)) {
                ret = false;
                goto failure;
            }
            
            if(cx->com()->forced_write_on_read()) {
                _dia("baseProxy::handle_cx_read_once[%c]: write on read enforced on socket %d", side, cx->socket());
                if(! handle_cx_write(side,cx)) {
                    ret = false;
                    goto failure;
                }
            }
        }
    } else {
        _dia("baseProxy::handle_cx_read_once[%c]: waiting_for_peercom read in cx with socket %d, in read_set: %s", side, cx->socket(),
                                                                 xcom->in_readset(cx->socket()) ? "yes" : "no");
    }

    // on failure, skip all operations and go here
    failure:
    
    // errors are products of operations above. Act on them.
    if(not handle_cx_events(side, cx))
        ret = false;    
    
    return ret;
}


// Iterate vector and set monitoring for each cx->socket() according to ifread (read monitor), and ifwrite (write monitor).
// Tricky one:
// paused_* arguments: 0 - don't change anything pausing
//                     greater than 0 - pause read or write
//                     lesser than 0 - unpause read or write

unsigned int baseProxy::change_monitor_for_cx_vec(std::vector<baseHostCX*>* cx_vec, bool ifread, bool ifwrite, int pause_read, int pause_write) {

    unsigned int sockets_changed = 0;

    // do reference
    if(cx_vec) {
        std::vector<baseHostCX *>& nnn = *cx_vec;
        for(auto cx: nnn) {
            if(ifread && ifwrite) {
                cx->com()->change_monitor(cx->socket(),EPOLLIN|EPOLLOUT);
            } else {
                if (ifread) {
                    cx->com()->change_monitor(cx->socket(), EPOLLIN);
                } else if (ifwrite) {
                    cx->com()->change_monitor(cx->socket(), EPOLLOUT);
                } else {
                    cx->com()->unset_monitor(cx->socket());
                }
            }

            if(pause_read != 0) {
                cx->read_waiting_for_peercom(pause_read > 0);
            }
            if(pause_write != 0) {
                cx->write_waiting_for_peercom(pause_write > 0);
            }

            sockets_changed++;
        }
    }

    return sockets_changed;
}

unsigned int baseProxy::change_side_monitoring(unsigned char side, bool ifread, bool ifwrite, int pause_read, int pause_write) {

    std::string str_side = "unknown";
    std::vector<baseHostCX*>* normal = nullptr;
    std::vector<baseHostCX*>* bound  = nullptr;


    if (side == 'l' || side == 'x') {
        str_side = "left";
        normal = &ls();
        bound = &lbs();
    }
    if (side == 'r' || side == 'y') {
        str_side = "right";
        normal = &rs();
        bound = &rbs();
    }


    unsigned int sockets_changed = 0;

    if(normal) {
        sockets_changed += change_monitor_for_cx_vec(normal,ifread,ifwrite, pause_read, pause_write);
    }
    if(bound) {
        sockets_changed += change_monitor_for_cx_vec(bound,ifread,ifwrite, pause_read, pause_write);
    }
    _inf("side-wide monitor change for side %c|%s [r %d:w %d - pr %d: pw %d]: %d sockets changed.",
                                              side,str_side.c_str(),
                                                 ifread, ifwrite,
                                                                 pause_read, pause_write, sockets_changed);

    return sockets_changed;
}


bool baseProxy::handle_cx_write_once(unsigned char side, baseCom* xcom, baseHostCX* cx) {

    if(cx->socket() == 0) {
        _dia("baseProxy::handle_cx_write_once[%c]: monitored socket changed to zero - terminating.",side);
        cx->error(true);

        handle_cx_events(side,cx);
        return false;
    }    

    if(cx->write_waiting_for_peercom()) {
        return handle_cx_events(side,cx);
    }

    bool in_writeset = xcom->in_writeset(cx->socket());
    bool in_force_writeset = cx->com()->forced_write_reset();


    if( in_writeset || in_force_writeset || ( ! cx->writebuf()->empty() ) ) {

        bool side_left = side == 'l' || side == 'L' || side == 'x' || side == 'X';
        bool side_right = side == 'r' || side == 'R' || side == 'y' || side == 'Y';

        auto  orig_bytes_sz = cx->writebuf()->size();
        auto  pending_bytes_sz = orig_bytes_sz;

        if(! handle_cx_write(side, cx)) {
            handle_cx_events(side,cx);
            return false;
        }
        pending_bytes_sz = cx->writebuf()->size();
        auto written_sz = orig_bytes_sz - pending_bytes_sz;

        if(cx->com()->forced_read_on_write()) {
            _dia("baseProxy::handle_cx_write_once[%c]: read on write enforced on socket %d", side, cx->socket());
            if(! handle_cx_read(side, cx)) {

                handle_cx_events(side,cx);
                return false;

            }
        }

        // if we wanted to write something, but after write we have some left-overs
        if (orig_bytes_sz > 0) {

            if (pending_bytes_sz > 0) {


                // on bottleneck, we monitor write on this socket to flush buffered data
                cx->com()->set_write_monitor(cx->socket());

                if (side_left) {
                    _dia("left write bottleneck %s, written %d, pending %d bytes!", state().write_left_bottleneck() ? "continuing" : "start", written_sz, pending_bytes_sz);
                    state().write_left_bottleneck(true);
                    change_side_monitoring('r', false, false, 1, 0);
                    change_side_monitoring('l', true, true, -1, -1);

                } else if (side_right) {
                    _dia("right write bottleneck %s, written %d, pending %d bytes!", state().write_right_bottleneck() ? "continuing" : "start", written_sz, pending_bytes_sz);
                    state().write_right_bottleneck(true);
                    change_side_monitoring('l', false, false, 1, 0);
                    change_side_monitoring('r', true, true, -1, -1);
                }
            } else {

                if (state().write_left_bottleneck() && side_left) {
                    _dia("left write bottleneck stop!");
                    state().write_left_bottleneck(false);
                    change_side_monitoring('r', true, false, -1, 0); //NOTE: write monitor enable?
                } else if (state().write_right_bottleneck() && side_right) {
                    _dia("right write bottleneck stop!");
                    state().write_right_bottleneck(false);
                    change_side_monitoring('l', true, false, -1, 0); //NOTE: write monitor enable?

                }
            }
        }
    }

    return true;
}


bool baseProxy::handle_sockets_accept(unsigned char side, baseCom* xcom, baseHostCX* thiscx) {
    
    sockaddr_storage clientInfo{};
    socklen_t addrlen = sizeof(clientInfo);

    int client = com()->accept(thiscx->socket(), (sockaddr*)&clientInfo, &addrlen);

    if(client < 0) {
        _dia("baseProxy::handle_sockets_accept[%c]: bound socket accept failed: %s", side, string_error(errno).c_str());
        // Return false and make proxy reattempt later. Reporting error as the success is always a bad idea.
        return false;
    }
    
    if(new_raw()) {
        _deb("baseProxy::handle_sockets_accept[%c]: raw processing on %d", side, client);
        if     (side == 'l') { on_left_new_raw(client); }
        else if(side == 'r') { on_right_new_raw(client); }
    }
    else {
        auto* cx = new_cx(client);

        if(!cx->read_waiting_for_peercom()) {
            _dia("baseProxy::handle_sockets_accept[%c]: new unpaused socket %d -> accepting", side, client);
            cx->on_accept_socket(client);

        } else {
            _dia("baseProxy::handle_sockets_accept[%c]: new waiting_for_peercom socket %d -> delaying", side, client);
            cx->on_delay_socket(client);
        }
        
        if     (side == 'l') { on_left_new(cx); }
        else if(side == 'r') { on_right_new(cx); }
    }
    
    return true;
}


int baseProxy::handle_sockets_once(baseCom* xcom) {

	run_timers();

    stats_.last_read = 0;
    stats_.last_write = 0;

	state().error_on_left_read = false;
	state().error_on_left_write = false;
    state().error_on_right_read = false;
    state().error_on_right_write = false;


    if ( xcom->poll_result >= 0) {


        // READS
		if(! left_sockets.empty() ) {
            for (auto i: left_sockets) {
                if (!handle_cx_read_once('l', xcom, i)) {
                    break;
                }
            }
        }

		if(! right_sockets.empty() ) {
            for (auto i: right_sockets) {
                if (!handle_cx_read_once('r', xcom, i)) {
                    break;
                }
            }
        }

		//WRITES
        if( ! left_sockets.empty() ) {
            for (auto i: left_sockets) {
                if (!handle_cx_write_once('l', xcom, i)) {
                    break;
                }
            }
        }
        if( ! right_sockets.empty() ) {
            for(auto i: right_sockets) {
                if(! handle_cx_write_once('r',xcom, i)) {
                    break;
                }
            }
        }

        // now operate permanent-connect sockets to create accepted sockets
        
        if(! left_pc_cx.empty() ) {
            for (auto i: left_pc_cx) {

                //READS

                // if socket is already in error, don't read, instead just raise again error, if we should reconnect
                if (i->error() and i->should_reconnect_now()) {
                    on_left_pc_error(i);
                    break;
                } else if (i->error()) {
                    break;
                }

                if (!handle_cx_read_once('x', xcom, i)) {
                    handle_last_status |= HANDLE_LEFT_PC_ERROR;

                    state().error_on_left_read = true;
                    on_left_pc_error(i);
                    break;
                } else {
                    bool opening_status = i->opening();
                    if (opening_status) {
                        on_left_pc_restore(i);
                    }
                }

               //WRITES

                // if socket is already in error, don't read, instead just raise again error, if we should reconnect
                if (i->error() and i->should_reconnect_now()) {
                    on_left_pc_error(i);
                    break;
                } else if (i->error()) {
                    break;
                }

                if (!handle_cx_write_once('x', xcom, i)) {
                    handle_last_status |= HANDLE_LEFT_PC_ERROR;

                    state().error_on_left_write = true;
                    on_left_pc_error(i);
                    break;
                } else {

                    if (i->opening()) {
                        on_left_pc_restore(i);
                    }
                }
            }
        }
        
        if(! right_pc_cx.empty() ) {
            for (auto i: right_pc_cx) {

                // READS

                // if socket is already in error, don't read, instead just raise again error, if we should reconnect
                if (i->error() and i->should_reconnect_now()) {
                    on_right_pc_error(i);
                    break;
                } else if (i->error()) {
                    break;
                }

                if (!handle_cx_read_once('y', xcom, i)) {
                    handle_last_status |= HANDLE_RIGHT_PC_ERROR;

                    state().error_on_right_read = true;
                    on_right_pc_error(i);
                    break;
                } else {
                    if (i->opening()) {
                        on_right_pc_restore(i);
                    }
                }


//              // WRITES

                // if socket is already in error, don't read, instead just raise again error, if we should reconnect
                if (i->error() and i->should_reconnect_now()) {
                    on_right_pc_error(i);
                    break;
                } else if (i->error()) {
                    break;
                }

                if (!handle_cx_write_once('y', xcom, i)) {
                    handle_last_status |= HANDLE_RIGHT_PC_ERROR;

                    state().error_on_right_write = true;
                    on_right_pc_error(i);
                    break;
                } else {

                    if (i->opening()) {
                        on_right_pc_restore(i);
                    }
                }
            }
        }
        
		// no socket is really ready to be processed; while it make sense to check 'connecting' sockets, it makes
		// no sense to loop through bound sockets.
		
		if (xcom->poll_result > 0) {

            auto accepted = [] (baseHostCX *p, auto action) -> bool {
                if (!p->read_waiting_for_peercom()) {
                    p->on_accept_socket(p->socket());

                    action(p);

                    return true;
                }
                return false;
            };

            auto accept_from = [&accepted] (vector_type<baseHostCX*>& v, auto action) {

                if (!v.empty()) {
                    for (auto *&ptr_ref: v) {
                        if (accepted(ptr_ref, action)) {
                            ptr_ref = nullptr;
                        }
                    }
                    v.erase(std::remove_if(v.begin(), v.end(), [] (auto *ptr) { return ptr == nullptr; }), v.end());
                }
            };

            // now operate bound sockets to create accepted sockets
            
            if( ! left_bind_sockets.empty() ) {
                for (auto i: left_bind_sockets) {
                    int s = i->socket();
                    if (xcom->in_readset(s)) {

                        auto m = locks::fd().lock(s);

                        if(m) {
                            auto l_ = std::unique_lock(*m);
                            handle_sockets_accept('l', xcom, (i));
                        }
                        else {
                            handle_sockets_accept('l', xcom, (i));
                            throw  std::runtime_error("mutex unprotected accept");
                        }

                        handle_last_status |= HANDLE_LEFT_NEW;
                    }
                }
            }
            
            
            // iterate and if un-paused, run the accept_socket and release (add them to regular socket list)
            // we will try to remove them all to not have delays

            accept_from(left_delayed_accepts, [this] (auto x) { ladd(x); });

            if(! right_bind_sockets.empty() ) {
                for (auto i: right_bind_sockets) {
                    int s = i->socket();
                    if (xcom->in_readset(s)) {

                        auto m = locks::fd().lock(s);

                        if(m) {
                            auto l_ = std::unique_lock(*m);
                            handle_sockets_accept('r', xcom, (i));
                        }
                        else {
                            handle_sockets_accept('r', xcom, (i));
                            throw  std::runtime_error("mutex unprotected accept");
                        }


                        handle_last_status |= HANDLE_RIGHT_NEW;
                    }
                }
            }

            accept_from(right_delayed_accepts, [this] (auto x) { radd(x); });

        }

		
// 		_dia("_");

        // handle the case when we are running this cycle due to n_tv timeout. In such a case return 0 to sleep accordingly.
        if (xcom->poll_result ==  0) {
            return 0;
        } else {
            return stats_.last_read + stats_.last_write;
        }
    }
    return 0;
}



void baseProxy::on_left_bytes(baseHostCX* cx) {
	_deb("Left context bytes: %s, bytes in buffer: %d", cx->c_type(), cx->readbuf()->size());
}


void baseProxy::on_right_bytes(baseHostCX* cx) {
	_deb("Right context bytes: %s, bytes in buffer: %d", cx->c_type(), cx->readbuf()->size());
}


void baseProxy::on_left_error(baseHostCX* cx) {
	if (cx->opening()) {
		_err("Left socket connection timeout %s:", cx->c_type());
	} else {
		_not("Left socket error: %s", cx->c_type());
	}
}


void baseProxy::on_right_error(baseHostCX* cx) {
	if (cx->opening()) {
		_err("Right socket connection timeout %s:", cx->c_type());
	} else {	
		_not("Right socket error: %s", cx->c_type());
	}
}


void baseProxy::on_left_pc_error(baseHostCX* cx) {
	_dum("Left permanent-connect socket error: %s", cx->c_type());
	
	if (cx->opening()) {
		_err("Left permanent socket connection timeout %s:", cx->c_type());
	}
	else if ( cx->reconnect()) {
		_inf("reconnecting");
	} 
	else {
		_dum("reconnection postponed");
	}
}


void baseProxy::on_right_pc_error(baseHostCX* cx) {
	_dum("Right permanent-connect socket error: %s", cx->c_type());

	if (cx->opening()) {
		_dia("Right permanent socket connection timeout %s:", cx->c_type());
	}
	
	if ( cx->reconnect()) {
		_dia("Reconnecting %s", cx->c_type());
	} 
	else {
		_dum("reconnection postponed");
	}
}


void baseProxy::on_left_pc_restore(baseHostCX* cx) {
    _dia("Left permanent connection restored: %s", cx->c_type());
    cx->opening(false);
    com()->set_monitor(cx->socket());
    com()->set_poll_handler(cx->socket(),this);
}


void baseProxy::on_right_pc_restore(baseHostCX* cx) {
    _dia("Right permanent connection restored: %s", cx->c_type());
    cx->opening(false);
    com()->set_monitor(cx->socket());
    com()->set_poll_handler(cx->socket(),this);    
}


void baseProxy::on_left_new(baseHostCX* cx) {
	ladd(cx);
}


void baseProxy::on_right_new(baseHostCX* cx) {
	radd(cx);
}

auto baseProxy::run_poll_socket_null_handler(int cur_socket, epoll::set_type& real_set, socket_set_type set_type) -> metering::poll {
    int hint_socket = poller()->hint_socket();

    _if_deb {
        if (cur_socket == hint_socket) {
            unsigned char buf[2048];
            memset(buf, 0, 2048);
            auto l = ::recv(cur_socket, buf, 2048, MSG_PEEK);
            if (l > 0) {
                _deb("Hint socket %d data: %s", cur_socket, buf);
            } else {
                _deb("Hint socket %d data: -none- (%d)", cur_socket, l);
            }
        }
    }

    if(cur_socket != hint_socket) {
        _war("baseProxy::run: non-hint socket %d has NO handler!!", cur_socket);

        if(cur_socket < 0) {

            // sometimes UDPCom leaves in_virt_set with orphaned virtual socket - make cleanup
            auto udpc = UDPCom::datagram_com_static();
            auto lc_ = std::scoped_lock(udpc->lock, udpc->in_virt_set.get_lock());
            udpc->in_virt_set.erase(cur_socket);

        }else {
            poller()->del(cur_socket);
        }

        metering::poll ret; ++ret.null_count;
        return ret;

    } else {
        // hint file descriptor don't have handler
        _deb("baseProxy::run: socket %d is hint socket, running proxy socket handler", cur_socket);
        handle_sockets_once(com());

        metering::poll ret; ++ret.hint_count;
        return ret;
    }
}

auto baseProxy::run_poll_socket(int cur_socket, epoll::set_type& real_set, socket_set_type set_type) -> metering::poll {

    metering::poll ret;

    epoll_handler* p_handler = com()->poller.get_handler(cur_socket);

    if(p_handler != nullptr) {

        auto seg = p_handler->fence_S;
        _ext("baseProxy::run: socket %d has registered handler 0x%x (fence %x)", cur_socket, p_handler, seg);

        if(seg != HANDLER_FENCE) {
            _err("baseProxy::run: socket %d magic fence doesn't match!!", cur_socket);
        } else {

            // Try if handler is a proxy object. If so, call different method.
            // This design is intentional, to separate meaning of "handling socket"
            // by proxy (which might be killed and terminated)
            // and generic "event handler".

            auto* proxy = dynamic_cast<baseProxy*>(p_handler);
            if(proxy != nullptr) {

                auto lcx = logan_context(proxy->to_string(iNOT));

                _deb("baseProxy::run_poll: socket %d -> handler 0x%x : executing", cur_socket, proxy);
                // call poller-carried proxy handler!
                proxy->handle_sockets_once(com());
                _deb("baseProxy::run_poll: socket %d -> handler 0x%x : finished", cur_socket, proxy);

                if(set_type == socket_set_type::ERRSET) {
                    proxy->state().dead(true);
                    _dia("Proxy 0x%x dead, socket %d in error state.", proxy, cur_socket);
                }

                if(proxy->state().dead()) {
                    proxy->shutdown();
                    _dia("baseProxy::run_poll: proxy 0x%x has been shutdown.", proxy);
                }

                ++ret.handled_count;

            } else {
                _deb("baseProxy::run: socket %d has generic handler", cur_socket);
                p_handler->handle_event(com());
                ++ret.generic_count;
            }
        }

    } else {
        auto nh = run_poll_socket_null_handler(cur_socket, real_set, set_type);
        ret += nh;
    }

    // locked, erase currently handled socket from the set
    real_set.erase(cur_socket);
    return ret;
}


int baseProxy::run_poll() {

    if(! poller()) {
        _err("com()->poller.poller is null!");
        return 1;
    }

    metering::poll stats;

    std::array<epoll::set_type*,5> sets;
    sets[socket_set_type::INSET] = &poller()->in_set;
    sets[socket_set_type::OUTSET] = &poller()->out_set;
    sets[socket_set_type::IDLESET] = &poller()->idle_set;
    sets[socket_set_type::ERRSET] = &poller()->err_set;
    sets[socket_set_type::VIRTSET] = nullptr;

    static constexpr std::array<const char*,5> setname = { "inset", "outset", "idleset", "errset", "virt-inset" };
    int name_iter = socket_set_type::INSET;

    bool is_udp = com()->master()->l4_proto() == SOCK_DGRAM;
    if(is_udp) {
        {
            auto udpc = UDPCom::datagram_com_static();
            auto lc_ = std::scoped_lock(udpc->lock, udpc->in_virt_set.get_lock());

            sets[socket_set_type::VIRTSET] = &udpc->in_virt_set;
        }
    }

    for (epoll::set_type* current_set: sets) {
        if(not current_set) continue;

        mp::set<int> copied;
        {
            auto l_ = std::scoped_lock(current_set->get_lock());
            for(auto s: current_set->get_ul()) {
                copied.emplace(s);
            }
        }
        for (auto cur_socket: copied) {
            _deb("baseProxy::run: %s socket %d ", setname.at(name_iter), cur_socket);
            auto round_stats = run_poll_socket(cur_socket, *current_set, (socket_set_type) name_iter);

            stats += round_stats;
        }

        name_iter++;
    }

    // clear in_set, so already handled sockets are excluded
    poller()->in_set.clear();

    run_timers();

        _deb("baseProxy::run: handlers (tot/cur) - proxy: %d/%d, gen: %d/%d, hint: %d/%d, null: %d/%d",
             stats_.polls.handled_count, stats.handled_count, stats_.polls.generic_count, stats.generic_count,
             stats_.polls.hint_count, stats.hint_count, stats_.polls.null_count, stats.null_count);

    stats_.polls += stats;


    return 0;
}

int baseProxy::run() {

    auto lcx = logan_context(to_string(iNOT));

    while(! state().dead() ) {
        
        if(pollroot()) {

            try {
                _ext("baseProxy::run: preparing sockets");
                int s_max = prepare_sockets(com());
                _ext("baseProxy::run: sockets prepared");
                if (s_max) {
                    com()->poll();
                }

                //  We currently ignore should_rerun:
                //  virtual udp set would trigger loop run on all threads when there are data for single one
                //  which is a bit expensive.
                //  This needs to be solved in the future.
                //
                //  DNS is ok except because its query-response nature. There are few corner-cases
                //  ie with curl, shooting two DNS queries for happy-eyeballs at once.
                run_poll();
            }
            catch (std::runtime_error const& e) {
                _err("baseProxy::run error - dead on: %s", e.what());
                state().dead(true);
            }
        }
    }

    return 0;
}

baseHostCX * baseProxy::listen(int sock, unsigned char side) {


    if ( sock > 0 ) {
        auto *cx = new baseHostCX(com()->replicate(), sock);

        cx->com()->nonlocal_dst(com()->nonlocal_dst());
        std::string res_host;
        std::string res_port;

        com()->resolve_socket_dst(sock, &res_host, &res_port);


        cx->host(res_host);
        cx->port(res_port);
        cx->rename(string_format("listen_%s:%s", res_host.c_str(), res_port.c_str()).c_str());

        if ( side == 'L' || side == 'l') lbadd(cx);
        else rbadd(cx);
        return cx;
    }

    return nullptr;
}

int baseProxy::bind(unsigned short port, unsigned char side) {


    // bind to port number - create socket
    int s = com()->bind(port);

    // listen on socket and get us hostcx
    auto cx = listen(s, side);

    if(cx) {
        return cx->socket();
    }

    return -1;
}

    // this function will always return value of 'port' parameter (but <=0 will not be added)
int baseProxy::bind(std::string const& path, unsigned char side) {

    // bind to port number - create socket
    int s = com()->bind(path.c_str());

    // listen on socket and get us hostcx
    auto cx = listen(s, side);

    if(cx) {
        cx->host() = string_format("listening_%s", path.c_str());
        return cx->socket();
    }

    return -1;
}


baseHostCX* baseProxy::new_cx(int s) {
	return new baseHostCX(com()->replicate(),s);
}


baseHostCX* baseProxy::new_cx(const char* host, const char* port) {
	return new baseHostCX(com()->replicate(),host,port);
}



int baseProxy::connect ( const char* host, const char* port, char side) {
	if (side == 'L') {
		return left_connect(host,port);
	}
	return right_connect(host,port);
}

int baseProxy::left_connect ( const char* host, const char* port)
{
	baseHostCX* cx = new_cx(host,port);
	
	int sock = cx->connect();
        if(sock > 0) {
            _dia("baseProxy::left_connect: successfully created socket %d", sock);
            lpcadd(cx);
        } else {
            _err("baseProxy::left_connect: socket not created, returned %s", sock);
        } 
        
        return sock;
}

int baseProxy::right_connect ( const char* host, const char* port)
{
	baseHostCX* cx = new_cx(host,port);
        int sock = cx->connect();
        if(sock > 0) {
            _dia("baseProxy::left_connect: successfully created socket %d", sock);
            rpcadd(cx);
        } else {
            _err("baseProxy::left_connect: socket not created, returned %s", sock);
        } 
        
        return sock;
}

std::string baseProxy::to_string(int verbosity) const {

    std::stringstream ret_ss;
    std::string left_label;
    std::string right_label;

    {
        std::stringstream l_ss;
        std::stringstream r_ss;

        for (auto ii: left_bind_sockets) l_ss << "a: " << ii->to_string(verbosity) << " ";
        for (auto ii: left_sockets) l_ss << "l:" << ii->to_string(verbosity) << " ";
        for (auto ii: left_delayed_accepts)l_ss << "l:" << ii->to_string(verbosity) << " ";
        for (auto ii: left_pc_cx) l_ss << "x:" << ii->to_string(verbosity) << " ";

        for (auto ii: right_bind_sockets) r_ss << "b:" + ii->to_string(verbosity) << " ";
        for (auto ii: right_sockets) r_ss << "r:" + ii->to_string(verbosity) << " ";
        for (auto ii: right_delayed_accepts)r_ss << "r:" + ii->to_string(verbosity) << " ";
        for (auto ii: right_pc_cx) r_ss << "y:" << ii->to_string(verbosity) << " ";

        left_label = l_ss.str();
        right_label = r_ss.str();

        if(left_label.empty())  (verbosity > iINF) ? left_label = "<empty> " : " ";
        if(right_label.empty()) (verbosity > iINF) ? right_label = "<empty> " : " ";
    }

    const char* rtr = "<+> ";
    const char* l_closed = "+> ";
    const char* r_closed = "<+ ";
    const char* closed = "<> ";

    auto rtr_symbol = rtr;

    if(left_label.empty() and right_label.empty())  rtr_symbol = closed;
    else if(left_label.empty()) rtr_symbol = l_closed;
    else if(right_label.empty()) rtr_symbol = r_closed;

    ret_ss << left_label << rtr_symbol << right_label;

	if(verbosity > DIA) {
        ret_ss << "\n";
        ret_ss << string_format("    parent id: 0x%x, poll_root: %d", parent(), pollroot());
    }
	
	return ret_ss.str();
}
