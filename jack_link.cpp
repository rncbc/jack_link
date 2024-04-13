// jack_link.cpp
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

#include "jack_link.hpp"

#include "jack_link_log.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <csignal>


jack_link::jack_link ( const std::string& name ) :
	m_name(name), m_link(120.0), m_client(nullptr),
	m_srate(44100.0), m_timebase(0), m_npeers(0),
	m_tempo(120.0), m_tempo_req(0.0), m_quantum(4.0),
	m_playing(false), m_playing_req(false),
	m_running(false), m_thread(nullptr)
{
	m_link.setNumPeersCallback([this](const std::size_t npeers)
		{ peers_callback(npeers); });
	m_link.setTempoCallback([this](const double tempo)
		{ tempo_callback(tempo); });
	m_link.setStartStopCallback([this](const bool playing)
		{ playing_callback(playing); });

	m_link.enableStartStopSync(true);

	initialize();
}


jack_link::~jack_link (void)
{
	terminate();
}


const std::string& jack_link::name (void) const
{
	return m_name;
}


bool jack_link::active (void) const
{
	return (m_client != nullptr);
}


std::size_t jack_link::npeers (void) const
{
	return m_npeers;
}


double jack_link::srate (void) const
{
	return m_srate;
}


double jack_link::quantum (void) const
{
	return m_quantum;
}


void jack_link::tempo ( double tempo )
{
	if (m_npeers > 0) {
		auto session_state = m_link.captureAppSessionState();
		const auto host_time = m_link.clock().micros();
		session_state.setTempo(tempo, host_time);
		m_link.commitAppSessionState(session_state);
	} else {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_tempo_req = tempo;
		timebase_reset();
		m_cond.notify_one();
	}
}


double jack_link::tempo (void) const
{
	return m_tempo;
}


void jack_link::playing ( bool playing )
{
	if (m_npeers > 0) {
		auto session_state = m_link.captureAppSessionState();
		const auto host_time = m_link.clock().micros();
		session_state.setIsPlaying(playing, host_time);
		m_link.commitAppSessionState(session_state);
	} else {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_playing_req = true;
		m_playing = playing;
		transport_reset();
		m_cond.notify_one();
	}
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
	if (state == JackTransportStarting && m_playing && !m_playing_req) {
		// Sync to current JACK transport frame-beat quantum...
		auto session_state = m_link.captureAudioSessionState();
		const auto host_time = m_link.clock().micros();
		const double beat = position_beat(pos);
		session_state.forceBeatAtTime(beat, host_time, m_quantum);
		m_link.commitAudioSessionState(session_state);
	}

	return 1;
}


void jack_link::timebase_callback (
	jack_transport_state_t state, jack_nframes_t nframes,
	jack_position_t *pos, int new_pos, void *user_data )
{
	jack_link *pJackLink = static_cast<jack_link *> (user_data);
	pJackLink->timebase_callback(state, nframes, pos, new_pos);
}


void jack_link::timebase_callback (
	jack_transport_state_t /*state*/, jack_nframes_t /*nframes*/,
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
	pos->tick = int32_t(ticks_per_beat * (beat - std::floor(beat)));
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
	jack_link_log("jack_link::on_shutdown()");

	m_client = nullptr;

	terminate();

	::fclose(stdin);
	std::cerr << std::endl;

//	std::terminate();
	::raise(SIGTERM);
}


void jack_link::peers_callback ( const std::size_t npeers )
{
	std::lock_guard<std::mutex> lock(m_mutex);
	jack_link_log("jack_link::peers_callback(%u)", npeers);
	m_npeers = npeers;
	timebase_reset();
	m_cond.notify_one();
}


void jack_link::tempo_callback ( const double tempo )
{
	std::lock_guard<std::mutex> lock(m_mutex);
	jack_link_log("jack_link::tempo_callback(%g)", tempo);
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
	jack_link_log("jack_link::playing_callback(%d)", int(playing));
	m_playing_req = true;
	m_playing = playing;
	transport_reset();
	m_cond.notify_one();
}


