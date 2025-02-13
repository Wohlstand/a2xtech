/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef RenderGL_T_H
#define RenderGL_T_H

#include "core/opengl/gl_inc.h"

#    ifdef RENDERGL_SUPPORTED

#include <utility>
#include <vector>
#include <array>
#include <set>
#include <map>
#include <unordered_map>

#include <SDL2/SDL_render.h>

#include "core/render_planes.h"

#include "core/opengl/gl_geom.h"

#include "core/base/render_base.h"
#include "cmd_line_setup.h"
#include "config.h"

#include "core/opengl/gl_program_object.h"
#include "core/opengl/gl_light_info.h"

struct StdPicture;
struct SDL_Window;

/*!
 * \brief Represents one of the game draw/read buffer textures
 *
 * RenderGL::framebufferCopy is used to copy contents from one buffer to another
 */
enum BufferIndex_t
{
    BUFFER_GAME = 0,   /**< main texture for the game */
    BUFFER_FB_READ,    /**< auxiliary texture used for reading from main buffer in shader */
    BUFFER_INT_PASS_1, /**< auxiliary target used to render an intermediate pass during multipass rendering */
    BUFFER_INT_PASS_2, /**< auxiliary target used to render an intermediate pass during multipass rendering */
    BUFFER_DEPTH_READ, /**< auxiliary texture used to copy the depth from the initial render pass */
    BUFFER_LIGHTING,   /**< texture used to store the outcome of the lighting calculations */
    BUFFER_MAX,
};

#ifdef RENDERGL_HAS_FBO
/*!
 * \brief Readable aliases for the texture units
 */
enum TextureUnit_t
{
    TEXTURE_UNIT_IMAGE = GL_TEXTURE0, /**< texture unit for the currently bound StdPicture (default) */
    TEXTURE_UNIT_FB_READ,             /**< texture unit for reading from the FB read buffer (set once) */
    TEXTURE_UNIT_MASK,                /**< texture unit for emulating bitmask rendering (set per bitmasked render) */
    TEXTURE_UNIT_PREV_PASS,           /**< texture unit for reading from the previous render pass (first set to game, and then most recent int pass) */
    TEXTURE_UNIT_DEPTH_READ,          /**< texture unit for reading depth from the initial render pass */
    TEXTURE_UNIT_LIGHT_READ,          /**< texture unit for reading lighting state (set once) */
};
#endif

class RenderGL final : public AbstractRender_t
{
private:
    /*!
     * \brief Takes GL profile int, returns profile string
     */
    static inline const char* get_profile_name(const GLint profile)
    {
        switch(profile)
        {
        case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY:
            return "Compatibility";
        case SDL_GL_CONTEXT_PROFILE_CORE:
            return "Core";
        case SDL_GL_CONTEXT_PROFILE_ES:
            return "ES";
        case 0:
            return "<Auto>";
        default:
            return "";
        }
    }

    static inline char get_profile_letter(const GLint profile)
    {
        switch(profile)
        {
        case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY:
            return 'P';
        case SDL_GL_CONTEXT_PROFILE_CORE:
            return 'R';
        case SDL_GL_CONTEXT_PROFILE_ES:
            return 'E';
        default:
            return ' ';
        }
    }

    // unused for now
    // enum class SupportLevel
    // {
    //     none,
    //     core,
    //     oes,
    //     ext,
    // };

    /*!
     * \struct Vertex_t
     * \brief Represents a single entry in a vertex array
     * \param position: 3 int16_ts in viewport coordinates
     * \param tint: 4 uint8_ts
     * \param texcoord: 2 floats in GL texture-normalized coordinates (not picture coordinates if non-power-of-two)
     */
    struct Vertex_t
    {
        using Tint = std::array<GLubyte, 4>;

        std::array<GLshort, 3> position;
        Tint tint;
        std::array<GLfloat, 2> texcoord;
    };

    /*!
     * \brief Represents a vertex array that can be rendered in a single glDrawArrays call
     * \param vertices: vector of vertices, frequently resized to zero without deallocation for efficiency
     * \param active: whether this vertex array was non-empty at any flushDrawQueues() since the last cleanupDrawQueues() call
     */
    struct VertexList
    {
        std::vector<Vertex_t> vertices;
        bool active = false;
    };

