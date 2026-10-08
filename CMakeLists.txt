cmake_minimum_required(VERSION 3.16)

# M4.3 consolidated firmware: build only the active main component and
# its transitive ESP-IDF dependencies. Legacy project components remain
# in history/reference but cannot enter the appliance binary.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "0.4.3")
project(prisma_pi_lite_esp32s3)
