# Watch system

## Overview of the build system

We have a single header build system (that depends on the Kore library) that
provides a build system for the project in this workspace.  Instead of using
make, CMake etc, the user can write their build scripts in C.  The build system
also has another feature that when run compares itself to its source and if the
source is newer, it will rebuild and relaunch itself.

All that is required to bootstrap the process is to compile `build.c` (using
`just bootstrap`) and the user can use the `build` executable from that point
on.

`build.h` is designed to be only included once in an object module.

## New feature for the build system

I would like to add a new build function `build_watch("source path",
build_func)` that watches a path and if anything changes in it, it will run the
`build_func`.  Additionally, this function will not exit unless Ctrl+C is
pressed.  This should work for both Windows and Linux.

Additionally, the build system provides a TUI that shows the errors and warnings
or a success message if everything built OK.  While in this TUI, Ctrl+C or Q
will quit.

But if there are errors, only errors are shown, even if there are warnings.  If
there are no errors, any warnings are shown.  If there are no errors or
warnings, a success message is shown.

The system can assume clang/GCC type error messages for detection.

How the information is displayed is up to you.  I would like to see colour,
emojis etc.

## Roadmap

Below are the milestones for completing this feature.  Each milestone has a
series of commits.

### Milestone 1: Core File Watching Infrastructure
**Goal**: Implement cross-platform file system watching capabilities

#### Commit 1.1: Add file watching API structure
- [x] Add `build_watch()` function signature to build.h
- [x] Define `WatchInfo` struct for watch configuration
- [x] Add platform-specific includes for file watching (Windows: `windows.h`, Linux: `sys/inotify.h`)

#### Commit 1.2: Implement Windows file watching
- [ ] Add Windows implementation using `ReadDirectoryChangesW`
- [ ] Handle recursive directory monitoring
- [ ] Filter for relevant file extensions (.c, .h, .cpp, .hpp)
- [ ] Implement proper cleanup and error handling

#### Commit 1.3: Implement Linux file watching  
- [ ] Add Linux implementation using `inotify` API
- [ ] Implement recursive directory monitoring with `inotify_add_watch`
- [ ] Handle `IN_MODIFY`, `IN_CREATE`, `IN_DELETE`, `IN_MOVED_TO` events
- [ ] Add proper cleanup and error handling

#### Commit 1.4: Add signal handling for Ctrl+C
- [ ] Implement cross-platform signal handling (SIGINT)
- [ ] Graceful shutdown of watch loops
- [ ] Cleanup resources on exit

### Milestone 2: Build Integration and Error Handling
**Goal**: Integrate file watching with existing build system and capture build output

#### Commit 2.1: Integrate with existing build system
- [ ] Modify `build_watch()` to accept `build_func` callback parameter
- [ ] Pass `CompileInfo` to callback functions
- [ ] Ensure proper arena memory management during watch loops

#### Commit 2.2: Capture build output and parse errors
- [ ] Redirect compiler output to buffers instead of stdout
- [ ] Parse clang/GCC error and warning messages using regex patterns
- [ ] Categorize messages by severity (error vs warning)
- [ ] Store parsed results in structured format

#### Commit 2.3: Implement debounced rebuilds
- [ ] Add file change debouncing (wait 100ms after last change)
- [ ] Prevent rapid successive rebuilds during bulk file operations
- [ ] Queue build requests and execute after stabilization period

### Milestone 3: Terminal User Interface (TUI)
**Goal**: Create an interactive TUI for displaying build results with color and emojis

#### Commit 3.1: Basic TUI framework
- [ ] Initialize cross-platform terminal handling
- [ ] Implement screen clearing and cursor positioning
- [ ] Add keyboard input handling for 'q' and Ctrl+C
- [ ] Set up color support detection and fallbacks

#### Commit 3.2: Status display implementation
- [ ] Create status display functions with emojis:
  - ✅ Success: "Build successful! No errors or warnings."
  - ⚠️  Warnings only: Show warning count and details
  - ❌ Errors: Show error count and details (hide warnings)
- [ ] Implement color coding (green/yellow/red)
- [ ] Add file path and line number highlighting

#### Commit 3.3: Interactive error navigation
- [ ] Allow arrow key navigation through errors/warnings
- [ ] Display error context (surrounding lines when available)
- [ ] Add jump-to-file functionality hints
- [ ] Implement scrolling for long error lists

#### Commit 3.4: Real-time update display
- [ ] Add build progress indicators (spinner/progress bar)
- [ ] Show "Building..." status during compilation
- [ ] Display file change notifications
- [ ] Add timestamp display for builds

### Milestone 4: Enhanced Features and Polish
**Goal**: Add advanced features and improve user experience

#### Commit 4.1: Configuration and filtering
- [ ] Add configuration file support (.buildwatch.json)
- [ ] Implement ignore patterns (similar to .gitignore)
- [ ] Allow custom file extension filters
- [ ] Add build timeout configuration

#### Commit 4.2: Build performance optimization
- [ ] Implement incremental build detection
- [ ] Add build caching mechanisms
- [ ] Optimize file change detection to reduce false positives
- [ ] Add build statistics (time tracking, success rate)

#### Commit 4.3: Enhanced TUI features
- [ ] Add split-pane view (file list + error details)
- [ ] Implement search functionality within errors
- [ ] Add build history navigation
- [ ] Include memory usage and performance metrics

#### Commit 4.4: Testing and documentation
- [ ] Add unit tests for file watching functionality
- [ ] Create integration tests for TUI components
- [ ] Add comprehensive error handling tests
- [ ] Update WATCH.md with usage examples and screenshots

### Milestone 5: Integration and Finalization
**Goal**: Complete integration with existing build system and ensure reliability

#### Commit 5.1: Update build.c integration
- [ ] Add command line flag support (`--watch`)
- [ ] Integrate with existing `build run` functionality
- [ ] Ensure compatibility with current build workflow

#### Commit 5.2: Cross-platform testing and fixes
- [ ] Test on Windows 10/11, Linux (Ubuntu/Arch), and macOS
- [ ] Fix platform-specific issues and edge cases
- [ ] Optimize performance for different file systems

#### Commit 5.3: Final polish and cleanup
- [ ] Code review and refactoring
- [ ] Performance profiling and optimization
- [ ] Documentation updates and examples
- [ ] Version tagging and release preparation