    /*!
     * \brief Represents the draw state needed for a single glDrawArrays call
     * \param program: non-nullable pointer to the program object for the draw call
     * \param uniform_step: integer index to the index of the program object's current uniform state, indexed by times that uniforms were set
     * \param texture: nullable pointer to a texture for the draw call
     */
    struct DrawContext_t
    {
        GLProgramObject* program = nullptr;
        int uniform_step = 0;
        StdPicture* texture = nullptr;

        constexpr DrawContext_t(std::nullptr_t)
            : program(nullptr), uniform_step(0), texture(nullptr) {}

        inline DrawContext_t(GLProgramObject& program)
            : program(&program), uniform_step(program.get_uniform_step()), texture(nullptr) {}

        inline DrawContext_t(GLProgramObject& program, StdPicture* texture)
            : program(&program), uniform_step(program.get_uniform_step()), texture(texture) {}

        constexpr bool operator==(const DrawContext_t& o) const noexcept
        {
            return texture == o.texture && program == o.program && uniform_step == o.uniform_step;
        }

        constexpr bool operator<(const DrawContext_t& o) const noexcept
        {
            return (program == o.program && uniform_step == o.uniform_step && texture < o.texture) || program < o.program || (program == o.program && uniform_step < o.uniform_step);
        }
    };

    /*!
     * \brief Utility struct to allow DrawContext_t to be used as a key in a hash-map
     */
    struct hash_DrawContext
    {
        std::size_t operator()(const RenderGL::DrawContext_t& c) const noexcept
        {
            return std::hash<GLProgramObject*>()(c.program) ^ (std::hash<StdPicture*>()(c.texture) >> 1) ^ (std::hash<int>()(c.uniform_step) >> 2);
        }
    };

#ifdef RENDERGL_HAS_SHADERS

    struct LightBuffer
    {
        GLLightSystem header;
        std::array<GLLight, 256> lights;
    };

#endif // #ifdef RENDERGL_HAS_SHADERS


    // state supporting the draw queues

    // unsorted draw queue that stores opaque draw calls in the current viewport state
    std::unordered_map<DrawContext_t, VertexList, hash_DrawContext> m_unordered_draw_queue;

    // sorted draw queue that stores translucent / advanced draw calls in the current viewport state
    std::map<std::pair<int, DrawContext_t>, VertexList> m_ordered_draw_queue;

    // state used to group subsequent ordered draws, even if their depths are not exactly the same
    std::unordered_map<DrawContext_t, int, hash_DrawContext> m_mask_draw_context_depth;
    DrawContext_t m_recent_draw_context = {nullptr};
    int m_recent_draw_context_depth = 0;



    // state supporting public render functionality

    // reference to currently active SDL window
    SDL_Window    *m_window = nullptr;

    // reference to SDL OpenGL context
    SDL_GLContext m_gContext = nullptr;

    // tracks whether the renderer is currently drawing to the screen (instead of the game texture)
    bool           m_recentTargetScreen = false;

    // set of all currently-loaded textures
    std::set<StdPicture *> m_loadedPictures;

    // Current scaling mode
    int m_current_scale_mode = Config_t::SCALE_DYNAMIC_NEAREST;

    // Offset to shake screen
    PointI m_viewport_offset = PointI(0, 0);
    // Keep zero viewport offset while this flag is on
    bool m_viewport_offset_ignore = false;

    //Need to calculate relative hardware viewport position when screen was scaled
    int m_phys_x = 0;
    int m_phys_y = 0;
    int m_phys_w = 0;
    int m_phys_h = 0;

    //Need to calculate in-game vScreen position
    RectSizeI m_viewport = RectSizeI(0, 0, 0, 0);

    //Need for HiDPI rendering (number of draw pixels per cursor pixel)
    float m_hidpi_x = 1.0f;
    float m_hidpi_y = 1.0f;



    // internal capability trackers
    GLint m_gl_majver = 0;
    GLint m_gl_minver = 0;
    GLint m_gl_profile = 0;

    bool m_use_logicop = false;
    bool m_use_shaders = false;
    bool m_has_fbo = false;
    bool m_use_depth_buffer = false;
    bool m_client_side_arrays = false;

