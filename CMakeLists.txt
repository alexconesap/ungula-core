# Ungula Generic Libraries
# Copyright © 2026, Alex Conesa
# MIT License

cmake_minimum_required(VERSION 3.15)

if(ESP_PLATFORM)
	# Build UngulaCore as an ESP-IDF component
	idf_component_register(INCLUDE_DIRS src)
	return()
endif()

project(UngulaCore VERSION 7.0.4)

add_subdirectory(src)
