#include "gl.hpp"
#include <GL/glx.h>
#include <stdexcept>
#include <string>
namespace engine {
Gl::Gl() {
#define LOAD(type, name)                                                                           \
    name = reinterpret_cast<type>(                                                                 \
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("gl" #name)));                       \
    if (!name)                                                                                     \
        throw std::runtime_error("Missing OpenGL entry point: gl" #name);
    ENGINE_GL_FUNCTIONS(LOAD)
#undef LOAD
}
} // namespace engine
