# Logger Module

Lightweight logging library implemented as a C++ module (`Logger`) with a simple streaming API, `std::format` support, per-channel log files, size-based rotation, and optional RAII function tracing.

## Features
- C++23 module import: `import Logger;`
- Levels: `NONE`, `ERROR`, `WARN`, `INFO`, `DEBUG`
- Streaming API via `operator<<` and `format(...)`
- Console printing by level, separate file logging by level
- Per-channel files (`<logPath>/<channel>.log`)
- Automatic rotation to `<logPath>/old/<channel>_<timestamp>.log` when size exceeds threshold
- Optional locking for multithreaded logging
- RAII `FunctionTracer` logs `[ENTER]`/`[EXIT]` with `std::source_location`

## Requirements
- C++23 compiler with module support (tested with MSVC 19.50+)
- CMake 4.1+ (module `FILE_SET` support)
- Standard library timezone support used by `std::chrono::zoned_time`

## Quick Start
```cpp
import Logger;
using namespace logger;

int main() {
    Logger::setLogPath("path/to/logs");     // creates directory if missing
    Logger::setFileSizeThreshold(1024 * 1024); // 1 MB rotation
    Logger::setPrintLevel(LogLevel::DEBUG); // console threshold
    Logger::setLogLevel(LogLevel::DEBUG);   // file threshold

    Logger(LogLevel::DEBUG) << "hello";
    Logger(LogLevel::INFO).format("value = {}", 42);
    (LogDebug("net").format("connected {}", "ok") << " extra").format(" details");

    FunctionTracer tracer(LogLevel::INFO, "trace");
    // do work...
}
```

### Message Format
Each log line is prefixed with:
`[YYYY-MM-DD HH:MM:SS.mmm][<channel>][<level>][TID:<thread_id>][<file>:<line>]`

## Integration

### As a subdirectory
If your project layout contains `logger-module/`:
```cmake
cmake_minimum_required(VERSION 4.1)
project(MyApp LANGUAGES CXX)
add_executable(MyApp main.cpp)

add_subdirectory(logger-module)
target_link_libraries(MyApp PRIVATE logger)
set_property(TARGET MyApp PROPERTY CXX_STANDARD 23)
```

### As a standalone library target
If you copy `logger-module/logger.cppm` into your project, define the module file set:
```cmake
add_library(logger)
target_sources(logger
    PUBLIC
        FILE_SET CXX_MODULES FILES
        logger.cppm
)
set_target_properties(logger PROPERTIES CXX_STANDARD 23)
```

Then import the module in your sources:
```cpp
import Logger;
using namespace logger;
```

## Configuration API
- `Logger::setLogPath(std::string path)`  
  Directory for logs. Creates directories if needed. A trailing slash is trimmed.

- `Logger::setFileSizeThreshold(std::streamoff bytes)`  
  Rotate when the active file exceeds `bytes`. Default is 1 MB.

- `Logger::setPrintLevel(LogLevel level)`  
  Console printing threshold. `NONE` disables console output.

- `Logger::setLogLevel(LogLevel level)`  
  File logging threshold. `NONE` disables file logging entirely.

- `Logger::setLockingEnabled(bool enabled)`  
  Enable/disable internal locking for multithreaded logging.

## Channels and Rotation
- Passing a `channel` groups logs into `<logPath>/<channel>.log`. Empty channel defaults to `"Default"`.
- When rotation triggers, the current file is renamed to `<logPath>/old/<channel>_<YYYYMMDDHHMMSS>.log`, and a new file is opened.

## Notes
- `FunctionTracer` uses `std::source_location` to include function name and file/line in logs.
- Only `LogDebug(channel)` helper is currently provided; use `Logger(LogLevel::INFO/ WARN/ ERROR, ...)` for other levels.

## License
This library is licensed under the MIT License. See `LICENSE`.
