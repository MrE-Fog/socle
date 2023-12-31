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

#ifndef POOLWRITER_HPP
#define POOLWRITER_HPP

#include <mutex>
#include <fstream>
#include <iostream>

#include <ptr_cache.hpp>
#include <traflog/basefilewriter.hpp>

namespace socle {

    class poolFileWriter : public baseFileWriter {

    protected:
        explicit poolFileWriter(): ofstream_pool("ofstream-pool", 30, true ) {
            log = logan::create("socle.filewriter");
        }

    public:
        using resource_t = std::pair<std::unique_ptr<std::ofstream>, std::unique_ptr<std::mutex>>;
        using pool_t = ptr_cache<std::string, resource_t>;

        poolFileWriter& operator=(poolFileWriter const&) = delete;
        poolFileWriter(poolFileWriter const&) = delete;

        static poolFileWriter* instance() {
            static poolFileWriter w = poolFileWriter();
            return &w;
        }

        auto& ofstream_lock() { return ofstream_pool.getlock(); }
        pool_t & ofstream_cache() { return ofstream_pool; };

        std::size_t write(std::string const& fnm,std::string const& str) override;
        std::size_t write(std::string const& fnm, buffer const& data) override;
        std::shared_ptr<resource_t> get_ofstream(std::string const& fnm, bool create = true);
        bool flush(std::string const& fnm) override;
        bool close(std::string const& fnm) override;
        bool open(std::string const& fnm) override;

        // pool writer is always opened
        bool opened() override { return true; };

        // detect if file removed and create a new one
        bool recreate(std::string const& fnm) override;
    private:
        logan_lite log;

        // pool of opened streams. If expired, they will be closed and destruct.
         pool_t ofstream_pool;
    };

}

#endif //POOLWRITER_HPP