void jack_link::initialize (void)
{
	m_thread = new std::thread([this]{ worker_start(); });
//	m_thread->detach();

	jack_status_t status = JackFailure;
	m_client = ::jack_client_open(m_name.c_str(), JackNullOption, &status);
	if (m_client == nullptr) {
		jack_link_log("Could not initialize JACK client.");
		if (status & JackFailure)
			jack_link_log("Overall operation failed.");
		if (status & JackInvalidOption)
			jack_link_log("Invalid or unsupported option.");
		if (status & JackNameNotUnique)
			jack_link_log("Client name not unique.");
		if (status & JackServerStarted)
			jack_link_log("Server is started.");
		if (status & JackServerFailed)
			jack_link_log("Unable to connect to server.");
		if (status & JackServerError)
			jack_link_log("Server communication error.");
		if (status & JackNoSuchClient)
			jack_link_log("Client does not exist.");
		if (status & JackLoadFailure)
			jack_link_log("Unable to load internal client.");
		if (status & JackInitFailure)
			jack_link_log("Unable to initialize client.");
		if (status & JackShmFailure)
			jack_link_log("Unable to access shared memory.");
		if (status & JackVersionError)
			jack_link_log("Client protocol version mismatch.");
	//	std::terminate();
		::raise(SIGTERM);
		return;
	};

	m_srate = double(::jack_get_sample_rate(m_client));

	::jack_set_process_callback(m_client, process_callback, this);
	::jack_set_sync_callback(m_client, sync_callback, this);
	::jack_on_shutdown(m_client, on_shutdown, this);

	::jack_activate(m_client);

	m_link.enable(true);

	timebase_reset();
}


void jack_link::terminate (void)
{
	worker_stop();

	if (m_thread) {
		m_thread->join();
		delete m_thread;
		m_thread = nullptr;
	}

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

	::jack_set_timebase_callback(
		m_client, 0, jack_link::timebase_callback, this);
}


void jack_link::transport_reset (void)
{
	if (m_client == nullptr)
		return;

	if (m_playing_req && m_playing && m_npeers > 0) {
		jack_position_t pos;
		const jack_transport_state_t state
			= ::jack_transport_query(m_client, &pos);
		if (state == JackTransportStopped) {
			// Sync to current JACK transport frame-beat quantum...
			auto session_state = m_link.captureAppSessionState();
			const auto host_time = m_link.clock().micros();
			const double beat = position_beat(&pos);
			session_state.forceBeatAtTime(beat, host_time, m_quantum);
			m_link.commitAppSessionState(session_state);
		}
	}

	// Start/stop playing on JACK...
	if (m_playing)
		::jack_transport_start(m_client);
	else
		::jack_transport_stop(m_client);
}


double jack_link::position_beat ( jack_position_t *pos ) const
{
	if (pos->valid & JackPositionBBT) {
		const double beats
			= double(pos->beat - 1)
			+ double(pos->tick) / double(pos->ticks_per_beat);
		return beats - double(pos->beats_per_bar);
	} else {
		const double quantum
			= std::max(m_quantum, 1.0);
		const double beats
			= m_tempo * pos->frame / (60.0 * pos->frame_rate);
		return std::fmod(beats, quantum) - quantum;
	}
}


void jack_link::worker_start (void)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	jack_link_log(m_name + ": started..."); 

	m_running = true;

	while (m_running) {
		worker_run();
		m_cond.wait_for(lock, std::chrono::milliseconds(100));
	}

	jack_link_log(m_name + ": terminated.");
}


