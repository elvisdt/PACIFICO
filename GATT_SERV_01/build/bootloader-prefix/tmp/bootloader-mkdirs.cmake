# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/elvis/esp/v5.3/esp-idf/components/bootloader/subproject"
  "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader"
  "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader-prefix"
  "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader-prefix/tmp"
  "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader-prefix/src/bootloader-stamp"
  "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader-prefix/src"
  "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/elvis/ESP_IDF/PACIFICO/GATT_SERV_01/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
