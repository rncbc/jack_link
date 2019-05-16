// jack_link.cpp
//
/****************************************************************************
   Copyright (C) 2017-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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


jack_link::jack_link (void) :
	m_link(120.0), m_client(nullptr),
	m_srate(44100.0), m_timebase(0), m_npeers(0),
	m_tempo(120.0), m_tempo_req(0.0), m_quantum(4.0),
	m_playing(false), m_playing_req(false),
	m_running(false), m_thread([this]{ worker_start(); })
{
	m_thread.detach();

	m_link.setNumPeersCallback([this](const std::size_t npeers)
		{ peers_callback(npeers); });
	m_link.setTempoCallback([this](const double tempo)
		{ tempo_callback(tempo); });
	m_link.setStartStopCallback([this](const bool playing)
		{ playing_callback(playing); });

	initialize();
}


jack_link::~jack_link (void)
{
	terminate();
}


const char *jack_link::name (void)
{
	return JACK_LINK_NAME;
}

const char *jack_link::version (void)
{
	return JACK_LINK_VERSION;
}


std::size_t jack_link::npeers (void) const
{
	return m_npeers;
}


double jack_link::srate (void) const
{
	return m_srate;
}


double jack_link::tempo (void) const
{
	return m_tempo;
}


double jack_link::quantum (void) const
{
	return m_quantum;
}


bool jack_link::playing (void) const
{
	return m_playing;
}


int jack_link::process_callback (
	jack_nframes_t /*nframes*/, void */*user_data*/ )
{
	return 0;
}


int jack_link::sync_callback (
	jack_transport_state_t state, jack_position_t *position, void *user_data )
{
	jack_link *pJackLink = static_cast<jack_link *> (user_data);
	return pJackLink->sync_callback(state, position);
}


int jack_link::sync_callback (
	jack_transport_state_t state, jack_position_t *pos )
{
	bool ret = (m_playing_req && m_playing);

	if (ret) {
		auto session_state = m_link.captureAudioSessionState();
		const auto host_time = m_link.clock().micros();
		const auto beat = session_state.beatAtTime(host_time, m_quantum);
		ret = (beat < 0.0);
		m_link.commitAudioSessionState(session_state);
	}

	return !ret;
}


void jack_link::timebase_callback (
	jack_transport_state_t state, jack_nframes_t nframes,
	jack_position_t *pos, int new_pos, void *user_data )
{
	jack_link *pJackLink = static_cast<jack_link *> (user_data);
	pJackLink->timebase_callback(state, nframes, pos, new_pos);
}


void jack_link::timebase_callback (
	jack_transport_state_t state, jack_nframes_t nframes,
	jack_position_t *pos, int new_pos )
{
	if (m_tempo_req > 0.0 && m_mutex.try_lock()) {
		m_tempo = m_tempo_req;
		m_tempo_req = 0.0;
		m_mutex.unlock();
	}

	const auto frame_time = std::chrono::microseconds(
		std::llround(1.0e6 * pos->frame / pos->frame_rate));

	const double beats_per_minute = m_tempo;
	const double beats_per_bar = std::max(m_quantum, 1.0);

	const double beats = beats_per_minute * frame_time.count() / 60.0e6;
	const double bar = std::floor(beats / beats_per_bar);
	const double beat = beats - bar * beats_per_bar;

	const bool   valid = (pos->valid & JackPositionBBT);
	const double ticks_per_beat = (valid ? pos->ticks_per_beat : 960.0);
	const float  beat_type = (valid ? pos->beat_type : 4.0f);

	pos->valid = JackPositionBBT;
	pos->bar = int32_t(bar) + 1;
	pos->beat = int32_t(beat) + 1;
	pos->tick = int32_t(ticks_per_beat * beat / beats_per_bar);
	pos->beats_per_bar = float(beats_per_bar);
	pos->ticks_per_beat = ticks_per_beat;
	pos->beats_per_minute = beats_per_minute;
	pos->beat_type = beat_type;

	if (new_pos) ++m_timebase;
}


void jack_link::on_shutdown ( void *user_data )
{
	jack_link *pJackLink = static_cast<jack_link *> (user_data);
	pJackLink->on_shutdown();
}


