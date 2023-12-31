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


#ifndef SOBJECT_HPP_
#define SOBJECT_HPP_

#include <set>

#include <log/logger.hpp>
#include <ptr_cache.hpp>
#include <display.hpp>
#include <mutex>
#include <shared_mutex>

namespace socle {


/*
 * Accounting info for all sobjects.
*/
struct sobject_info {
    static bool enable_bt_;
    std::string* bt_ = nullptr;
    
    std::string extra_string() const { if (bt_) { return string_format("creation point:\n%s",bt_->c_str()); } else { return ""; } }
    sobject_info() { init(); }

    void init() { 
        created_ = time(nullptr); 
        if(enable_bt_) {
            bt_ = new std::string(bt());
        }
    }

    time_t created_ = 0L;
    time_t age() const { return time(nullptr) - created_; }

    std::string to_string(int verbosity) const;
    [[nodiscard]] inline std::string str() const { return to_string(iINF); };

    virtual ~sobject_info() { delete bt_; };
    
    TYPENAME_BASE("sobject_info")
    DECLARE_LOGGING(to_string)

protected:
    logan_lite log { "internal.sobject" };
};

struct meter {

private:
    std::atomic_ulong total_{};

    std::atomic_ulong prev_counter_{};
    std::atomic_ulong  curr_counter_{};

    constexpr static unsigned int scoreboard_sz = 3;
    std::atomic_ulong scoreboard[scoreboard_sz] = {0};

    std::chrono::system_clock::time_point last_update{};
    std::atomic_uint interval_{1};

    std::atomic_uint cnt_updates = 0;       // count no. of updates and shorten avg division if smaller than ( 2 + scoreboard_sz)

    mutable std::shared_mutex _chrono_lock;
public:

    explicit meter(unsigned int interval=1): interval_(interval) { last_update = std::chrono::system_clock::now(); };

    unsigned long update(unsigned long val);
    [[nodiscard]] unsigned long get() const;
    [[nodiscard]] unsigned long total() const { return total_; };

    void push_score(unsigned long val) {

        for(int i =  scoreboard_sz - 1 ; i > 0; i--) {
            unsigned long temp = scoreboard[i-1];
            scoreboard[i] = temp;
        }
        scoreboard[0] = val;
    }

    [[nodiscard]]
    unsigned long sum_score() const {
        unsigned long ret = 0;

        unsigned int max_it = cnt_updates;
        unsigned int cur_it = 0;

        for(int i =  scoreboard_sz - 1 ; i >= 0 ; i--) {
            if(++cur_it > max_it){
                break;  // dont count starting-zero values
            }

            ret += scoreboard[i];
        }
        return ret;
    }
};


class base_sobject {
public:
    virtual std::string to_string(int verbosity) const = 0;
    [[nodiscard]] inline std::string str() const { return to_string(iINF); }
};

class sobject;

// Singleton class - used as central sobject storage
class sobjectDB : public base_sobject {

    std::unordered_map<sobject*, std::unique_ptr<sobject_info>> db_;
    std::unordered_map<uint64_t, std::unique_ptr<sobject>> oid_db_;

    //sobjectDB() : db_("global object db",0, false), oid_db_("oid db", 0, false) {};
    sobjectDB()  = default;
    virtual ~sobjectDB() {
        for(auto & it: oid_db_) {
            it.second.release();
        }
    };

public:
    static sobjectDB& get() {
        static sobjectDB sobjdb = sobjectDB();
        return sobjdb;
    }

    static std::unordered_map<sobject*,std::unique_ptr<sobject_info>>& db() { return sobjectDB::get().db_; }
    static std::unordered_map<uint64_t,std::unique_ptr<sobject>>& oid_db() { return sobjectDB::get().oid_db_; }

    // convenience methods giving info in human readable string form
    static std::string str_list(const char* class_criteria = nullptr,
                                const char* delimiter = nullptr,
                                int verbosity = iINF,
                                const char* content_criteria = nullptr);

    static std::string str_stats(const char* criteria);