    bool m_has_es3_shaders = false;
    bool m_has_npot_textures = false;
    bool m_has_bgra_textures = false;


    // preferences

    float m_render_scale_factor = 1.0f;
    float m_lighting_scale_factor = 0.5f;



    // OpenGL state

    // depth of most recent draw in each plane (reset per frame)
    RenderPlanes_t m_render_planes;

    // tick for current frame
    uint64_t m_current_frame = 0;



    // framebuffer object (FBO) state

    // note: {} value-initializes array to zeros

    // textures for each render-to-texture layer
    std::array<GLuint, BUFFER_MAX> m_buffer_texture{};
    // FBOs for each render-to-texture layer (only the first is required)
    std::array<GLuint, BUFFER_MAX> m_buffer_fb{};
    // texture for main game framebuffer's depth component (preferred)
    GLuint m_game_depth_texture = 0;
    // fallback: renderbuffer for main game framebuffer's depth component
    GLuint m_game_depth_rb = 0;

    // references to main game framebuffer's texture and FBO
    const GLuint& m_game_texture = m_buffer_texture[BUFFER_GAME];
    const GLuint& m_game_texture_fb = m_buffer_fb[BUFFER_GAME];
    // texture for depth buffer read
    const GLuint& m_depth_read_texture = m_buffer_texture[BUFFER_DEPTH_READ];

    // tracks the current pass render target during multipass rendering (as a buffer index), always BUFFER_GAME except during flushDrawQueues()
    BufferIndex_t m_cur_pass_target = BUFFER_GAME;



    // vertex buffer object (VBO) and vertex array object (VAO) state

    // tuned carefully for performance on Mac and Emscripten, which require using VBOs
    static constexpr int s_num_vertex_buffers = 128;

    // VBOs and their currently initialized sizes
    GLuint m_vertex_buffer[s_num_vertex_buffers] = {0};
    GLsizeiptr m_vertex_buffer_size[s_num_vertex_buffers] = {0};

    // most recently filled VBO
    int m_cur_vertex_buffer_index = 0;

#ifdef RENDERGL_HAS_VAO
    // Single VAO (only used in GL Core), never changed. Only touched during init / cleanup functions.
    GLuint m_glcore_vao = 0;
#endif


    // some queue flags, used at flushDrawQueues()
    bool m_drawQueued = false;



    // shader state

    // current transform matrix
    std::array<GLfloat, 16> m_transform_matrix;

    // representation of viewport (vScreen) to allow shaders to transform between draw coordinates and framebuffer texture coordinates
    std::array<GLfloat, 4> m_shader_read_viewport;

    // clock variable in seconds for shaders (loops at 60s)
    GLfloat m_shader_clock = 0.0f;

    // number of times the above have been set (used to limit number of uniform update calls)
    uint64_t m_transform_tick = 0;

    // OpenGL program objects
    GLProgramObject m_standard_program;
    GLProgramObject m_no_tint_program;
    GLProgramObject m_bitmask_program;
    GLProgramObject m_output_program;
    GLProgramObject m_program_rect_filled;
    GLProgramObject m_program_rect_unfilled;
    GLProgramObject m_program_circle;
    GLProgramObject m_program_circle_hole;



    // extra state supporting the lighting system

#ifdef RENDERGL_HAS_SHADERS

    // performs the first distance field calculation pass
    GLProgramObject m_distance_field_1_program;

    // performs the remaining distance field calculation passes
    GLProgramObject m_distance_field_2_program;

    // performs the light calculation pass
    GLProgramObject m_lighting_calc_program;

    // applies the calculated light to the framebuffer
    GLProgramObject m_lighting_apply_program;

    // blank 1x1 texture for programs using light to reference when light is disabled
    GLuint m_null_light_texture = 0;

    // UBO to support the lighting queue
    GLuint m_light_ubo = 0;

    // filled with lights over the course of a render queue fill
    LightBuffer m_light_queue;

    // tracks how many lights have been added in the current frame
    int m_light_count = 0;



    // Source for builtin shaders, defined at render_gl_shaders.cpp
    static const char* const s_es2_standard_vert_src;
    static const char* const s_es2_advanced_vert_src;

    static const char* const s_es3_standard_vert_src;
    static const char* const s_es3_advanced_vert_src;

