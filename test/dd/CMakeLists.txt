if(TARGET MQT::CoreDD)
  file(GLOB_RECURSE DD_TEST_SOURCES *.cpp)
  package_add_test(mqt-core-dd-test MQT::CoreDD ${DD_TEST_SOURCES})
  target_link_libraries(mqt-core-dd-test PRIVATE MQT::CoreCircuitOptimizer)
endif()
