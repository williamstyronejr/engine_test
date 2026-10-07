#pragma once
#include <GL/glcorearb.h>

#define ENGINE_GL_FUNCTIONS(X)                                                                     \
    X(PFNGLGETSTRINGPROC, GetString)                                                               \
    X(PFNGLGETINTEGERVPROC, GetIntegerv)                                                           \
    X(PFNGLGETQUERYIVPROC, GetQueryiv)                                                             \
    X(PFNGLGENQUERIESPROC, GenQueries)                                                             \
    X(PFNGLDELETEQUERIESPROC, DeleteQueries)                                                       \
    X(PFNGLQUERYCOUNTERPROC, QueryCounter)                                                         \
    X(PFNGLGETQUERYOBJECTUIVPROC, GetQueryObjectuiv)                                               \
    X(PFNGLGETQUERYOBJECTUI64VPROC, GetQueryObjectui64v)                                           \
    X(PFNGLGETERRORPROC, GetError)                                                                 \
    X(PFNGLCLEARCOLORPROC, ClearColor)                                                             \
    X(PFNGLCLEARPROC, Clear)                                                                       \
    X(PFNGLVIEWPORTPROC, Viewport)                                                                 \
    X(PFNGLENABLEPROC, Enable)                                                                     \
    X(PFNGLDISABLEPROC, Disable)                                                                   \
    X(PFNGLSCISSORPROC, Scissor)                                                                   \
    X(PFNGLBLENDFUNCSEPARATEPROC, BlendFuncSeparate)                                               \
    X(PFNGLGETPROGRAMINTERFACEIVPROC, GetProgramInterfaceiv)                                       \
    X(PFNGLGETPROGRAMRESOURCEINDEXPROC, GetProgramResourceIndex)                                   \
    X(PFNGLGETPROGRAMRESOURCEIVPROC, GetProgramResourceiv)                                         \
    X(PFNGLPROGRAMUNIFORM1IPROC, ProgramUniform1i)                                                 \
    X(PFNGLUNIFORM1IPROC, Uniform1i)                                                               \
    X(PFNGLGENFRAMEBUFFERSPROC, GenFramebuffers)                                                   \
    X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer)                                                   \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D)                                         \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, CheckFramebufferStatus)                                     \
    X(PFNGLDELETEFRAMEBUFFERSPROC, DeleteFramebuffers)                                             \
    X(PFNGLBLENDFUNCPROC, BlendFunc)                                                               \
    X(PFNGLCREATESHADERPROC, CreateShader)                                                         \
    X(PFNGLSHADERSOURCEPROC, ShaderSource)                                                         \
    X(PFNGLCOMPILESHADERPROC, CompileShader)                                                       \
    X(PFNGLGETSHADERIVPROC, GetShaderiv)                                                           \
    X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog)                                                 \
    X(PFNGLDELETESHADERPROC, DeleteShader)                                                         \
    X(PFNGLCREATEPROGRAMPROC, CreateProgram)                                                       \
    X(PFNGLDETACHSHADERPROC, DetachShader)                                                         \
    X(PFNGLATTACHSHADERPROC, AttachShader)                                                         \
    X(PFNGLLINKPROGRAMPROC, LinkProgram)                                                           \
    X(PFNGLGETPROGRAMIVPROC, GetProgramiv)                                                         \
    X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog)                                               \
    X(PFNGLDELETEPROGRAMPROC, DeleteProgram)                                                       \
    X(PFNGLUSEPROGRAMPROC, UseProgram)                                                             \
    X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays)                                                   \
    X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray)                                                   \
    X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays)                                             \
    X(PFNGLGENBUFFERSPROC, GenBuffers)                                                             \
    X(PFNGLBINDBUFFERPROC, BindBuffer)                                                             \
    X(PFNGLBUFFERDATAPROC, BufferData)                                                             \
    X(PFNGLBUFFERSUBDATAPROC, BufferSubData)                                                       \
    X(PFNGLDELETEBUFFERSPROC, DeleteBuffers)                                                       \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray)                                   \
    X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer)                                           \
    X(PFNGLDRAWARRAYSPROC, DrawArrays)                                                             \
    X(PFNGLGENTEXTURESPROC, GenTextures)                                                           \
    X(PFNGLBINDTEXTUREPROC, BindTexture)                                                           \
    X(PFNGLTEXIMAGE2DPROC, TexImage2D)                                                             \
    X(PFNGLTEXPARAMETERIPROC, TexParameteri)                                                       \
    X(PFNGLDELETETEXTURESPROC, DeleteTextures)                                                     \
    X(PFNGLACTIVETEXTUREPROC, ActiveTexture)                                                       \
    X(PFNGLREADPIXELSPROC, ReadPixels)                                                             \
    X(PFNGLREADBUFFERPROC, ReadBuffer)                                                             \
    X(PFNGLPIXELSTOREIPROC, PixelStorei)                                                           \
    X(PFNGLDEBUGMESSAGECALLBACKPROC, DebugMessageCallback)

namespace engine {
struct Gl {
#define DECLARE(type, name) type name{};
    ENGINE_GL_FUNCTIONS(DECLARE)
#undef DECLARE
    Gl();
};
} // namespace engine