void jack_link::worker_run (void)
{
	if (m_client && m_npeers > 0) {

		int request = 0;

		double beats_per_minute = 0.0;
		double beats_per_bar = 0.0;
		bool playing_req = false;

		jack_position_t pos;
		const jack_transport_state_t state
			= ::jack_transport_query(m_client, &pos);

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

		if (pos.valid & JackPositionBBT) {
			if (std::abs(m_tempo - pos.beats_per_minute) > 0.01) {
				beats_per_minute = pos.beats_per_minute;
				++request;
			}
			if (std::abs(m_quantum - pos.beats_per_bar) > 0.01) {
				beats_per_bar = pos.beats_per_bar;
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
				// Sync to current JACK transport frame-beat quantum...
				if (m_playing && !playing_req) {
					const double beat = position_beat(&pos);
					session_state.forceBeatAtTime(beat, host_time, m_quantum);
				}
			}
			if (playing_req) {
				m_playing_req = true;
				m_playing = playing;
				// Sync to current JACK transport frame-beat quantum...
				if (m_playing) {
					const double beat = position_beat(&pos);
					session_state.forceBeatAtTime(beat, host_time, m_quantum);
				}
				// Start/stop playing on Link...
				session_state.setIsPlaying(m_playing, host_time);
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


// daemon mode stuff...
//

#include <sys/param.h>


static bool daemon_started = false;


void daemon_fork (void)
{
	const int pid = ::fork();
	if (pid > 0)
		::exit(0);
	else 
	if (pid < 0)
		::exit(1);
}


void daemon_start (void)
{
	daemon_fork();

	::setsid();

	daemon_fork();

	for(int fd = 0; fd < NOFILE; ++fd)
		::close(fd);
	
	::chdir("/tmp");
	::umask(0);

	daemon_started = true;
}


// main line stuff...
//

void sig_handler ( int sig_no )
{
	if (daemon_started) {
		jack_link_log("Daemon is terminating with signal %d (SIG%s).", sig_no, ::sigabbrev_np(sig_no));
		daemon_started = false;
	//	::exit(sig_no);
	} else {
		::fclose(stdin);
		std::cerr << std::endl;
	}
}


void trim_ws ( std::string& s )
{
	const char *ws = " \t\n\r";
	const std::string::size_type first
		= s.find_first_not_of(ws);
	if (first != std::string::npos)
		s.erase(0, first);
	const std::string::size_type last
		= s.find_last_not_of(ws);
	if (last != std::string::npos)
		s.erase(last + 1);
	else
		s.clear();
}


void version (void)
{
	jack_link_log(JACK_LINK_NAME " v" JACK_LINK_VERSION " (Link v" ABLETON_LINK_VERSION ")");
}


void usage (void)
{
	std::cout << std::endl;
	std::cout << "Usage: " << JACK_LINK_NAME << " [options]" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << std::endl;
	std::cout << "  -n, --name <name>" << std::endl;
	std::cout << "\tClient name (default = '" JACK_LINK_NAME "')" << std::endl;
	std::cout << std::endl;
	std::cout << "  -q, --quiet" << std::endl;
	std::cout << "\tRun as quiet as a daemon (default = no)" << std::endl;
	std::cout << std::endl;
	std::cout << "  -d, --daemon" << std::endl;
	std::cout << "\tRun in the background as a daemon (default = no)" << std::endl;
	std::cout << std::endl;
	std::cout << "  -h, --help" << std::endl;
	std::cout << "\tShow this help about command line options" << std::endl;
	std::cout << std::endl;
}


int main ( int argc, char **argv )
{
	::signal(SIGABRT, &sig_handler);
	::signal(SIGHUP,  &sig_handler);
	::signal(SIGINT,  &sig_handler);
	::signal(SIGQUIT, &sig_handler);
	::signal(SIGTERM, &sig_handler);

	std::string name = JACK_LINK_NAME;
	bool quiet = false;
	bool daemon = false;

	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (!arg.compare("-n") || !arg.compare("--name")) {
			if (++i < argc) {
				name = argv[i];
				if (name.empty()) {
					std::cerr << "Invalid empty name" << std::endl;
					return 2;
				}
			}
		}
		else
		if (!arg.compare("-q") || !arg.compare("--quiet")) {
			quiet = true;
		}
		else
		if (!arg.compare("-d") || !arg.compare("--daemon")) {
			daemon = true;
		}
		else
		if (!arg.compare("-h") || !arg.compare("--help")) {
			usage();
			return 1;
		}
	}

	jack_link_log logger;

	if (daemon)
		daemon_start();

	if (daemon_started) {
		logger.start(JACK_LINK_NAME, name);
		jack_link_log("Daemon is starting with PID %u...", ::getpid());
	}

	if (!quiet)
		version();

	jack_link app(name);

	// Enter daemon loop (background)...
	//
	if (daemon) {
		while (daemon_started)
			::sleep(1);
		app.terminate();
		jack_link_log("Daemon terminated.");
		logger.stop();
		return 0;
	}

	// Enter interactive loop (foreground)...
	//
	std::string line, arg;

	while (!std::cin.eof() && app.active()) {
		if (!quiet)
			std::cout << app.name() << "> ";
		std::getline(std::cin, line);
		trim_ws(line);
		std::transform(
			line.begin(), line.end(),
			line.begin(), ::tolower);
		arg.clear();
		const std::string::size_type pos
			= line.find_first_of(' ');
		if (pos != std::string::npos) {
			arg = line.substr(pos + 1);
			line.erase(pos);
			trim_ws(arg);
		}
		if (!line.compare("quit") || !line.compare("exit"))
			break;
		if (!line.compare("start"))
			app.playing(true);
		else
		if (!line.compare("stop"))
			app.playing(false);
		else
		if (!line.compare("tempo")) {
			double bpm = 0.0;
			std::istringstream(arg) >> bpm;
			if (bpm > 0.0)
				app.tempo(bpm);
			else
				std::cout << "tempo: " << app.tempo() << std::endl;
		}
		else
		if (!line.compare("status")) {
			std::cout << "name: "    << app.name()    << std::endl;
			std::cout << "npeers: "  << app.npeers()  << std::endl;
			std::cout << "srate: "   << app.srate()   << std::endl;
			std::cout << "tempo: "   << app.tempo()   << std::endl;
			std::cout << "quantum: " << app.quantum() << std::endl;
			std::cout << "playing: " <<
				(app.playing() ? "started" : "stopped") << std::endl;
		}
		else
		if (!line.compare("version"))
			version();
		else
		if (!line.compare("help")) {
			std::cout << "help | start | stop";
			std::cout << " | tempo [bpm] | status";
			std::cout << " | version | quit | exit" << std::endl;
		}
		else
		if (!line.empty())
			std::cout << "?Invalid command." << std::endl;
	}

	return 0;
}


// end of jack_link.cpp
