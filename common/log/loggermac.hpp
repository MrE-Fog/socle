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

#ifndef LOGGERMAC_HPP
#define LOGGERMAC_HPP

#define iNON 0
#define iFAT 1
#define iCRI 2
#define iERR 3
#define iWAR 4
#define iNOT 5
#define iINF 6
#define iDIA 7
#define iDEB 8
#define iDUM 9
#define iEXT 10

// logging topics

#define GEN  0x00000000
#define CRT  0x00010000  // SSL/PKI topic
#define ATH  0x00020000  // user validation (AAA)

#define KEYS 0x00008000  // dump all secrets

#define LOG_FLNONE 0x00000000
#define LOG_FLRAW  0x00000001  // don't print out any dates, or additional data  on the line, just this message



#define DECLARE_LOGGING(get_name_func)  \
private:                                \
    mutable logdata_t logdata;           \
public:                                 \
    std::string hr(bool update=true) const { if(update or logdata.empty()) hr(get_name_func(iINF)); return logdata.optional().value_or(""); }; \
    void hr(std::string const& s) const {                    \
       std::scoped_lock<std::mutex> l(logdata.mtx_);        \
       logdata.hr_ = s;                                     \
    }                                                        \
    static loglevel& log_level() {  return logdata_t::lg_; };  \


#define TYPENAME_BASE(string_name)     \
    [[nodiscard]] virtual const char* c_type() const { return string_name; };



#define TYPENAME_OVERRIDE(string_name)     \
    [[nodiscard]] const char* c_type() const override { return string_name; };



#endif //LOGGERMAC_HPP

