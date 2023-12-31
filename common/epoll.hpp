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
#include <unistd.h>

#include <mpstd.hpp>
#include <log/logan.hpp>

#include <shared_mutex>

#define HANDLER_FENCE 0xcaba1a


class baseCom;


template<typename K, class T = std::set<K>>
struct protected_set {

    inline auto erase_ul(K e) {
        return set_.erase(e);
    }

    inline auto erase(K e) {
        auto l_ = std::scoped_lock(lock_);
        return set_.erase(e);
    }

    inline auto insert_ul(K e) {
        return set_.insert(e);
    }

    inline auto insert(K e) {
        auto l_ = std::scoped_lock(lock_);
        return set_.insert(e);
    }


    inline auto clear_ul() {
        return set_.clear();
    }

    inline auto clear() {
        auto l_ = std::scoped_lock(lock_);
        return set_.clear();
    }

    inline auto find_ul(K e) const {
        return ( set_.find(e) != set_.end());
    }

    inline auto find(K e) const {
        auto l_ = std::scoped_lock(lock_);
        return ( set_.find(e) != set_.end());
    }

    inline auto empty_ul() const {
        return set_.empty();
    }

    inline auto empty() const {
        auto l_ = std::scoped_lock(lock_);
        return set_.empty();
    }

    inline auto size_ul() const {
        return set_.size();
    }

    inline auto size() const {
        auto l_ = std::scoped_lock(lock_);
        return set_.size();
    }


    inline T& get_ul() {
        return set_;
    }

    inline T const& get_ul() const {
        return set_;
    }


    protected_set() = default;
    protected_set(protected_set const& r) : set_(r.set_) {}

    protected_set& operator=(protected_set const& r) {
        if(&r != this) {
            set_ = r.set_;
        }
        return *this;
    }


    inline std::recursive_mutex& get_lock() const { return lock_; }

private:
    T set_;
    mutable std::recursive_mutex lock_;
};

struct epoll {

    using set_type = protected_set<int, mp::set<int>>;
    static constexpr int EPOLLER_MAX_EVENTS = 50;

    struct epoll_event events[EPOLLER_MAX_EVENTS];
    std::atomic_int epoll_fd_ = 0;
    std::atomic_int hint_fd_ = 0;
    bool auto_epollout_remove = true;
    set_type in_set;
    set_type out_set;
    set_type err_set;
    set_type enforce_in_set;

    // this set is used for sockets where ARE already some data, but we wait for more.
    // because of this, socket will be REMOVED from in_set (so avoiding CPU spikes when there are still not enough of data)
    // but those sockets will be added latest after time set in @rescan_timeout microseconds.
    set_type rescan_set_in;
    set_type rescan_set_out;
    std::chrono::high_resolution_clock::time_point rescan_timer {};

    bool in_read_set(int check);
    bool in_write_set(int check);


    // idle timeout
    int idle_timeout_ms = 1000;

    // incremented each click and reset on trigger (if greater than idle_timeout_ms)
    long idle_counter = 0;
    // if round is 0, we are waiting. If 1 - we will trigger on watched sockets
    // make it 1, so on start it will flip to 0
    bool idle_round = true;

    //sockets to be added to idle_watched (to ensure defined idle timeout (and possibly slightly more)
    set_type idle_watched_pre;
    //idle socket timer - sockets in this list will be added to idle_set.
    // However, if we receive *any* socket activity (depends on monitoring), socket is
    set_type idle_watched;

    // set with sockets in idle state. Idle list is erased on each poll.
    set_type idle_set;
    bool in_idle_set(int check);
    bool in_idle_watched_set(int check);

    // remove socket from the idle detection machinery.
    unsigned long clear_idle_watch(int check);
    void set_idle_watch(int check);


    int init();

    /// @brief prepare rescan sockets for another wait() operation
    void process_pre_wait_rescans();

    /// @brief prepare idle sockets for another wait() operation
    void process_pre_wait_idles();

    /// @brief wait on poll results from epoll_wait with 'timeout' passed to it: zero: return immediately, negative: block indefinitely
    virtual int wait(long timeout);

    /// @brief add enforced sockets to in_set
    void enforced_to_inset();

    int process_epoll_events(int nfds);
    virtual bool add(int socket, int mask);
    virtual bool modify(int socket, int mask);
    virtual bool del(int socket);
    virtual bool rescan_in(int socket);
    virtual bool enforce_in(int socket);
    virtual bool rescan_out(int socket);
    virtual unsigned long cancel_rescan_in(int socket);
    virtual unsigned long cancel_rescan_out(int socket);
    bool rescans_empty() const;

    virtual bool click_timer_now (); // return true if we should add them back to in_set (scan their readability again). If yes, reset timer.

