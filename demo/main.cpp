#include <string>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/process.hpp>
#include <async++.h>
#include <boost/process/mitigate.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

namespace bp = boost::process;
namespace bpi = bp::initializers;
namespace bi = boost::iostreams;
namespace po = boost::program_options;

class CmdArgs{
 public:
  std::string confrel;
  bool install;
  bool pack;
  int timeout;

  CmdArgs() : confrel("Debug"), install(false), pack(false), timeout(10) {}

  void parse(int argc, char *argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help, h", "produce help message")
            ("config",
             po::value<std::string>(&confrel)->default_value("Debug"),
             "build configuration")
                ("install",
                 "add install stage (in _install directory)")
                    ("pack",
                     "add pack stage (tar.gz)")
                        ("timeout",
                         po::value<int>(&timeout)->default_value(10),
                         "wait time (in seconds)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("install")) install = true;
    if (vm.count("pack")) pack = true;

    if (vm.count("help")) {
      std::cout << "Usage: builder [options]" << std::endl;
      std::cout << desc << std::endl;
      return;
    }

    // Check errors
    if (confrel != "Debug" && confrel != "Release")
      throw std::runtime_error("Config must be Debug|Release");

    if (timeout <= 0)
      throw std::runtime_error("Timeout can't be less 0");
  }
};

int build(const CmdArgs &cmd) {
  const std::string build1 = "cmake -H. -B_builds -DCMAKE_INSTALL_PREFIX=_install "
      "-DCMAKE_BUILD_TYPE=" + cmd.confrel;
  const std::string build_2 = "cmake --build _builds";
  const std::string build_3 = build_2 + "--target install";
  const std::string build_4 = build_2 + "--target package";

  auto const task = [&](const std::string &com, int prev_ec)
      -> int {
     if (prev_ec) return prev_ec;
    bp::child c(com, bp::std_out > stdout, bp::std_err > stderr);
    if (!c.wait_for(std::chrono::seconds(cmd.timeout))) {
      c.terminate();
      throw std::runtime_error("Wait time is over.");
    }
    c.wait();
    return c.exit_code();
  };

  auto task1 = async::spawn([&]()-> int {return task(build1, 0);});
  auto task2 = task1.then(
      ([&](int prev_ec)->int {return task(build_2, prev_ec);})
                                                                  );
  auto& task_prev = task2;
  if (cmd.install)
  task_prev = task2.then(
      ([&](int prev_ec)->int {return task(build_3, prev_ec);})
                                                                  );
  if (cmd.pack) {
    auto task4 = task_prev.then(
        [&](int prev_ec)->int { return task(build_4, prev_ec);}
                                                              );
    return task4.get();
  }
 return task_prev.get();
}

int main(int argc, char *argv[]) {
try {
  CmdArgs cmd;
  cmd.parse(argc, argv);
  return build(cmd);
} catch(std::exception &ec) {
  std::cout << ec.what() << std::endl;
  exit(EXIT_FAILURE);
} catch (...) {
  std::cout << "unknown error";
}

}