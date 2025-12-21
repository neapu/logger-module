module;
#include <sstream>
#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <source_location>
#include <chrono>
#include <map>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <atomic>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#include <thread>

export module Logger;

export namespace logger {
enum class LogLevel {
    NONE = 0,
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    DEBUG = 4
};

class Logger {
public:
    Logger(LogLevel level, const std::string& channel = std::string{}, const std::source_location& location = std::source_location::current());
    ~Logger();

    Logger& operator<<(const char* s);
    Logger& operator<<(std::string_view sv);
    Logger& operator<<(const std::string& s);
    template <class T>
    Logger& operator<<(T&& t)
    {
        m_data << std::forward<T>(t);
        return *this;
    }

    template<typename... Args>
    Logger& format(std::format_string<Args...> fmt, Args&&... args)
    {
        auto formatted = std::format(fmt, std::forward<Args>(args)...);
        m_data << std::string_view(formatted);
        return *this;
    }

    static void setLogPath(const std::string& path);
    static void openNewFile(const std::string& channel);
    static void setFileSizeThreshold(std::streamoff bytes);
    static void rotateIfNeeded(const std::string& channel);
    static void setPrintLevel(LogLevel level) { s_printLevel = level; }
    static void setLogLevel(LogLevel level) { s_logLevel = level; }
    static void pureLog(LogLevel level, const std::string& channel, const std::string& message);
    static void setLockingEnabled(bool enabled);
private:
    std::stringstream m_data;
    LogLevel m_level;
    std::source_location m_location;
    std::string m_channel;
    static std::string s_logPath;
    static std::map<std::string, std::ofstream> s_ofstreamMap;
    static std::streamoff s_maxFileSize;
    static LogLevel s_printLevel;
    static LogLevel s_logLevel;
    static std::recursive_mutex s_mutex;
    static std::atomic<bool> s_lockEnabled;
};
Logger LogDebug(const std::string& channel = std::string{}, const std::source_location& location = std::source_location::current());
Logger LogInfo(const std::string& channel = std::string{}, const std::source_location& location = std::source_location::current());
Logger LogWarn(const std::string& channel = std::string{}, const std::source_location& location = std::source_location::current());
Logger LogError(const std::string& channel = std::string{}, const std::source_location& location = std::source_location::current());

class FunctionTracer {
public:
    explicit FunctionTracer(LogLevel level = LogLevel::INFO, const std::string& channel = std::string{}, const std::source_location& location = std::source_location::current());
    ~FunctionTracer();
private:
    LogLevel m_level;
    std::string m_channel;
    std::source_location m_location;
    std::string m_logPrefix;
};
}

module: private;
unsigned long getProcessId()
{
#ifdef _WIN32
    return static_cast<unsigned long>(_getpid());
#else
    return static_cast<unsigned long>(getpid());
#endif
}
std::string getTimeString()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const zoned_time zt{current_zone(), now};
    const auto local_tp = zt.get_local_time();
    const auto sec_tp = floor<seconds>(local_tp);
    const auto ms = duration_cast<milliseconds>(local_tp - sec_tp).count();
    return std::format("{:%F %T}.{:03}", sec_tp, ms);
}

std::string levelToString(logger::LogLevel level)
{
    using enum logger::LogLevel;
    switch (level) {
    case ERROR: return "ERROR";
    case WARN: return "WARN";
    case INFO: return "INFO";
    case DEBUG: return "DEBUG";
    default: return std::string{};
    }
}

