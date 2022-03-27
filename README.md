# jack_link 

  **jack_link** is a [JACK](https://jackaudio.org) transport timebase
  prototype bridge to [Ableton Link](https://www.ableton.com/en/link/).

  Upstream author: Rui Nuno Capela <rncbc@rncbc.org>.


## Prerequisites

   **jack_link** software prerequisites for building are a C++11 compiler
   (_g++_) and the [JACK](https://jackaudio.org) client C libraries and
   headers (_libjack-devel_).


## Building

   **jack_link** relies on [link](https://github.com/Ableton/link) as a Git 
   submodule on the main [jack_link](https://github.com/rncbc/jack_link)
   repository, so one needs to setup the working tree as follows:

     git clone --recursive https://github.com/rncbc/jack_link
     cd jack_link
     make

   Then just run it:

     ./jack_link

   To show available commands, enter `help` on the `jack_link>` prompt:

     jack_link> help

   To quit, enter `quit` or `exit` (or just press `Ctrl+D`):

     jack_link> quit

   Enjoy.


## License

   **jack_link** is free, open-source [Linux Audio](https://linuxaudio.org)
   software, distributed under the terms of the GNU General Public License
   ([GPL](https://www.gnu.org/copyleft/gpl.html)) version 2 or later.


## Copyright

   Copyright (C) 2017-2022, rncbc aka Rui Nuno Capela. All rights reserved.
