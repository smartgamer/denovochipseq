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
include core/apps/splazers/CMakeFiles/splazers.dir/depend.make

# Include the progress variables for this target.
include core/apps/splazers/CMakeFiles/splazers.dir/progress.make

# Include the compile flags for this target's objects.
include core/apps/splazers/CMakeFiles/splazers.dir/flags.make

core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o: core/apps/splazers/CMakeFiles/splazers.dir/flags.make
core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o: ../core/apps/splazers/splazers.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/yuhao/chipSeq/software/seqan-trunk/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/splazers && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/splazers.dir/splazers.cpp.o -c /home/yuhao/chipSeq/software/seqan-trunk/core/apps/splazers/splazers.cpp

core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/splazers.dir/splazers.cpp.i"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/splazers && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/yuhao/chipSeq/software/seqan-trunk/core/apps/splazers/splazers.cpp > CMakeFiles/splazers.dir/splazers.cpp.i

core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/splazers.dir/splazers.cpp.s"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/splazers && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/yuhao/chipSeq/software/seqan-trunk/core/apps/splazers/splazers.cpp -o CMakeFiles/splazers.dir/splazers.cpp.s

core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.requires:
.PHONY : core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.requires

core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.provides: core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.requires
	$(MAKE) -f core/apps/splazers/CMakeFiles/splazers.dir/build.make core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.provides.build
.PHONY : core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.provides

core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.provides.build: core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o

# Object files for target splazers
splazers_OBJECTS = \
"CMakeFiles/splazers.dir/splazers.cpp.o"

# External object files for target splazers
splazers_EXTERNAL_OBJECTS =

bin/splazers: core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o
bin/splazers: core/apps/splazers/CMakeFiles/splazers.dir/build.make
bin/splazers: core/apps/splazers/CMakeFiles/splazers.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable ../../../bin/splazers"
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/splazers && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/splazers.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
core/apps/splazers/CMakeFiles/splazers.dir/build: bin/splazers
.PHONY : core/apps/splazers/CMakeFiles/splazers.dir/build

core/apps/splazers/CMakeFiles/splazers.dir/requires: core/apps/splazers/CMakeFiles/splazers.dir/splazers.cpp.o.requires
.PHONY : core/apps/splazers/CMakeFiles/splazers.dir/requires

core/apps/splazers/CMakeFiles/splazers.dir/clean:
	cd /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/splazers && $(CMAKE_COMMAND) -P CMakeFiles/splazers.dir/cmake_clean.cmake
.PHONY : core/apps/splazers/CMakeFiles/splazers.dir/clean

core/apps/splazers/CMakeFiles/splazers.dir/depend:
	cd /home/yuhao/chipSeq/software/seqan-trunk/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/yuhao/chipSeq/software/seqan-trunk /home/yuhao/chipSeq/software/seqan-trunk/core/apps/splazers /home/yuhao/chipSeq/software/seqan-trunk/build /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/splazers /home/yuhao/chipSeq/software/seqan-trunk/build/core/apps/splazers/CMakeFiles/splazers.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : core/apps/splazers/CMakeFiles/splazers.dir/depend

