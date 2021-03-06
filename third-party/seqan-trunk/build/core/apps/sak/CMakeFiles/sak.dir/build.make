# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /home/yuhao/software/bin/bin/cmake

# The command to remove a file.
RM = /home/yuhao/software/bin/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /home/yuhao/software/bin/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/yuhao/chipSeq/software/seqan-trunk

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/yuhao/chipSeq/software/seqan-trunk/build

# Include any dependencies generated for this target.
include core/apps/sak/CMakeFiles/sak.dir/depend.make

# Include the progress variables for this target.
include core/apps/sak/CMakeFiles/sak.dir/progress.make

# Include the compile flags for this target's objects.
include core/apps/sak/CMakeFiles/sak.dir/flags.make

core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o: core/apps/sak/CMakeFiles/sak.dir/flags.make
core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o: ../core/apps/sak/sak.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/yuhao/chipSeq/software/seqan-trunk/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/sak && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/sak.dir/sak.cpp.o -c /home/yuhao/chipSeq/software/seqan-trunk/core/apps/sak/sak.cpp

core/apps/sak/CMakeFiles/sak.dir/sak.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/sak.dir/sak.cpp.i"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/sak && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/yuhao/chipSeq/software/seqan-trunk/core/apps/sak/sak.cpp > CMakeFiles/sak.dir/sak.cpp.i

core/apps/sak/CMakeFiles/sak.dir/sak.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/sak.dir/sak.cpp.s"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/sak && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/yuhao/chipSeq/software/seqan-trunk/core/apps/sak/sak.cpp -o CMakeFiles/sak.dir/sak.cpp.s

core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.requires:
.PHONY : core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.requires

core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.provides: core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.requires
	$(MAKE) -f core/apps/sak/CMakeFiles/sak.dir/build.make core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.provides.build
.PHONY : core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.provides

core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.provides.build: core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o

# Object files for target sak
sak_OBJECTS = \
"CMakeFiles/sak.dir/sak.cpp.o"

# External object files for target sak
sak_EXTERNAL_OBJECTS =

bin/sak: core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o
bin/sak: core/apps/sak/CMakeFiles/sak.dir/build.make
bin/sak: core/apps/sak/CMakeFiles/sak.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable ../../../bin/sak"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/sak && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/sak.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
core/apps/sak/CMakeFiles/sak.dir/build: bin/sak
.PHONY : core/apps/sak/CMakeFiles/sak.dir/build

core/apps/sak/CMakeFiles/sak.dir/requires: core/apps/sak/CMakeFiles/sak.dir/sak.cpp.o.requires
.PHONY : core/apps/sak/CMakeFiles/sak.dir/requires

core/apps/sak/CMakeFiles/sak.dir/clean:
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/sak && $(CMAKE_COMMAND) -P CMakeFiles/sak.dir/cmake_clean.cmake
.PHONY : core/apps/sak/CMakeFiles/sak.dir/clean

core/apps/sak/CMakeFiles/sak.dir/depend:
	cd /home/yuhao/chipSeq/software/seqan-trunk/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/yuhao/chipSeq/software/seqan-trunk /home/yuhao/chipSeq/software/seqan-trunk/core/apps/sak /home/yuhao/chipSeq/software/seqan-trunk/build /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/sak /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/sak/CMakeFiles/sak.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : core/apps/sak/CMakeFiles/sak.dir/depend