void jack_link::on_shutdown (void)
{
	std::cerr << "jack_link::on_shutdown()" << std::endl;
	m_client = nullptr;

	terminate();

	::fclose(stdin);
	std::cerr << std::endl;
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


void jack_link::playing_callback ( const bool playing )
{
	if (m_playing_req && m_mutex.try_lock()) {
		m_playing_req = false;
		m_mutex.unlock();
		return;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	std::cerr << "jack_link::playing_callback(" << playing << ")" << std::endl;
	m_playing_req = true;
	m_playing = playing;
	transport_reset();
	m_cond.notify_one();
}


void jack_link::initialize (void)
{
	m_link.enableStartStopSync(true);

	jack_status_t status = JackFailure;
	m_client = ::jack_client_open(jack_link::name(), JackNullOption, &status);
	if (m_client == nullptr) {
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
	::jack_set_sync_callback(m_client, sync_callback, this);
	::jack_on_shutdown(m_client, on_shutdown, this);

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
		m_client = nullptr;
	}
}


void jack_link::timebase_reset (void)
{
	if (m_client == nullptr)
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


void jack_link::transport_reset (void)
{
	if (m_client == nullptr)
		return;

	if (m_playing_req && m_playing) {
		auto session_state = m_link.captureAppSessionState();
		const auto host_time = m_link.clock().micros();
		session_state.requestBeatAtTime(0.0, host_time, m_quantum);
		const auto beat = session_state.beatAtTime(host_time, m_quantum);
		if (beat < 0.0) {
			jack_position_t position;
			const jack_transport_state_t state
				= ::jack_transport_query(m_client, &position);
			if (state == JackTransportStopped) {
				// Advance/relocate at the nearest zero-beat frame..
				const auto frame_time = std::chrono::microseconds(
					std::llround(1.0e6 * position.frame / position.frame_rate));
				const double beats_per_bar
					= std::max(m_quantum, 1.0);
				const double beats
					= m_tempo * frame_time.count() / 60.0e6;
				const double beat0
					= beats + beat + beats_per_bar
					- std::fmod(beats, beats_per_bar);
				if (beat0 > 0.0) {
					const jack_nframes_t frame0
						= std::lrint(60.0 * position.frame_rate * beat0 / m_tempo);
					::jack_transport_locate(m_client, frame0);
				}
			}
		}
		m_link.commitAppSessionState(session_state);
	}

	if (m_playing)
		::jack_transport_start(m_client);
	else
		::jack_transport_stop(m_client);
}


void jack_link::worker_start (void)
{
	m_running = true;

	std::cout << jack_link::name() << " v" << jack_link::version() << std::endl; 
	std::cout << jack_link::name() << ": started..." << std::endl; 

	while (m_running) {
		std::unique_lock<std::mutex> lock(m_mutex);
		worker_run();
		m_cond.wait_for(lock, std::chrono::milliseconds(100));
	}

	std::cout << jack_link::name() << ": terminated." << std::endl;
}


void jack_link::worker_run (void)
{
	if (m_client && m_npeers > 0) {

		int request = 0;

		double beats_per_minute = 0.0;
		double beats_per_bar = 0.0;
		bool playing_req = false;

		jack_position_t position;
		const jack_transport_state_t state
			= ::jack_transport_query(m_client, &position);

		const bool playing
			= (state == JackTransportRolling
			|| state == JackTransportLooping);

		if ((playing && !m_playing) || (!playing && m_playing)) {
			if (m_playing_req) {
				m_playing_req = false;
			} else {
				playing_req = true;
				++request;
			}
		}

		if (position.valid & JackPositionBBT) {
			if (std::abs(m_tempo - position.beats_per_minute) > 0.01) {
				beats_per_minute = position.beats_per_minute;
				++request;
			}
			if (std::abs(m_quantum - position.beats_per_bar) > 0.01) {
				beats_per_bar = position.beats_per_bar;
				++request;
			}
		}

		if (request > 0) {
			auto session_state = m_link.captureAppSessionState();
			const auto host_time = m_link.clock().micros();
			if (beats_per_minute > 0.0) {
				m_tempo = beats_per_minute;
				session_state.setTempo(m_tempo, host_time);
			}
			if (beats_per_bar > 0.0) {
				m_quantum = beats_per_bar;
				session_state.requestBeatAtTime(0.0, host_time, m_quantum);
			}
			if (playing_req) {
				m_playing_req = true;
				m_playing = playing;
				// Find the current frame beat fraction..
				const auto frame_time = std::chrono::microseconds(
					std::llround(1.0e6 * position.frame / position.frame_rate));
				const double beats_per_bar
					= std::max(m_quantum, 1.0);
				const double beats
					= m_tempo * frame_time.count() / 60.0e6;
				const double beat0
					= std::fmod(beats, beats_per_bar) - beats_per_bar;
				session_state.setIsPlayingAndRequestBeatAtTime(
					m_playing, host_time, beat0, m_quantum);
			}
			m_link.commitAppSessionState(session_state);
		}
	}
}


void jack_link::worker_stop (void)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_running) {
		m_running = false;
		m_cond.notify_all();
	}
}


#include <csignal>

void sig_handler ( int sig_no )
{
	::fclose(stdin);
	std::cerr << std::endl;
}


#include <cctype>

int main ( int /*argc*/, char **/*argv*/ )
{
	::signal(SIGABRT, &sig_handler);
	::signal(SIGHUP,  &sig_handler);
	::signal(SIGINT,  &sig_handler);
	::signal(SIGQUIT, &sig_handler);
	::signal(SIGTERM, &sig_handler);

	jack_link app;

	std::string line;
	while (!std::cin.eof()) {
		std::cout << app.name() << "> ";
		std::getline(std::cin, line);
		std::transform(
			line.begin(), line.end(),
			line.begin(), ::tolower);
		if (!line.compare("quit"))
			break;
		if (!line.compare("status")) {
			std::cout << "npeers: "  << app.npeers()  << std::endl;
			std::cout << "srate: "   << app.srate()   << std::endl;
			std::cout << "tempo: "   << app.tempo()   << std::endl;
			std::cout << "quantum: " << app.quantum() << std::endl;
			std::cout << "playing: " << app.playing() << std::endl;
		}
	}

	return 0;
}


// end of jack_link.cpp