    void clear();

    bool hint_socket(int socket); // this is the socket which will be additionally monitored for EPOLLIN; each time it's readable, single byte is read from it.
    [[nodiscard]] inline int hint_socket() const { return hint_fd_.load(); }
    [[nodiscard]] inline int epoll_socket() const { return epoll_fd_.load(); }

    virtual ~epoll() {
        if (epoll_socket() > 0) ::close(epoll_socket());
    };

    static inline loglevel log_level {iINF};
    logan_lite log = logan_lite("com.epoll");

    void _debug_sockets(int nfds); // debug print sockets in events
};


// There is epoll_handler class somewhere around. Promises.
class epoll_handler;

// handler statistics/troubleshooting struct
struct handler_stats {
    unsigned long call_count;

    void clear() {
        call_count=0L;
    }
};

using handler_stats_t = handler_stats;

// handler + its stats holder
struct handler_info  {
    handler_stats_t stats;
    epoll_handler* handler;

    void clear() {
        handler = nullptr;
        stats.clear();
    }
};

using handler_info_t = handler_info;
/*
 * Class poller is HOLDER of epoll pointer. Reason for this is to have single point of self-initializing 
 * code. It's kind of wrapper, which doesn't init anything until there is an attempt to ADD something into it.
 */
struct epoller {
    std::unique_ptr<epoll> poller;
    virtual void init_if_null();
    
    bool in_read_set(int check);
    bool in_write_set(int check);
    bool in_idle_set(int check);
    bool add(int socket, int mask);
    bool modify(int socket, int mask);
    bool del(int socket);
    bool rescan_in(int socket);
    bool enforce_in(int socket);
    bool rescan_out(int socket);
    unsigned long cancel_rescan_in(int socket);
    unsigned long cancel_rescan_out(int socket);

    bool rescans_empty();

    bool click_timer_now (); // return true if we should add them back to in_set (scan their readability again). If yes, reset timer.
    
    int wait(long timeout);
    bool hint_socket(int socket); // this is the socket which will be additionally monitored for EPOLLIN; each time it's readable, single byte is read from it.

    // handler hints is a map of socket->handler. We will allow to grow it as needed. No purges. 
    mp::map<int,handler_info_t> handler_db;
    epoll_handler* get_handler(int check);
    void clear_handler(int check);
    void set_handler(int check, epoll_handler*);

    void set_idle_watch(int check);
    void clear_idle_watch(int check);

    ~epoller();

    logan_lite log = logan_lite("com.epoll");
    mutable std::shared_mutex lock_;
};

class epoll_handler {
public:
    int fence_S = HANDLER_FENCE;
    virtual void handle_event(baseCom*) = 0;
    virtual ~epoll_handler() {
        if(registrant != nullptr) {
            std::scoped_lock<std::mutex> l(lock_);

            for(auto s: registered_sockets.get_ul()) {

                epoll_handler* owner = registrant->get_handler(s);

                // don't remove foreign handlers!
                if(this == owner) {
                    registrant->clear_handler(s);
                }
            }
        }
    }

    friend struct epoller;

private:
    epoller* registrant = nullptr;
    epoll::set_type registered_sockets;
    std::mutex lock_;
};

struct socket_state {

    int socket_;
    epoll_handler *handler_;
    baseCom *com_;
    int state_;
    enum { SS_NONE = -1, SS_CLOSING = 0, SS_OPENING = 1 };

    bool owner_ = true;

    socket_state() : socket_(0), handler_(nullptr), com_(nullptr), state_(socket_state::SS_NONE), owner_(true) {}
    socket_state(int s, epoll_handler *h, baseCom *com, bool owner) :
        socket_(s), handler_(h), com_(com), state_(socket_state::SS_NONE), owner_(owner) {}
    virtual ~socket_state();

    void set(int s, epoll_handler *h, baseCom *com, bool owner=true) {
        socket_ = s;
        handler_ = h;
        com_ = com;
        owner_ = owner;
    }

    virtual void update (int s);
    inline void opening() { update(socket_state::SS_OPENING); }
    inline void closing() { update(socket_state::SS_CLOSING); }

    void mon_write();
    void mon_read();
    void mon_none();

    [[nodiscard]] inline int state() const { return state_; }
    [[nodiscard]] inline int socket() const { return socket_; }
    [[nodiscard]] const char* state_str() const { return ss_str(state_); }

private:
    // convert state to string
    static const char* ss_str(int s) {
        switch(s) {
            case SS_NONE:
                return "NONE";
            case SS_OPENING:
                return "OPENING";
            case SS_CLOSING:
                return "CLOSING";
            default:
                return "<?>";
        }
    }
};

#endif //EPOLL_HPP