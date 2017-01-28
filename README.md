# jack_link 

  jack_link is a [JACK](http://jackaudio.org) transport timebase prototype
  bridge to [Ableton Link](https://www.ableton.com/en/link/).

  Upstream author: Rui Nuno Capela <rncbc@rncbc.org>.


## Building

   jack_link relies on [link](https://github.com/Ableton/link) as a Git 
   submodule, so after the main [jack_link](https://github.com/rncbc/jack_link)
   repository is cloned, one needs to setup the working tree as follows:

     git clone https://github.com/rncbc/jack_link
     cd jack_link
     git submodule update --init --recursive

     make

   Then just run it:

     ./jack_link

   To quit, enter "quit" on the "jack_link>" prompt:

     jack_link> quit

   Enjoy.


## Copyright

   Copyright (C) 2017, rncbc aka Rui Nuno Capela. All rights reserved.

## License

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
