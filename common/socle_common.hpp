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


// Uncomment line below and enable extensive buffer object profiling.
// Warning: it's resource intensive.

//#define SOCLE_MEM_PROFILE

// you can enforce/suppress new API with removing formula

#include <openssl/opensslv.h>

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#   define USE_OPENSSL11
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x10101000L)
#   define USE_OPENSSL111
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
#   define USE_OPENSSL300
#endif