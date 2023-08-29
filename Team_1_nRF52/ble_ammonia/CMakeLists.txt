# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(corosect-project)

target_sources(app PRIVATE src/main.c)
target_sources_ifndef(CONFIG_DISABLE_NH3 app PRIVATE src/nh3_sensor.c)
target_sources(app PRIVATE src/bluetooth.c)
target_sources(app PRIVATE src/temp.c)
target_sources(app PRIVATE src/power.c)