    static const char* const s_es2_standard_frag_src;
    static const char* const s_es2_no_tint_frag_src;
    static const char* const s_es2_bitmask_frag_src;
    static const char* const s_es3_bitmask_frag_src;

    static const char* const s_es2_rect_filled_frag_src;
    static const char* const s_es2_rect_unfilled_frag_src;
    static const char* const s_es2_circle_frag_src;
    static const char* const s_es2_circle_hole_frag_src;

    static const char* const s_es2_lighting_apply_frag_src;

    static const char* const s_es3_distance_field_1_frag_src;
    static const char* const s_es3_distance_field_2_frag_src;
    static const char* const s_es3_lighting_calc_frag_src;

#endif // #ifdef RENDERGL_HAS_SHADERS


    /***********************************************************\
    || Initialization functions, defined at render_gl_init.cpp ||
    \***********************************************************/

    // For all:
    // Returning false indicates a catastrophic failure and results in the OpenGL engine not being created
    // Otherwise, instance flags will be set to reflect actual capabilities

    /*!
     * \brief Helper function to attempt to initialize an SDL OpenGL context of the desired profile and version
     * \param[inout] context: reference to nullable SDL_GLContext. If non-null, no-op (already inited). If null, attempts initialization and signals success by leaving context non-null.
     * \param[in] window: SDL_Window to use for the context
     * \param profile: OpenGL profile to initialize
     * \param majver: minimum OpenGL major version to initialize
     * \param minver: minimum OpenGL minor version to initialize
     */
    static void try_init_gl(SDL_GLContext& context, SDL_Window* window, GLint profile, GLint majver, GLint minver, Config_t::RenderMode_t mode);

    // initializes a single framebuffer with the game's current screen resolution scaled by the appropriate scaling factor
    // if a render target (BUFFER_GAME, BUFFER_INT_PASS_1, BUFFER_INT_PASS_2), will include depth
    void createFramebuffer(BufferIndex_t buffer);

    // destroys a single framebuffer
    void destroyFramebuffer(BufferIndex_t buffer);

    /*!
     * \brief Initialize the SDL OpenGL bindings according to version preferences and compile-time support, sets the version, and sets profile/version-dependent flags
     * \returns false no supported OpenGL profile can be initialized
     */
    bool initOpenGL();

    /*!
     * \brief Initialize the OpenGL debug bindings if requested and possible
     */
    bool initDebug();

    /*!
     * \brief Initialize the global shader objects (also detects ES 2/3 shader support)
     * \returns false if ES 2 shaders cannot be successfully compiled, but fixed-function pipeline is also unavailable
     */
    bool initShaders();

    /*!
     * \brief Initializes the game texture and framebuffer, the shader read texture, and the multipass support framebuffers
     *
     * No-op if FBO is unsupported
     *
     * Only attempts to initialize shader read buffer if using shaders to emulate logic ops or supporting user shaders
     * Only attempts to initialize other auxiliary buffers if supporting user shaders
     */
    bool initFramebuffers();

    /*!
     * \brief Setup the clear functions, blending, alpha test (legacy), and depth test
     *
     * Clear functions: sets the clear color and depth
     * Blending: sets standard alpha blending
     * Alpha test: in fixed-function pipeline, enables alpha test with 0x08 cutoff
     * Depth test: enables depth test by default and sets the pass function to >= (greater-than-or-equal)
     */
    bool initState();

    /*!
     * \brief Enable vertex arrays
     *
     * allocate VBOs if required (Core/Emscripten) or requested and possible
     * also sets up the VAO if required (Core) or requested and possible
     *
     * \returns false if required VBO/VAO cannot be created
     */
    bool initVertexArrays();


    /***********************************************************************\
    || Private draw management functions, defined at render_gl_backend.cpp ||
    \***********************************************************************/

    // normalizes a rect to fully fit within the (0, 0, TargetW, TargetH) box
    static void s_normalize_coords(RectSizeI& r);

