# For the web simulator: SDL3 is Emscripten's port (-sUSE_SDL=3, set by
# PlayShelf's CMakeLists.txt), so the target links nothing.
if (NOT TARGET SDL3::SDL3)
    add_library(SDL3::SDL3 INTERFACE IMPORTED)
endif ()
set(SDL3_FOUND TRUE)
