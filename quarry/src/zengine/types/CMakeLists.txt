target_include_directories(Zen PUBLIC 3d)

target_sources(Zen PRIVATE
        color.cpp
        line_2d.cpp
        plane_bounded.cpp
        rectangle.cpp
        shape.cpp
        vector2.cpp
)

add_subdirectory(3d)