    // shortcut to the correct glOrtho / glOrthof function
    void m_Ortho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far);

    // perform a framebuffer->framebuffer copy
    void framebufferCopy(BufferIndex_t dest, BufferIndex_t source, RectSizeI r);

    // perform a full depth buffer copy to the depth read buffer (accessible from TEXTURE_UNIT_DEPTH_READ)
    void depthbufferCopy();

    // sets required GL logicOp state for masks
    void prepareDrawMask();
    // sets required GL logicOp state for front images
    void prepareDrawImage();
    // unsets GL logicOp state for ordinary drawing
    void leaveMaskContext();

    // fill a vertex array buffer with a vertex attribute array. prepares all state needed for call to glDrawArrays
    void fillVertexBuffer(const Vertex_t* vertex_attribs, int count);
    // deallocate render queues unused since previous call to cleanupDrawQueues
    void cleanupDrawQueues();
    // clear render queues without drawing or deallocating
    void clearDrawQueues();

    // executes and clears all vertex lists in the unordered draw queue
    void flushUnorderedDrawQueue();
    // sets BUFFER_INT_PASS_1 to contain a distance field to the nearest edge in the game depth buffer
    void calculateDistanceField();
    // sorts and coalesces lights in the light queue
    void coalesceLights();
    // calculates the lighting state using depth buffer information
    void calculateLighting();
    // prepares for the next pass during multipass drawing
    void prepareMultipassState(int pass, int num_pass);
    // executes and optionally clears all vertex lists in the ordered draw queue
    void executeOrderedDrawQueue(bool clear);
    // Draws and clears all render queues. Called prior to changing GL context.
    void flushDrawQueues();


    /************************************************************************\
    || Front-end render helper functions, defined at render_gl_frontend.cpp ||
    \************************************************************************/

    // Selects efficient ordered vertex list for given context and depth pair. Batches across subsequent draws and masks.
    VertexList& getOrderedDrawVertexList(DrawContext_t context, int depth);

    // Adds vertices to a VertexList
    void addVertices(VertexList& list, const RectI& loc, const RectF& texcoord, GLshort depth, const Vertex_t::Tint& tint);
    void addVertices(VertexList& list, const QuadI& loc, const RectF& texcoord, GLshort depth, const Vertex_t::Tint& tint);

    // Checks for and adds lights for a draw call to the light buffer
    void addLights(const GLPictureLightInfo& light_info, const QuadI& loc, const RectF& texcoord, GLshort depth);

    // simple helper function to make a triangle strip for a single-quad draw
    std::array<Vertex_t, 4> genTriangleStrip(const RectI& loc, const RectF& texcoord, GLshort depth, const Vertex_t::Tint& tint);

protected:
    // Compiles user fragment shader and assembles program in target.
    void compileShaders(StdPicture &target) override;


    /**************************************************************\
    || Public render functions, defined at render_gl_frontend.cpp ||
    \**************************************************************/

