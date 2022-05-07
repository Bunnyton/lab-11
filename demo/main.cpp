#include <string>
#include <iostream>
#include <async++.h>
#include <boost/process.hpp>
#include <boost/program_options.hpp>

namespace bp = boost::process;
namespace po = boost::program_options;

struct CmdArgs {
  std::string config;
  int timeout;
  bool install;
  bool pack;
};

bool parse_cmd(int argc, char* argv[], CmdArgs& args);
int build(CmdArgs& args);

int main(int argc, char* argv[]) {
  try {
    // Parse command line
    CmdArgs args;
    if (!parse_cmd(argc, argv, args)) return 0;

    // Run build
    return build(args);

  } catch (std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cout << "Exception: Unknown error!" << std::endl;
    return 1;
  }
}

bool parse_cmd(int argc, char* argv[], CmdArgs& args) {
  // Add options
  po::options_description desc("Allowed options");
  desc.add_options()("help, h", "produce help message")(
      "config", po::value<std::string>(&args.config)->default_value("Debug"),
      "build configuration")
      ("install",
       "add install stage (in _install directory)")
      ("pack", "add pack stage (tar.gz)")(
      "timeout", po::value<int>(&args.timeout)->default_value(30),
      "wait time (in seconds)");

  // Parse arguments
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  args.install = vm.count("install");
  args.pack = vm.count("pack");

  if (vm.count("help")) {
    std::cout << "Usage: builder [options]" << std::endl;
    std::cout << desc << std::endl;
    return false;
  }

  // Check errors
  if (args.config != "Debug" && args.config != "Release") {
    std::cout << "Config should be Debug|Release, but it is "
              << args.config << std::endl;
    return false;
  }

  if (args.timeout <= 0) {
    std::cout << "Timeout can't be less or equal 0." << std::endl;
    return false;
  }

  return true;
}

int build(CmdArgs& args) {
  // Processes commands
  const std::string build_1 =
      "cmake -H. -B_builds -DCMAKE_INSTALL_PREFIX=_install "
      "-DCMAKE_BUILD_TYPE=" +
      args.config;
  const std::string build_2 = "cmake --build _builds";
  const std::string install = "cmake --build _builds --target install";
  const std::string pack = "cmake --build _builds --target package";

  // Vectors for commands and processes
  std::vector<std::string> script;
  std::vector<async::task<int>> events;
  async::task<int>* task_ptr;

  // Lambda for launch process
  auto const task = [&args](const std::string& command, int prev_ec) -> int {
    if (prev_ec) return prev_ec;
    bp::child c(command, bp::std_out > stdout, bp::std_err > stderr);
    if (!c.wait_for(std::chrono::seconds(args.timeout))) {
      c.terminate();
      throw std::runtime_error("Wait time is over.");
    }
    c.wait();
    return c.exit_code();
  };

  // Add commands in script
  script.push_back(build_1);
  script.push_back(build_2);
  if (args.install) script.push_back(install);
  if (args.pack) script.push_back(pack);

  // Launch events chain
  events.push_back(
      async::spawn(
          [task, &script]() -> int {
            return task(script[0], 0);
          }
      )
  );

  for (size_t i = 1; i < script.size(); ++i) {
    task_ptr = &events.back();
    events.push_back(
        task_ptr->then(
            [task, &script, i](int prev_ec) -> int {
              return task(script[i], prev_ec);
            }
        )
    );
  }

  // Wait exit code
  return events.back().get();
}
