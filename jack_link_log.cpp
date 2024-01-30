// jack_link_log.hpp
//
/****************************************************************************
   Copyright (C) 2017-2024, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "jack_link_log.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>

#include <sys/stat.h>


//---------------------------------------------------------------------
// jack_link_log -- impl.
//

// Pseudo-singleton instance.
jack_link_log *jack_link_log::g_logger = nullptr;


// Constructor (pseudo-singleton)
//
jack_link_log::jack_link_log (void) : m_started(false)
{
	g_logger = this;
}


// Constructor (loggers)
jack_link_log::jack_link_log ( const char *format, ... )
{
	if (g_logger) {
		va_list args;
		va_start(args, format);
		g_logger->log(format, args);
		va_end(args);
	}
}


jack_link_log::jack_link_log ( const std::string& msg )
{
	if (g_logger)
		g_logger->log(msg);
}


// Destructor (common)
jack_link_log::~jack_link_log (void)
{
	if (g_logger == this) g_logger = nullptr; 
}


// Starter method (logging to file)
//
void jack_link_log::start (	const std::string& name, const std::string& appname )
{
	std::string path;	
	const char *home = ::getenv("HOME");
	if (home) {
		path += home;
		path.push_back('/');
		path += ".log";
		path.push_back('/');
		path += name;
	}
	if (path.empty() || !mkpath(path))
		path += "/tmp";
	path.push_back('/');
	path += appname;
	path += ".log";

	m_started = true;
	m_name = name;
	m_path = path;
}


// Stopper method (logging to file).
//
void jack_link_log::stop (void)
{
	m_started = false;
	m_name.clear();
	m_path.clear();
}


// Logger methods.
//
void jack_link_log::log ( const char *format, va_list& args )
{
	va_list args2; 
	va_copy(args2, args); 
	const int n = ::vsnprintf(nullptr, 0, format, args2) + 1; 
	va_end(args2); 
	char *msg = new char [n];
	::vsnprintf(msg, n, format, args);
	log(std::string(msg));
	delete [] msg;
}


void jack_link_log::log ( const std::string& msg )
{
	if (m_started && !m_name.empty() && !m_path.empty()) {
		const time_t time = ::time(0);
		std::ofstream ofs(m_path, std::ios_base::app);
		ofs << std::put_time(::localtime(&time), "%Y-%m-%d %H:%M:%S");
		ofs << ' ' << m_name << ':' << ' ' << msg << std::endl;
	} else {
		std::cout << msg << std::endl;
	}
}


// Create directory path (recursive)
//
bool jack_link_log::mkpath ( const std::string& path, int mode )
{
	if (path.empty())
		return false;

	struct stat sbuf;
	if (::stat(path.c_str(), &sbuf) == 0 && S_ISDIR(sbuf.st_mode))
		return true;

	std::string sdir(path);
	const std::string::size_type last
	= sdir.find_last_of('/');
	if (last != std::string::npos)
		sdir.erase(last);
	else
		sdir.clear();

	if (mkpath(sdir, mode))
		return (::mkdir(path.c_str(), mode) == 0);
	else
		return false;
}



// end of jack_link_log.cpp

