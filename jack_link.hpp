// jack_link.hpp
//
/****************************************************************************
   Copyright (C) 2017-2020, rncbc aka Rui Nuno Capela. All rights reserved.

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

#define JACK_LINK_QUOTE1(x)  #x
#define JACK_LINK_QUOTE2(x)  JACK_LINK_QUOTE1(x)

#if defined(_NAME)
#define JACK_LINK_NAME       JACK_LINK_QUOTE2(_NAME)
#else
#define JACK_LINK_NAME       JACK_LINK_QUOTE1(jack_link)
#endif

#if defined(_VERSION)
#define JACK_LINK_VERSION    JACK_LINK_QUOTE2(_VERSION)
#else
#define JACK_LINK_VERSION    JACK_LINK_QUOTE1(0.1.3)
#endif

#if defined(_LINK_VERSION)
#define ABLETON_LINK_VERSION JACK_LINK_QUOTE2(_LINK_VERSION)
#else
#define ABLETON_LINK_VERSION JACK_LINK_QUOTE1(3.0.2)
#endif

#define _USE_MATH_DEFINES

#include <ableton/Link.hpp>

#include <jack/jack.h>

#include <chrono>
#include <mutex>
#include <thread>


class jack_link
{
public:

	jack_link();
	~jack_link();

	static const char *name();
	static const char *version();

	static const char *link_version();

	bool active() const;

	std::size_t npeers() const;
	double srate() const;
	double quantum() const;

	void tempo(double tempo);
	double tempo() const;

	void playing(bool playing);
	bool playing() const;

protected:

	static int process_callback(
		jack_nframes_t nframes,
		void *user_data);

	static int sync_callback(
		jack_transport_state_t state,
		jack_position_t *pos,
		void *user_data);

	int sync_callback(
		jack_transport_state_t state,
		jack_position_t *pos);

	static void timebase_callback(
		jack_transport_state_t state,
		jack_nframes_t nframes,
		jack_position_t *pos,
		int new_pos, void *user_data);

	void timebase_callback(
		jack_transport_state_t state,
		jack_nframes_t nframes,
		jack_position_t *pos,
		int new_pos);

	static void on_shutdown(void *user_data);

	void on_shutdown();

	void peers_callback(const std::size_t npeers);
	void tempo_callback(const double tempo);
	void playing_callback(const bool playing);

	void initialize();
	void terminate();

	void timebase_reset();
	void transport_reset();

	double position_beat(jack_position_t *pos) const;

	void worker_start();
	void worker_run();
	void worker_stop();

private:

	ableton::Link m_link;
	jack_client_t *m_client;
	double m_srate;
	unsigned long m_timebase;
	std::size_t m_npeers;
	double m_tempo, m_tempo_req;
	double m_quantum;
	bool m_playing, m_playing_req;
	bool m_running;
	std::thread m_thread;
	std::mutex m_mutex;
	std::condition_variable m_cond;
};


#endif //__jack_link_hpp

// end of jack_link.hpp
