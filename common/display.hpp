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

#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <string>
#include <vector>
#include <arpa/inet.h>


class buffer;

std::string string_format(const std::string& fmt, ...);
std::vector<std::string> 
            string_split(std::string str, char delimiter);
std::string string_trim(const std::string& orig);
std::string&& string_tolower(const std::string& orig);
std::string string_csv(const std::vector<std::string>& str_list_ref, const char delim = ',');

std::string number_suffixed(unsigned long xn);
std::string hex_print(unsigned char* data, unsigned int len);
std::string hex_dump(unsigned char *data, int size, unsigned int=0,unsigned char=0);
std::string hex_dump(buffer&, unsigned int=0,unsigned char=0);
std::string hex_dump(buffer*, unsigned int=0,unsigned char=0);
std::string string_error();
std::string bt();

std::string escape(std::string orig, bool to_print = false);
inline std::string printable(std::string orig) {
    return escape(orig,true);
}

int safe_val(std::string s, int default_val=-1);

// get sanitized, dot-separated kernel version. 
std::string get_kernel_version();
// compare dot-formated @target version with against @real version. @returns false if real version is lower than target.
bool version_check(std::string real, std::string target);

template <typename T> inline void flag_set(T* variable, T check) { *variable |= (T)check; }
template <typename T> inline T flag_set(const T variable, T check) { T r = variable; r |= (T)check; return r; }
template <typename T> inline bool flag_check(const T* variable, T check) { return (*variable & check); }
template <typename T> inline bool flag_check(const T variable, T check) { return (variable & check); }


#define  flag_add    flag_set<unsigned int> 
#define  flag_test   flag_check<unsigned int> 
 

std::string inet_family_str(int fa);
int inet_ss_address_unpack(sockaddr_storage *ptr, std::string* = nullptr, unsigned short* port = nullptr);
int inet_ss_address_remap(sockaddr_storage *orig, sockaddr_storage *mapped);
std::string inet_ss_str(sockaddr_storage *s);

#endif
