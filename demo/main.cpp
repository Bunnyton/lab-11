#include <boost/program_options.hpp>
#include <boost/process.hpp>
#include <string>
#include <iostream>

namespace bp = boost::process;
namespace po = boost::program_options;

struct CmdArgs {
  std::string config;
  unsigned int timeout;
  bool install;
  bool pack;
};

bool parse_cmd(int argc, char* argv[], CmdArgs& args) {
  // Add options
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help, h", "produce help message")
      ("config",
       po::value<std::string>(&args.config)->default_value("Debug"),
       "build configuration")
      ("install", "add install stage (in _install directory)")
      ("pack", "add pack stage (tar.gz)")
      ("timeout",
       po::value<unsigned int>(&args.timeout)->default_value(300),
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

  return true;
}

int main(int argc, char* argv[]) {
  try {
    CmdArgs args;

    if(!parse_cmd(argc, argv, args))
      return 0;

    std::string command = "ls";
    int res = bp::system(command);

    std::cout << static_cast<bool>(res) << std::endl;
  }
  catch (std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl;
    return 1;
  }
  catch (...) {
    std::cout << "Exception: Unknown error!" << std::endl;
    return 1;
  }

  return 0;
}