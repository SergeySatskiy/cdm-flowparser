/*
 * codimension - graphics python two-way code editor and analyzer
 * Copyright (C) 2014 - 2016  Sergey Satskiy <sergey.satskiy@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Python extension module - utility functions
 */

#include <string.h>
#include "cflowutils.hpp"


const std::string   emptyString;


const char *  trimStart( const char *  str, int  len )
{
    const char *    end( str + len );
    while ( str < end && memchr( " \t\n\r", *str, 4 ) )
        ++str;
    return str;
}

const char *  trimEnd( const char *  end, int  len )
{
    const char *    begin( end - len );
    while ( end > begin  && memchr( " \t\n\r", end[ -1 ], 4 ) )
        --end;
    return end;
}

std::string  trim( const char *  buffer, int  len )
{
    const char *    begin( trimStart( buffer, len ) );
    const char *    end( trimEnd( buffer + len, len ) );

    if ( begin < end )
        return std::string( begin, end );
    return emptyString;
}

void trimInplace( std::string &  str )
{
    int           len( str.length() );
    const char *  b( str.c_str() );

    const char *    begin( trimStart( b, len ) );
    const char *    end( trimEnd( b + len, len ) );

    if ( begin < end )
        str.assign( begin, end );
    else
        str.clear();
}


void trimEndInplace( std::string &  str )
{
    int           len( str.length() );
    const char *  b( str.c_str() );
    str.assign( b, trimEnd( b + len, len ) );
}

std::vector< std::string >  splitLines( const std::string &  str )
{
    std::vector<std::string>    strings;
    std::string::size_type      pos( 0 );
    std::string::size_type      prev( 0 );

    for ( ; ; )
    {
        pos = str.find( "\r\n", prev );
        if ( pos != std::string::npos )
        {
            strings.push_back( str.substr( prev, pos - prev ) );
            prev = pos + 2;
            continue;
        }
        pos = str.find( '\n', prev );
        if ( pos != std::string::npos )
        {
            strings.push_back( str.substr( prev, pos - prev ) );
            prev = pos + 1;
            continue;
        }
        pos = str.find( '\r', prev );
        if ( pos != std::string::npos )
        {
            strings.push_back( str.substr( prev, pos - prev ) );
            prev = pos + 1;
            continue;
        }
        break;
    }

    // To get the last substring (or only, if delimiter is not found)
    strings.push_back( str.substr( prev ) );
    return strings;
}


// Note: works only for a string without \n or \r in it
std::string  expandTabs( const std::string &  s, int  tabstop )
{
    std::string     r;

    for ( std::string::const_iterator x( s.begin() ); x != s.end(); ++x )
    {
        switch ( *x )
        {
            case '\t':
                r += std::string( tabstop - ( r.size() % tabstop ), ' ' );
                break;
            default:
                r += *x;
        }
    }
    return r;
}


bool  isBlankLine( const std::string &  s )
{
    for ( std::string::const_iterator x( s.begin() ); x != s.end(); ++x )
    {
        if ( *x != ' '|| *x != '\t' )
            return false;
    }
    return true;
}

