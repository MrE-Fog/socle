#ifndef EPOLL_HPP
#define EPOLL_HPP

#include <string>
#include <cstring>
#include <ctime>
#include <csignal>
#include <vector>
#include <set>
#include <unordered_map>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <logger.hpp>

#define EPOLLER_MAX_EVENTS 50
#define HANDLER_FENCE 0xcaba1a


class baseCom;

struct epoll {
    struct epoll_event events[EPOLLER_MAX_EVENTS];
    int fd = 0;
    int hint_fd = 0;
    bool auto_epollout_remove = true;
    std::set<int> in_set;
    std::set<int> out_set;

    // this set is used for sockets where ARE already some data, but we wait for more.
    // because of this, socket will be REMOVED from in_set (so avoiding CPU spikes when there are still not enough of data)
    // but those sockets will be added latest after time set in @rescan_timeout microseconds.
    std::set<int> rescan_set_in;
    std::set<int> rescan_set_out;
    struct timeb rescan_timer;
    
    bool in_read_set(int check);
    bool in_write_set(int check);

    int init();
    virtual int wait(int timeout = -1);
    virtual bool add(int socket, int mask=EPOLLIN);
    virtual bool modify(int socket, int mask);
    virtual bool del(int socket);
    virtual bool rescan_in(int socket);
    virtual bool rescan_out(int socket);
    virtual bool should_rescan_now(); // return true if we should add them back to in_set (scan their readability again). If yes, reset timer.
    
    inline void clear() { memset(events,0,EPOLLER_MAX_EVENTS*sizeof(epoll_event)); in_set.clear(); out_set.clear(); }
    bool hint_socket(int socket); // this is the socket which will be additinally monitored for EPOLLIN; each time it's readable, single byte is read from it.
    inline int hint_socket(void) const { return hint_fd; }
    
    virtual ~epoll() {}
    
    static int log_level;
};


class epoll_handler;
/*
 * Class poller is HOLDER of epoll pointer. Reason for this is to have single point of self-initializing 
 * code. It's kind of wrapper, which doesn't init anything until there is an attempt to ADD something into it.
 */
struct epoller {
    struct epoll* poller = nullptr;
    virtual void init_if_null();
    
    bool in_read_set(int check);
    bool in_write_set(int check);
    virtual bool add(int socket, int mask=(EPOLLIN));
    virtual bool modify(int socket, int mask);
    virtual bool del(int socket);
    virtual bool rescan_in(int socket);
    virtual bool rescan_out(int socket);
    virtual bool should_rescan_now(); // return true if we should add them back to in_set (scan their readability again). If yes, reset timer.
    
    virtual int wait(int timeout = -1);
    virtual bool hint_socket(int socket); // this is the socket which will be additinally monitored for EPOLLIN; each time it's readable, single byte is read from it.

    // handler hints is a map of socket->handler. We will allow to grow it as needed. No purges. 
    std::unordered_map<int,epoll_handler*> handler_hints;    
    epoll_handler* get_handler(int check);
    void clear_handler(int check);
    void set_handler(int check, epoll_handler*);
    
    virtual ~epoller() { if(poller) delete poller; }
};

class epoll_handler {
public:
    int fence__ = HANDLER_FENCE;
    virtual void handle_event(baseCom*) = 0;
    virtual ~epoll_handler() { 
        if(registrant != nullptr) { 
            for(auto s: registered_sockets) { 
                registrant->clear_handler(s); 
            }
        }
    }
    
    friend class epoller;
protected:
    epoller* registrant = nullptr;
    std::set<int> registered_sockets;
    
};

#endif //EPOLL_HPP