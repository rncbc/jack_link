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


jack_link::jack_link (void) :
	m_link(120.0), m_client(NULL),
	m_srate(44100.0), m_timebase(0), m_npeers(0),
	m_tempo(120.0), m_tempo_req(0.0), m_quantum(4.0),
	m_running(false), m_thread([this]{ worker_start(); })
{
	m_thread.detach();

	initialize();
}


jack_link::~jack_link (void)
{
	terminate();
}


int jack_link::process_callback (
	jack_nframes_t /*nframes*/, void */*pvUserData*/ )
{
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
	if (m_tempo_req > 0.0 && m_mutex.try_lock()) {
		m_tempo = m_tempo_req;
		m_tempo_req = 0.0;
		m_mutex.unlock();
	}

	const auto time = std::chrono::microseconds(
		llround(1.0e6 * position->frame / position->frame_rate));

	const double beats_per_minute = m_tempo;
	const double beats_per_bar = std::max(m_quantum, 1.);

	const double beats = beats_per_minute * time.count() / 60.0e6;
	const double bar = std::floor(beats / beats_per_bar);
	const double beat = beats - bar * beats_per_bar;

	const bool   valid = (m_position.valid & JackPositionBBT);
	const double ticks_per_beat = (valid ? m_position.ticks_per_beat : 960.0);
	const float  beat_type = (valid ? m_position.beat_type : 4.0f);

	position->valid = JackPositionBBT;
	position->bar = int32_t(bar) + 1;
	position->beat = int32_t(beat) + 1;
	position->tick = int32_t(ticks_per_beat * beat / beats_per_bar);
	position->beats_per_bar = float(beats_per_bar);
	position->ticks_per_beat = ticks_per_beat;
	position->beats_per_minute = beats_per_minute;
	position->beat_type = beat_type;

	if (new_pos) ++m_timebase;
}


void jack_link::peers_callback ( const std::size_t npeers )
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cerr << "jack_link::peers_callback(" << npeers << ")" << std::endl;
	m_npeers = npeers;
	timebase_reset();
	m_cond.notify_one();
}


void jack_link::tempo_callback ( const double tempo )
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cerr << "jack_link::tempo_callback(" << tempo << ")" << std::endl;
	m_tempo_req = tempo;
	timebase_reset();
	m_cond.notify_one();
}


void jack_link::initialize (void)
{
	::memset(&m_position, 0, sizeof(jack_position_t));

	m_link.setNumPeersCallback([this](const std::size_t n) { peers_callback(n); });
	m_link.setTempoCallback([this](const double bpm) { tempo_callback(bpm); });

	jack_status_t status = JackFailure;
	m_client = ::jack_client_open("jack_link", JackNullOption, &status);
	if (m_client == NULL) {
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

	m_srate = double(::jack_get_sample_rate(m_client));

	::jack_set_process_callback(m_client, process_callback, this);

	::jack_activate(m_client);

	m_link.enable(true);
}


void jack_link::terminate (void)
{
	worker_stop();

	m_link.enable(false);

	if (m_client) {
		::jack_deactivate(m_client);
		::jack_client_close(m_client);
		m_client = NULL;
	}
}


void jack_link::timebase_reset (void)
{
	if (m_client == NULL)
		return;

	if (m_timebase > 0) {
		::jack_release_timebase(m_client);
		m_timebase = 0;
	}

	if (m_npeers > 0) {
		::jack_set_timebase_callback(
			m_client, 0, jack_link::timebase_callback, this);
	}
}


void jack_link::worker_start (void)
{
	m_running = true;

	while (m_running) {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait_for(lock, std::chrono::milliseconds(200));
		worker_run();
	}
}


void jack_link::worker_run (void)
{
	if (m_client && m_npeers > 0) {

		int request = 0;

		double beats_per_minute = 0.0;
		double beats_per_bar = 0.0;

		::jack_transport_query(m_client, &m_position);

		if (m_position.valid & JackPositionBBT) {
			if (std::abs(m_tempo - m_position.beats_per_minute) > 0.01) {
				beats_per_minute = m_position.beats_per_minute;
				++request;
			}
			if (std::abs(m_quantum - m_position.beats_per_bar) > 0.01) {
				beats_per_bar = m_position.beats_per_bar;
				++request;
			}
		}

		if (request > 0) {
			auto timeline = m_link.captureAppTimeline();
			const auto frame_time = ::jack_frame_time(m_client);
			const auto host_time = std::chrono::microseconds(
				llround(1.0e6 * frame_time / m_srate));
			if (beats_per_minute > 0.0) {
				m_tempo = beats_per_minute;
				timeline.setTempo(m_tempo, host_time);
			}
			if (beats_per_bar > 0.0) {
				m_quantum = beats_per_bar;
				timeline.requestBeatAtTime(0, host_time, m_quantum);
			}
			m_link.commitAppTimeline(timeline);
		}
	}
}


void jack_link::worker_stop (void)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_running = false;
	m_cond.notify_all();
}


int main ( int /*argc*/, char **/*argv*/ )
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
