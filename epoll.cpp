#include <epoll.hpp>
#include <hostcx.hpp>

int epoll::init() {
    // size in epoll_create is ignored since 2.6.8, but has to be greater than 0
    fd = epoll_create(1);
    DIA_("epoll::init: epoll socket created: %d",fd);
    if (fd == -1) {
        ERR_("epoll::init:%x: epoll_create failed! errno %d",this,errno);
    }
    
    return fd;
}

int epoll::wait(int timeout) {
    clear();
    int nfds = epoll_wait(fd, events, EPOLLER_MAX_EVENTS, timeout);
    
    if(nfds > 0) {
        EXT_("epoll::wait: %d socket events",nfds);
    }
    
    for(int i = 0; i < nfds; ++i) {
        if(events[i].events & EPOLLIN) {
            DEB_("epoll::wait: data received into socket %d",events[i].data.fd);
            in_set.insert(events[i].data.fd);
        }
        else if(events[i].events & EPOLLOUT) {
            //INF_("epoll::wait: socket %d writable",events[i].data.fd);
            out_set.insert(events[i].data.fd);
            
            if(auto_epollout_remove) {
                modify(events[i].data.fd,EPOLLIN);
            }
        } else {
            DIA_("epoll::wait: uncaught event value %d",events[i].events);
        }
    }
   
    return nfds;
}

bool epoll::add(int socket, int mask) {
    struct epoll_event ev;
    ev.events = mask;
    ev.data.fd = socket;
    
    DEB_("epoll:add:%x: epoll_ctl(%d): called to add socket %d ",this, fd, socket);
    
    if (::epoll_ctl(fd, EPOLL_CTL_ADD, socket, &ev) == -1) {
        if(errno == EEXIST) {
            EXT_("epoll:add:%x: epoll_ctl(%d): socket %d already added",this, fd, socket);
        }
        else {
            ERR_("epoll:add:%x: epoll_ctl(%d): cannot add socket %d: %s",this, fd, socket, string_error().c_str());
            return false;
        } 
    } else {
        DIA_("epoll:add:%x: epoll_ctl(%d): socket added %d",this, fd, socket);
    }
    
    return true;
}

bool epoll::modify(int socket, int mask) {
    struct epoll_event ev;
    ev.events = mask;
    ev.data.fd = socket;
    
    DEB_("epoll:modify:%x: epoll_ctl(%d): called to modify socket %d ",this, fd, socket);
    
    if (::epoll_ctl(fd, EPOLL_CTL_MOD, socket, &ev) == -1) {
        if(errno == ENOENT) {
            DIA_("epoll:modify:%x: epoll_ctl(%d): socket %d not monitored, fixing...",this, fd, socket);
            add(socket,mask);
            return false;
        }
        else {
            ERR_("epoll:modify:%x: epoll_ctl(%d): cannot modify socket %d: %s",this, fd, socket, string_error().c_str());
            return false;
        } 
    } else {
        DIA_("epoll:modify:%x: epoll_ctl(%d): socket added %d",this, fd, socket);
    }
    
    return true;
}


bool epoll::in_read_set(int check) {
    auto f = in_set.find(check);
    return (f != in_set.end());
}

bool epoll::in_write_set(int check) {
//     auto f = out_set.find(check);
//     return (f != out_set.end());
    return true;
}


void epoller::init_if_null()
{
    if (poller == nullptr) { 
        poller = new epoll(); 
        if (poller->init() < 0) {
            poller = nullptr;
            FATS_("cannot create poller instance!!!");
            exit(-1);
        }
    }
}

bool epoller::add(int socket, int mask)
{
    init_if_null();
    
    if(poller != nullptr) {
        return poller->add(socket,mask);
    }
    
    return false;
};

bool epoller::modify(int socket, int mask)
{
    init_if_null();
    
    if(poller != nullptr) {
        return poller->modify(socket,mask);
    }
    
    return false;
};

bool epoller::in_read_set(int check)
{
    init_if_null();
    if(poller) return poller->in_read_set(check);
    
    return false;
}

bool epoller::in_write_set(int check)
{
    init_if_null();
    if(poller) return poller->in_write_set(check);
    
    return false;
}

int epoller::wait(int timeout) {
    init_if_null();
    if(poller) return poller->wait(timeout);
    
    return 0;
}

epoll_handler* epoller::get_handler(int check) {
    auto it = handler_hints.find(check);
    
    if(it == handler_hints.end()) {
        return nullptr;
    } else {
        epoll_handler* ret = it->second;
        return ret;
    }

    return nullptr;
}
void epoller::clear_handler(int check) {
    handler_hints[check] = nullptr;
}

void epoller::set_handler(int check, epoll_handler* h) {
    handler_hints[check] = h;
}