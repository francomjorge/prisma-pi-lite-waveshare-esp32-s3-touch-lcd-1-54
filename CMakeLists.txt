cmake_minimum_required(VERSION 3.16)

# Consolidated production firmware: build only the active main component and
# its transitive ESP-IDF dependencies.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "1.0.0")
project(prisma_pi_lite_esp32s3)