    static std::recursive_mutex& getlock() {
        static std::recursive_mutex m;
        return m;
    }

    // ask object to destruct itself
    static int ask_destroy(void* ptr);

    TYPENAME_BASE("sobjectDB")
    std::string to_string(int verbosity) const override { return this->c_type(); };

    DECLARE_LOGGING(to_string)

private:
    logan_lite log {"internal.sobject"};
};

class sobject : public base_sobject {

public:
    using oid_type = uint64_t ;

private:
    oid_type oid_;

public:
    sobject();
    virtual ~sobject();
    inline oid_type oid() const noexcept { return oid_; };

    // ask kindly to stop use this object (for example, user implementation could set error indicator, etc. )
    virtual bool ask_destroy() = 0;

    // return string representation of the object on single line
    std::string to_string(int verbosity) const override { std::stringstream ss; ss << this->c_type() << "-" << oid(); return ss.str(); };

    static meter& mtr_created() { static meter mtr_created_; return mtr_created_; } ;
    static meter& mtr_deleted() { static meter mtr_deleted_; return mtr_deleted_; } ;

    TYPENAME_BASE("sobject")
    DECLARE_LOGGING(to_string)

private:
    logan_lite log {"internal.sobject"};
};


template <class T> class sref;

template <class T>
class spointer {
    public:
        explicit spointer() : pointer_(nullptr) {};
        explicit spointer(T* ptr): pointer_(ptr) {};
        virtual ~spointer() { delete pointer_; }

        unsigned int usage() const { return count_.load(); }

        bool valid() const { return (pointer_ != nullptr); }
        void invalidate() { delete pointer_; pointer_ = nullptr; count_=0; }
        
        T* operator->() const { return pointer_;  }
        T& operator*() const { return *pointer_; }
        T* ptr() const { return pointer_; }
        void ptr(T* p) { invalidate(); pointer_ = p; }

        spointer<T>& operator=(const spointer<T>&) = delete;
        spointer(const spointer&) = delete;
    private:
        T* pointer_ = nullptr;
        std::atomic_int32_t count_ = 0;
        
        inline void use() { count_++; };
        inline void unuse() {
            if(count_> 0) count_--;

            // fight data race in previous test
            if(count_ < 0) {
                count_ = 0;
            }
        };
        
    friend class sref<T>;
};

template <class T>
class sref {
    public:
        explicit sref() : reference_(nullptr) {};
        explicit sref(spointer<T>* r) : reference_(r) { r->use(); };
        explicit sref(spointer<T>& r) : reference_(&r) { r.use(); };

        sref<T>& operator=(const sref& other) {
            if(this != &other) {
                unref();
                reference_ = other.reference_;
                if(reference_ != nullptr) reference_->use();
            }
            return *this;
        };

        sref<T>& operator=(const spointer<T>* sptr) {
            unref();
            newref(sptr);

            return *this;
        };

        sref(const sref<T>& other) {
            reference_ = other.reference_;

            if(reference_ != nullptr) {
                reference_->use();
            }
        };

        virtual ~sref() { unref(); };

        inline void unref() {
            if(reference_ != nullptr) {
                reference_->unuse();
                reference_ = nullptr;
            }
        }

        spointer<T>* ref() { return reference_; }

        void newref(spointer<T>* n) { reference_ = n; if(n != nullptr) { n->use(); } };
        void newref(spointer<T>& n) { reference_ = &n; if(reference_ != nullptr) { reference_->use(); } };
        void ref(spointer<T>* n) { unref(); newref(n); };
        void ref(spointer<T>& n) { unref(); newref(&n); };

        T* refval() { if(ref() != nullptr)  return ref()->pointer_; return nullptr; }


    private:
        spointer<T>* reference_ = nullptr;
};


typedef spointer<std::vector<std::string>> spointer_vector_string;
typedef spointer<std::vector<int>> spointer_vector_int;
typedef spointer<std::set<int>> spointer_set_int;
typedef sref<std::vector<std::string>> sref_vector_string;

}
#endif

