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

#ifndef UXCOM_HPP
# define UXCOM_HPP

#include <string>
#include <cstring>
#include <ctime>
#include <csignal>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#include <log/logger.hpp>
#include <basecom.hpp>
#include <display.hpp>
#include <tcpcom.hpp>

class UxCom : public TCPCom {
public:
    UxCom(): TCPCom() {
        l3_proto(AF_UNIX);
        l4_proto(SOCK_STREAM);
        
        // change socket properties
        connect_sock_family = AF_UNIX;
        connect_sock_type = SOCK_STREAM;
        bind_sock_family = AF_UNIX;
        bind_sock_type = SOCK_STREAM;
        bind_sock_protocol = 0;

    };
    ~UxCom() override;

    baseCom* replicate() override { return new UxCom(); };
    
    int connect(const char* host, const char* port) override;
    int bind(unsigned short port) final;  //this bind is deprecated, returning always -1. Use bind(const char*).
    int bind(const char* name) override;

    TYPENAME_OVERRIDE("UxCom")
    std::string to_string([ [maybe_unused]] int verbosity) const override { return c_type(); };

    std::string shortname() const override { return std::string("ux"); }

private:
    logan_lite log {"com.unix"};
};

#endif