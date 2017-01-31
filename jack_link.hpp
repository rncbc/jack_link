// jack_link.hpp
//
/****************************************************************************
   Copyright (C) 2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __jack_link_hpp
#define __jack_link_hpp

#define _USE_MATH_DEFINES

#include <ableton/Link.hpp>

#include <jack/jack.h>

#include <chrono>
#include <mutex>


class jack_link
{
public:

	jack_link();
	~jack_link();

protected:

	static int process_callback(
		jack_nframes_t nframes,
		void *pvUserData);

	int process_callback(
		jack_nframes_t nframes);

	static void timebase_callback(
		jack_transport_state_t state,
		jack_nframes_t nframes,
		jack_position_t *position,
		int new_pos, void *pvUserData);

	void timebase_callback(
		jack_transport_state_t state,
		jack_nframes_t nframes,
		jack_position_t *position,
		int new_pos);

	void peers_callback(const std::size_t n);
	void tempo_callback(const double bpm);

	void initialize();
	void terminate();

private:

	ableton::Link m_link;
	jack_client_t *m_pJackClient;
	jack_position_t m_jack_pos;
	double m_sampleRate;
	std::size_t m_numPeers;
	double m_tempo, m_requestedTempo;
	double m_quantum, m_requestedQuantum;
	std::mutex m_mutex;
};


#endif//__jack_link_hpp

// end of jack_link.hpp
