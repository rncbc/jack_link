// jack_link_log.hpp
//
/****************************************************************************
   Copyright (C) 2017-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#pragma once

#include <string>
#include <cstdarg>


//---------------------------------------------------------------------
// jack_link_log -- decl.
//

class jack_link_log
{
public:

	// Constructor (pseudo-singleton)
	jack_link_log();

	// Constructor (loggers)
	jack_link_log(const char *format, ...);
	jack_link_log(const std::string& msg);

	// Destructor (common)
	~jack_link_log();

	// Starter method (logging to file)
	void start(
		const std::string& name,
		const std::string& appname = "jack_link");

	// Stopper method (logging to file).
	void stop();

	// Logger instance state properties.
	bool started() const { return m_started; }

	const std::string& name() const { return m_name; }
	const std::string& path() const { return m_path; }

protected:
	
	// Logger methods.
	void log(const char *format, va_list& args);
	void log(const std::string& msg);

	// Create directory path (recursive)
	bool mkpath(const std::string& path, int mode = 0755);

private:

	// State variables.
	bool        m_started;
	std::string m_path; 
	std::string m_name; 

	// Pseudo-singleton instance.
	static jack_link_log *g_logger;
};


// end of jack_link_log.hpp