public:
    RenderGL();
    ~RenderGL() override;


    unsigned int SDL_InitFlags() override;

    bool isWorking() override;

    bool hasFrameBuffer() override;

    bool initRender(SDL_Window *window) override;

    /*!
     * \brief Close the renderer
     */
    void close() override;

    /*!
     * \brief Call the repaint
     */
    void repaint() override;

    void applyViewport();

    /*!
     * \brief Update viewport (mainly after screen resize)
     */
    void updateViewport() override;

    /*!
     * \brief Reset viewport into default state
     */
    void resetViewport() override;

    /*!
     * \brief Set the viewport area
     * \param x X position
     * \param y Y position
     * \param w Viewport Width
     * \param h Viewport Height
     */
    void setViewport(int x, int y, int w, int h) override;

    /*!
     * \brief Set the render offset
     * \param x X offset
     * \param y Y offset
     *
     * All drawing objects will be drawn with a small offset
     */
    void offsetViewport(int x, int y) override; // for screen-shaking

    /*!
     * \brief Set temporary ignore of render offset
     * \param en Enable viewport offset ignore
     *
     * Use this to draw certain objects with ignorign of the GFX offset
     */
    void offsetViewportIgnore(bool en) override;

    /*!
     * \brief Make any subsequent draws invisible to any previous draws (reflections)
     *
     * Only has an effect for OpenGL renderer and other batched renderers.
     *
     * Note: may result in subsequent transparent draw being inaccurately drawn above previous transparent draw.
     */
    inline void splitFrame() override
    {
        flushDrawQueues();
    }

    void mapToScreen(int x, int y, int *dx, int *dy) override;

    void mapFromScreen(int x, int y, int *dx, int *dy) override;

    /*!
     * \brief Get the current size of the window in render pixels
     * \param w Width
     * \param h Height
     */
    void getRenderSize(int *w, int *h) override;

    /*!
     * \brief Set render target into the virtual in-game screen (use to render in-game world)
     */
    void setTargetTexture() override;

    /*!
     * \brief Set render target into the real window or screen (use to render on-screen buttons and other meta-info)
     */
    void setTargetScreen() override;

    /*!
     * \brief Sets draw plane for subsequent draws.
     *
     * \param plane Which draw plane should be used.
     */
    void setDrawPlane(uint8_t plane) override;

    void loadTextureInternal(StdPicture &target,
                     uint32_t width,
                     uint32_t height,
                     uint8_t *RGBApixels,
                     uint32_t pitch,
                     bool is_mask,
                     uint32_t least_width,
                     uint32_t least_height);

    void loadTextureInternal(StdPicture &target,
                     uint32_t width,
                     uint32_t height,
                     uint8_t *RGBApixels,
                     uint32_t pitch,
                     uint32_t mask_width,
                     uint32_t mask_height) override;

    void loadTextureMask(StdPicture &target,
                     uint32_t width,
                     uint32_t height,
                     uint8_t *RGBApixels,
                     uint32_t pitch,
                     uint32_t image_width,
                     uint32_t image_height) override;

    bool textureMaskSupported() override;

    bool userShadersSupported() override;

    bool depthTestSupported() override;

    void unloadTexture(StdPicture &tx) override;
    void unloadGifTextures() override;
    void clearAllTextures() override;

    void clearBuffer() override;

#ifdef THEXTECH_BUILD_GL_MODERN
    int registerUniform(StdPicture &target, const char* name) override;
    void assignUniform(StdPicture &target, int index, const UniformValue_t& value) override;
    void spawnParticle(StdPicture &target, double worldX, double worldY, ParticleVertexAttrs_t attrs) override;

    void addLight(const GLLight &light) override;
    void setupLighting(const GLLightSystem &system) override;
    void renderLighting() override;
#endif

    // Draw primitives

    void renderRect(int x, int y, int w, int h,
                    XTColor color = XTColor(),
                    bool filled = true) override;

    void renderRectBR(int _left, int _top, int _right,
                      int _bottom, XTColor color) override;

    void renderCircle(int cx, int cy,
                      int radius,
                      XTColor color = XTColor(),
                      bool filled = true) override;

    void renderCircleHole(int cx, int cy,
                          int radius,
                          XTColor color = XTColor()) override;




    // Draw texture

    void renderTextureScaleEx(double xDst, double yDst, double wDst, double hDst,
                              StdPicture &tx,
                              int xSrc, int ySrc,
                              int wSrc, int hSrc,
                              double rotateAngle =.0, FPoint_t *center = nullptr, unsigned int flip = X_FLIP_NONE,
                              XTColor color = XTColor()) override;

    void renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                            StdPicture &tx,
                            XTColor color = XTColor()) override;

    void renderTexture(double xDst, double yDst, double wDst, double hDst,
                       StdPicture &tx,
                       int xSrc, int ySrc,
                       XTColor color = XTColor()) override;

    void renderTextureFL(double xDst, double yDst, double wDst, double hDst,
                         StdPicture &tx,
                         int xSrc, int ySrc,
                         double rotateAngle =.0, FPoint_t *center = nullptr, unsigned int flip = X_FLIP_NONE,
                         XTColor color = XTColor()) override;

    void renderTexture(float xDst, float yDst, StdPicture &tx,
                       XTColor color = XTColor()) override;

    void renderParticleSystem(StdPicture &tx,
                              double camX,
                              double camY) override;



    // Retrieve raw pixel data

    void getScreenPixels(int x, int y, int w, int h, unsigned char *pixels) override;

    void getScreenPixelsRGBA(int x, int y, int w, int h, unsigned char *pixels) override;

    int  getPixelDataSize(const StdPicture &tx) override;

    void getPixelData(const StdPicture &tx, unsigned char *pixelData) override;

};


#endif // RENDERGL_SUPPORTED

#endif // RenderGL_T_H
