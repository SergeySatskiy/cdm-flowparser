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
 * Python extension module - versions
 */

#ifndef CFLOWVERSION_HPP
#define CFLOWVERSION_HPP


#define CDM_CF_STR_HELPER(x) #x
#define CDM_CF_STR(x) CDM_CF_STR_HELPER(x)

#define CML_VERSION  1
#define CML_VERSION_AS_STRING CDM_CF_STR(CML_VERSION)


#ifndef CDM_CF_PARSER_VERSION
#error "You must specify version"
#endif

#endif

