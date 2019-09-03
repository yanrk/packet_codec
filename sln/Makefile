# arguments
runlink                = static
platform               = linux/x64



# paths home
project_home           = ..
build_dir              = $(project_home)/sln
bin_dir                = $(project_home)/lib/$(platform)
object_dir             = $(project_home)/.objs



# includes of packet_codec headers
packet_codec_inc_path  = $(project_home)/inc
packet_codec_includes  = -I$(packet_codec_inc_path)



# all includes that packet_codec solution needs
includes               = $(packet_codec_includes)



# source files of packet_codec solution
packet_codec_src_path  = $(project_home)/src
packet_codec_source    = $(filter %.cpp, $(shell find $(packet_codec_src_path) -depth -name "*.cpp"))



# objects of packet_codec solution
packet_codec_objects   = $(packet_codec_source:$(project_home)%.cpp=$(object_dir)%.o)



# packet_codec depends libraries
packet_codec_depends   =



# output libraries
ifeq ($(runlink), static)
	packet_codec_outputs = $(bin_dir)/libpacket_codec.a
else ifeq ($(platform), mac)
	packet_codec_outputs = $(bin_dir)/libpacket_codec.dylib
else
	packet_codec_outputs = $(bin_dir)/libpacket_codec.so
endif



# build output command line
ifeq ($(runlink), static)
	build_command      = ar -rv $(packet_codec_outputs) $^
else
	build_command      = g++ -std=c++11 -shared -o $(packet_codec_outputs) $^ $(packet_codec_depends)
endif



# build targets
targets                = packet_codec

# let 'build' be default target, build all targets
build    : $(targets)

packet_codec      : $(packet_codec_objects)
	@echo
	@echo "@@@@@  start making packet_codec  @@@@@"
	$(build_command)
	@echo "@@@@@  make packet_codec success  @@@@@"
	@echo

# build all objects
$(object_dir)/%.o:$(project_home)/%.cpp
	@dir=`dirname $@`;		\
	if [ ! -d $$dir ]; then	\
		mkdir -p $$dir;		\
	fi
	g++ -c -std=c++11 -g -Wall -O1 -pipe -fPIC $(includes) -o $@ $<

clean            :
	rm -rf $(object_dir) $(bin_dir)/libpacket_codec.*

rebuild          : clean build
