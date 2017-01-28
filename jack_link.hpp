// jack_link.hpp
//

#ifndef __jack_link_hpp
#define __jack_link_hpp

#define _USE_MATH_DEFINES
#include <ableton/Link.hpp>
#include <jack/jack.h>


class jack_link
{
public:

	jack_link();
	~jack_link();

protected:

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

	void initialize();
	void terminate();

private:

	ableton::Link m_link;

	jack_client_t *m_pJackClient;
};


#endif//__jack_link_hpp

// end of jack_link.hpp
