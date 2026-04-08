from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import cmake_layout, CMake, CMakeDeps, CMakeToolchain

class uSEEDLinkToRingServerConan(ConanFile):
   name = "uSEEDLinkToRingServer"
   #version = "0.4.0"
   license = "MIT"
   description = "Propagates files from a SEEDLink client to a RingServer in the same pod in the UUSS Kubernetes environment."
   url = "https://github.com/uofuseismo/uSEEDLinkToRingServer"
   #topics = ("uSEEDLinkToRingServer")
   settings = "os", "compiler", "build_type", "arch"
   options = {"use_tbb": [True, False], "build_tests" : [True, False], "with_conan" : [True, False]}
   default_options = {"hwloc/*:shared": "True",
                      "opentelemetry-cpp/*:with_otlp_http": "True",
                      "opentelemetry-cpp/*:with_otlp_grpc": "True",
                      "opentelemetry-cpp/*:with_abi_v2" : "True",
                      "spdlog/*:header_only" : "True",
                      "use_tbb": "True",
                      "build_tests" : "True",
                      "with_conan" : "True",}
   export_sources = "CMakeLists.txt", "LICENSE", "README.md", "cmake/*", "src/*", "testing/*"
   generators = "CMakeDeps", "CMakeToolchain"

   def requirements(self):
       # dependencies
       self.requires("opentelemetry-cpp/1.24.0")
       self.requires("boost/1.89.0")
       self.requires("spdlog/1.17.0")
       self.requires("onetbb/2022.3.0")

   def build_requirements(self):
       # test dependncies and build tools
       self.test_requires("catch2/3.13.0")

   def layout(self):
       # defines the project layout
       cmake_layout(self)

   def build(self):
       # invokes the build system
       cmake = CMake(self)
       cmake.configure()
       cmake.build()
       #if can_run(self):
       #   # run tests particularly CTest 
       #   cmake.test()

   def test(self):
       if can_run(self):
          cmake.test()

   #def generate(self):
   #    tc = CMakeToolchain(self)
   #    tc.generate()

   def package(self):
       # copies files from the build to package folder
       cmake = CMake(self)
       cmake.install()

   def package_info(self):
       self.cpp_info.libs = ["uSEEDLinkToRingServer"]

