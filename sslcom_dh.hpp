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

#ifndef __SSLCOM_DH_HPP
#define __SSLCOM_DH_HPP

#ifndef HEADER_DH_H
#include <openssl/dh.h>
#endif

#include <common/socle_common.hpp>

#ifndef USE_OPENSSL300

DH *get_dh512();
DH *get_dh768();
DH *get_dh1024();
DH *get_dh1536();
DH *get_dh2048();
DH *get_dh3072();
DH *get_dh4096();

#endif

#endif //__SSLCOM_DH_HPP