# CMakeList.txt: my_all_total_program 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# 将源代码添加到此项目的可执行文件。
include_directories(include)
#add_executable (my_all_total_program "src/my_all_total_program.cpp")
file(GLOB My_Source_Files src/*.cpp)
add_executable(my_all_total_program ${My_Source_Files} "include/pfld.h" "include/ort_utils.h" "include/ort_config.h" "src/ort_utils.cpp"   "include/constants.h" "src/ort_basehandler.cpp" "include/utils.h" "src/utils.cpp" "include/ssrnet.h" "src/ssrnet.cpp" "include/fsanet.h" "src/fsanet.cpp" "include/FaceParsingBiSeNet.h" "src/FaceParsingBiSeNet.cpp" "include/arcface.h" "src/arcface.cpp" "include/hello.h" "src/hello.cpp" "src/ncnn_basehandle.cpp" "include/pfld_ncnn.h" "src/pfld_ncnn.cpp")


find_package(OpenCV REQUIRED)
find_library(ONNXRUNTIME_LIB onnxruntime PATHS "F:/onnxruntime-win-x64-1.9.0/lib")
find_library(NCNN_LIB ncnn PATHS "E:/ncnn-20230816-full-source/build/install/lib")

set(onnxruntime_INCLUDE_DIRS F:/onnxruntime-win-x64-1.9.0/include)
set(ncnn_INCLUDE_DIRS E:/ncnn-20230816-full-source/build/install/include)

include_directories(${onnxruntime_INCLUDE_DIRS} ${ncnn_INCLUDE_DIRS})
include_directories(include)
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(my_all_total_program ${OpenCV_LIBS} ${ONNXRUNTIME_LIB} ${NCNN_LIB})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET my_all_total_program PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。