namespace logger {
std::string Logger::s_logPath{};
std::map<std::string, std::ofstream> Logger::s_ofstreamMap{};
std::streamoff Logger::s_maxFileSize = 1024 * 1024;
LogLevel Logger::s_printLevel = LogLevel::DEBUG;
LogLevel Logger::s_logLevel = LogLevel::NONE;
std::recursive_mutex Logger::s_mutex{};
std::atomic<bool> Logger::s_lockEnabled{true};
Logger::Logger(LogLevel level, const std::string& channel, const std::source_location& location)
    : m_level(level), m_location(location), m_channel(channel)
{
    if (m_channel.empty()) {
        m_channel = "Default";
    }
}
Logger::~Logger()
{
    if (m_data.str().empty() || m_level == LogLevel::NONE) {
        return;
    }
    if (m_level > s_printLevel && (s_logPath.empty() || m_level > s_logLevel)) {
        return;
    }
    std::stringstream ss;
    ss << "[" << getTimeString() << "]";
    ss << "[" << m_channel << "]";
    ss << "[" << levelToString(m_level) << "]";

    auto threadId = std::this_thread::get_id();

    ss << "[TID:" << threadId << "]";

    const auto fileName = std::filesystem::path(m_location.file_name()).filename().string();
    ss << "[" << fileName << ":" << m_location.line() << "]";

    pureLog(m_level, m_channel, ss.str() + m_data.str());
}

Logger& Logger::operator<<(const char* s)
{
    if (s) {
        m_data.write(s, static_cast<std::streamsize>(std::char_traits<char>::length(s)));
    } else {
        m_data << "(null)";
    }
    return *this;
}

Logger& Logger::operator<<(std::string_view sv)
{
    m_data.write(sv.data(), static_cast<std::streamsize>(sv.size()));
    return *this;
}

Logger& Logger::operator<<(const std::string& s)
{
    m_data.write(s.data(), static_cast<std::streamsize>(s.size()));
    return *this;
}
void Logger::setLogPath(const std::string& path)
{
    s_logPath = path;
    if (s_logPath.ends_with('/') || s_logPath.ends_with('\\')) {
        s_logPath.pop_back();
    }
    std::error_code ec;
    std::filesystem::create_directories(s_logPath, ec);
}
void Logger::openNewFile(const std::string& channel)
{
    std::unique_lock<std::recursive_mutex> lock;
    if (s_lockEnabled.load()) {
        lock = std::unique_lock<std::recursive_mutex>(s_mutex);
    }
    std::ofstream os;
    os.open(s_logPath + "/" + channel + ".log", std::ios::out | std::ios::app);
    if (!os.is_open()) {
        return;
    }
    os << "[" << getTimeString() << "][HEADER][PID:" << getProcessId() << "]" << std::endl;
    s_ofstreamMap.emplace(channel, std::move(os));
}
void Logger::setFileSizeThreshold(std::streamoff bytes)
{
    s_maxFileSize = bytes;
}
void Logger::rotateIfNeeded(const std::string& channel)
{
    std::unique_lock<std::recursive_mutex> lock;
    if (s_lockEnabled.load()) {
        lock = std::unique_lock<std::recursive_mutex>(s_mutex);
    }
    auto it = s_ofstreamMap.find(channel);
    if (it == s_ofstreamMap.end()) {
        return;
    }
    if (it->second.tellp() <= s_maxFileSize) {
        return;
    }
    it->second.close();
    s_ofstreamMap.erase(it);
    using namespace std::chrono;
    const auto now = system_clock::now();
    const zoned_time zt{current_zone(), now};
    const auto local_tp = zt.get_local_time();
    const auto sec_tp = floor<seconds>(local_tp);
    const std::string timestamp = std::format("{:%Y%m%d%H%M%S}", sec_tp);
    const std::string baseName = s_logPath + "/" + channel + ".log";
    const std::string oldDir = s_logPath + "/old";
    std::error_code ec;
    std::filesystem::create_directories(oldDir, ec);
    const std::string newName = oldDir + "/" + channel + "_" + timestamp + ".log";
    std::filesystem::rename(baseName, newName, ec);
    openNewFile(channel);
}
void Logger::pureLog(LogLevel level, const std::string& channel, const std::string& message)
{
    // print to console
    if (level <= s_printLevel && s_printLevel != LogLevel::NONE) {
        std::cout << message << std::endl;
    }

    // log to file
    if (s_logPath.empty() || level > s_logLevel || s_logLevel == LogLevel::NONE) {
        return;
    }

    std::unique_lock<std::recursive_mutex> lock;
    if (s_lockEnabled.load()) {
        lock = std::unique_lock<std::recursive_mutex>(s_mutex);
    }

    auto it = s_ofstreamMap.find(channel);
    if (it == s_ofstreamMap.end()) {
        openNewFile(channel);
        it = s_ofstreamMap.find(channel);
        if (it == s_ofstreamMap.end()) {
            return;
        }
    }

    rotateIfNeeded(channel);
    it = s_ofstreamMap.find(channel);
    if (it == s_ofstreamMap.end()) {
        return;
    }

    it->second << message << std::endl;
}
void Logger::setLockingEnabled(bool enabled)
{
    s_lockEnabled.store(enabled);
}
Logger LogDebug(const std::string& channel, const std::source_location& location)
{
    return Logger(LogLevel::DEBUG, channel, location);
}
Logger LogInfo(const std::string& channel, const std::source_location& location)
{
    return Logger(LogLevel::INFO, channel, location);
}
Logger LogWarn(const std::string& channel, const std::source_location& location)
{
    return Logger(LogLevel::WARN, channel, location);
}
Logger LogError(const std::string& channel, const std::source_location& location)
{
    return Logger(LogLevel::ERROR, channel, location);
}
FunctionTracer::FunctionTracer(LogLevel level, const std::string& channel, const std::source_location& location)
    : m_level(level), m_channel(channel), m_location(location)
{
    Logger(level, channel, location) << "[ENTER][" << m_location.function_name() << "]";
}
FunctionTracer::~FunctionTracer()
{
    Logger(m_level, m_channel, m_location) << "[EXIT][" << m_location.function_name() << "]";
}
} // namespace logger
