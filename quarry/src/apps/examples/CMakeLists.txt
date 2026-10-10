add_executable(Vector2_TEST vector2_examples.cpp)
target_link_libraries(Vector2_TEST PRIVATE Zen)

add_executable(Timer_TEST timer_examples.cpp)
target_link_libraries(Timer_TEST PRIVATE Zen)

add_executable(VarStorage_TEST var_storage_examples.cpp)
target_link_libraries(VarStorage_TEST PRIVATE Zen)

add_executable(MessageBus_TEST message_bus_example.cpp)
target_link_libraries(MessageBus_TEST PRIVATE Zen)
