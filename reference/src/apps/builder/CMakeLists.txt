target_sources(Zen PRIVATE
        build_state.cpp
)

add_executable(ZenBuilder main.cpp)
target_link_libraries(ZenBuilder PRIVATE Zen)

