#include "app/pingpong_app.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <limits>
#include <string>

namespace {

void PrintUsage(std::ostream& os, const char* program_name) {
  os << "Usage: " << program_name << " [options]\n"
     << "Options:\n"
     << "  --rounds N            Number of ping-pong rounds (default: 10)\n"
     << "  --interval-ms N       Delay after each send in milliseconds (default: 5000)\n"
     << "  --startup-wait-ms N   Wait before clients start in milliseconds (default: 500)\n"
     << "  --duration-sec N      Stop after N seconds even if rounds are not finished\n"
     << "  --node1-port N        Port for node1 server and node2 target (default: 50051)\n"
     << "  --node2-port N        Port for node2 server and node1 target (default: 50052)\n"
     << "  --help                Show this message\n";
}

bool ParseIntValue(const std::string& option_name, const char* text, int min_value,
                   int max_value, int* out_value, std::string* error) {
  try {
    const long long parsed = std::stoll(text);
    if (parsed < min_value || parsed > max_value) {
      *error = option_name + " must be between " + std::to_string(min_value) +
               " and " + std::to_string(max_value);
      return false;
    }
    *out_value = static_cast<int>(parsed);
    return true;
  } catch (const std::exception&) {
    *error = option_name + " must be an integer";
    return false;
  }
}

bool ParseArgs(int argc, char* argv[], app::PingPongAppConfig* config,
               bool* show_help, std::string* error) {
  bool rounds_explicit = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      *show_help = true;
      return true;
    }

    if (i + 1 >= argc) {
      *error = "missing value for " + arg;
      return false;
    }

    const char* value = argv[++i];
    int parsed = 0;

    if (arg == "--rounds") {
      if (!ParseIntValue(arg, value, 1, std::numeric_limits<int>::max(), &parsed,
                         error)) {
        return false;
      }
      config->rounds = parsed;
      rounds_explicit = true;
      continue;
    }

    if (arg == "--interval-ms") {
      if (!ParseIntValue(arg, value, 0, std::numeric_limits<int>::max(), &parsed,
                         error)) {
        return false;
      }
      config->interval = std::chrono::milliseconds(parsed);
      continue;
    }

    if (arg == "--startup-wait-ms") {
      if (!ParseIntValue(arg, value, 0, std::numeric_limits<int>::max(), &parsed,
                         error)) {
        return false;
      }
      config->startup_wait = std::chrono::milliseconds(parsed);
      continue;
    }

    if (arg == "--duration-sec") {
      if (!ParseIntValue(arg, value, 1, std::numeric_limits<int>::max(), &parsed,
                         error)) {
        return false;
      }
      config->duration = std::chrono::seconds(parsed);
      continue;
    }

    if (arg == "--node1-port") {
      if (!ParseIntValue(arg, value, 1, 65535, &parsed, error)) {
        return false;
      }
      config->node1_port = parsed;
      continue;
    }

    if (arg == "--node2-port") {
      if (!ParseIntValue(arg, value, 1, 65535, &parsed, error)) {
        return false;
      }
      config->node2_port = parsed;
      continue;
    }

    *error = "unknown option: " + arg;
    return false;
  }

  if (config->node1_port == config->node2_port) {
    *error = "--node1-port and --node2-port must be different";
    return false;
  }

  if (config->duration.has_value() && !rounds_explicit) {
    config->rounds = std::numeric_limits<int>::max();
  }

  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  app::PingPongAppConfig config;
  bool show_help = false;
  std::string error;

  if (!ParseArgs(argc, argv, &config, &show_help, &error)) {
    std::cerr << "error: " << error << '\n';
    PrintUsage(std::cerr, argv[0]);
    return 1;
  }

  if (show_help) {
    PrintUsage(std::cout, argv[0]);
    return 0;
  }

  app::PingPongApp app(config);
  return app.Run();
}
