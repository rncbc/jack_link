// jack_link.cpp
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

#include "jack_link.hpp"

#include <iostream>
#include <string>
#include <cctype>


jack_link::jack_link (void) : m_link(120.0), m_pJackClient(NULL),
	m_sampleRate(44100.0), m_numPeers(0), m_tempo(120.0), m_requestedTempo(0.0), m_quantum(4.0)
{
	initialize();
}


jack_link::~jack_link (void)
{
	terminate();
}


int jack_link::process_callback ( jack_nframes_t nframes, void *pvUserData )
{
	jack_link *pJackLink = static_cast<jack_link *> (pvUserData);
	return pJackLink->process_callback(nframes);
}


int jack_link::process_callback ( jack_nframes_t nframes )
{
	if (m_requestedTempo > 0.0 && m_mutex.try_lock()) {
		const auto frameTime
			= ::jack_last_frame_time(m_pJackClient);
		const auto hostTime
			= std::chrono::microseconds(llround(1.0e6 * frameTime / m_sampleRate));
		auto timeline = m_link.captureAudioTimeline();
		timeline.setTempo(m_requestedTempo, hostTime);
		m_link.commitAudioTimeline(timeline);
		m_tempo = m_requestedTempo;
		m_requestedTempo = 0.0;
		m_mutex.unlock();
	}

	return 0;
}


void jack_link::timebase_callback (
	jack_transport_state_t state, jack_nframes_t nframes,
	jack_position_t *position, int new_pos, void *pvUserData )
{
	jack_link *pJackLink = static_cast<jack_link *> (pvUserData);
	pJackLink->timebase_callback(state, nframes, position, new_pos);
}


void jack_link::timebase_callback (
	jack_transport_state_t state, jack_nframes_t nframes,
	jack_position_t *position, int new_pos )
{
	const auto time = std::chrono::microseconds(
		llround(1.0e6 * position->frame / position->frame_rate));

	const double beats_per_minute = m_tempo;
	const double beats_per_bar = std::max(m_quantum, 1.);

	const double beats = beats_per_minute * time.count() / 60.0e6;
	const double bar = std::floor(beats / beats_per_bar);
	const double beat = beats - bar * beats_per_bar;

	static const double ticks_per_beat = 960.0;
	static const float beat_type = 4.0f;

	position->valid = JackPositionBBT;
	position->bar = int32_t(bar) + 1;
	position->beat = int32_t(beat) + 1;
	position->tick = int32_t(ticks_per_beat * beat / beats_per_bar);
	position->beats_per_bar = float(beats_per_bar);
	position->ticks_per_beat = ticks_per_beat;
	position->beats_per_minute = beats_per_minute;
	position->beat_type = beat_type;
}


void jack_link::tempo_callback ( const double bpm )
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_tempo = bpm;
}


void jack_link::peers_callback ( const std::size_t n )
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_numPeers = n;
}


void jack_link::initialize (void)
{
	m_link.setTempoCallback([this](const double bpm) { tempo_callback(bpm); });
	m_link.setNumPeersCallback([this](const std::size_t n) { peers_callback(n); });

	jack_status_t status = JackFailure;
	m_pJackClient = ::jack_client_open("jack_link", JackNullOption, &status);
	if (m_pJackClient == NULL) {
		std::cerr << "Could not initialize JACK client:" << std::endl;
		if (status & JackFailure)
			std::cerr << "Overall operation failed." << std::endl;
		if (status & JackInvalidOption)
			std::cerr << "Invalid or unsupported option." << std::endl;
		if (status & JackNameNotUnique)
			std::cerr << "Client name not unique." << std::endl;
		if (status & JackServerStarted)
			std::cerr << "Server is started." << std::endl;
		if (status & JackServerFailed)
			std::cerr << "Unable to connect to server." << std::endl;
		if (status & JackServerError)
			std::cerr << "Server communication error." << std::endl;
		if (status & JackNoSuchClient)
			std::cerr << "Client does not exist." << std::endl;
		if (status & JackLoadFailure)
			std::cerr << "Unable to load internal client." << std::endl;
		if (status & JackInitFailure)
			std::cerr << "Unable to initialize client." << std::endl;
		if (status & JackShmFailure)
			std::cerr << "Unable to access shared memory." << std::endl;
		if (status & JackVersionError)
			std::cerr << "Client protocol version mismatch." << std::endl;
		std::cerr << std::endl;
		std::terminate();
	};

	m_sampleRate = double(::jack_get_sample_rate(m_pJackClient));

	::jack_set_process_callback(
		m_pJackClient, process_callback, this);
	::jack_set_timebase_callback(
		m_pJackClient, 0, jack_link::timebase_callback, this);

	::jack_activate(m_pJackClient);

	m_link.enable(true);
}


void jack_link::terminate (void)
{
	m_link.enable(false);

	if (m_pJackClient) {
		::jack_deactivate(m_pJackClient);
		::jack_client_close(m_pJackClient);
		m_pJackClient = NULL;
	}
}


int main ( int, char ** )
{
	jack_link app;

	std::string line;
	while (line.compare("quit")) {
		std::cout << "jack_link> ";
		getline(std::cin, line);
		std::transform(
			line.begin(), line.end(),
			line.begin(), ::tolower);
	}

	return 0;
}


// end of jack_link.cpp
