if(CONFIG_BU9795)
  # Add bu9795_driver.h to the set of global include paths.
  zephyr_include_directories(.)

  zephyr_library()
  zephyr_library_sources(
    bu9795_driver.c
    )
endif()
