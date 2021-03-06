#ifndef API_GPU_PARTICLE_H
#define API_GPU_PARTICLE_H

/* -------------------------------------------------------------------------- */

#include <algorithm>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include "opengl.h"
#include "api/random_buffer.h"
#include "api/vector_field.h"

class AppendConsumeBuffer;

/* -------------------------------------------------------------------------- */

/**
 * @brief The GPUParticle class
 *
 * @note As is, the system forces the number of simulated particles to be factor
 * of kThreadsGroupWidth (to avoid condition checking at boundaries).
 * Condition checking is just at emission stage.
 */
class GPUParticle
{
public:
  static float constexpr kDefaultSimulationVolumeSize = 256.0f;

  enum EmitterType {
    EMITTER_POINT,
    EMITTER_DISK,
    EMITTER_SPHERE,
    EMITTER_BALL,
    kNumEmitterType
  };

  enum SimulationVolume {
    VOLUME_SPHERE,
    VOLUME_BOX,
    VOLUME_NONE,
    kNumSimulationVolume
  };

  struct SimulationParameters_t {
    float time_step_factor = 1.0f;
    float min_age = 50.0f;
    float max_age = 100.0f;
    EmitterType emitter_type = EmitterType::EMITTER_SPHERE;
    float emitter_position[3]  = { 0.0f, 0.0f, 0.0f };
    float emitter_direction[3] = { 0.0f, 1.0f, 0.0f };
    float emitter_radius = 32.0f;
    SimulationVolume bounding_volume = SimulationVolume::VOLUME_SPHERE;
    float bounding_volume_size = kDefaultSimulationVolumeSize;

    float scattering_factor = 1.0f;
    float vectorfield_factor = 1.0f;
    float curlnoise_factor = 16.0f;
    float curlnoise_scale = 128.0f;
    float velocity_factor = 8.0f;

    bool enable_scattering = false;
    bool enable_vectorfield = false;
    bool enable_curlnoise = true;
    bool enable_velocity_control = true;
  };

  enum RenderMode {
    RENDERMODE_STRETCHED,
    RENDERMODE_POINTSPRITE,
    kNumRenderMode
  };

  enum ColorMode {
    COLORMODE_DEFAULT,
    COLORMODE_GRADIENT,
    kNumColorMode
  };

  struct RenderingParameters_t {
    RenderMode rendermode = RENDERMODE_STRETCHED;
    float stretched_factor = 10.0f;
    ColorMode colormode = COLORMODE_DEFAULT;
    float birth_gradient[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    float death_gradient[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    float min_size = 0.75f;
    float max_size = 25.0f;
    float fading_factor = 0.35f;
  };

  GPUParticle() :
    num_alive_particles_(0u),
    pbuffer_(nullptr),
    gl_indirect_buffer_id_(0u),
    gl_dp_buffer_id_(0u),
    gl_sort_indices_buffer_id_(0u),
    vao_(0u),
    query_time_(0u),
    simulated_(false),
    enable_sorting_(false),
    enable_vectorfield_(true)
  {}

  void init();
  void deinit();

  void update(float const dt, glm::mat4x4 const& view);
  void render(glm::mat4x4 const& view, glm::mat4x4 const& viewProj);

  inline SimulationParameters_t& simulation_parameters() {
   return simulation_params_;
  }

  inline RenderingParameters_t& rendering_parameters() {
   return rendering_params_;
  }

  inline const glm::uvec3& vectorfield_dimensions() const {
    return vectorfield_.dimensions();
  }

  inline void enable_sorting(bool status) { enable_sorting_ = status; }
  inline void enable_vectorfield(bool status) { enable_vectorfield_ = status; }

private:
  // [STATIC]
  static unsigned int const kThreadsGroupWidth;

  // [USER DEFINED]
  static unsigned int const kMaxParticleCount = (1u << 18u);
  static unsigned int const kBatchEmitCount   = std::max(256u, (kMaxParticleCount >> 4u));

  static
  unsigned int GetThreadsGroupCount(unsigned int const nthreads) {
      return (nthreads + kThreadsGroupWidth-1u) / kThreadsGroupWidth;
  }

  static
  unsigned int FloorParticleCount(unsigned int const nparticles) {
    return kThreadsGroupWidth * (nparticles / kThreadsGroupWidth);
  }

  void _setup_render();

  void _emission(unsigned int const count);
  void _simulation(float const time_step);
  void _postprocess();
  void _sorting(glm::mat4x4 const& view);

  SimulationParameters_t simulation_params_;
  RenderingParameters_t rendering_params_;

  unsigned int num_alive_particles_;              //< number of particle written and rendered on last frame.
  AppendConsumeBuffer *pbuffer_;                  //< Append / Consume buffer for particles.
  RandomBuffer randbuffer_;                       //< StorageBuffer to hold random values.
  VectorField vectorfield_;                       //< Vector field handler.

  struct {
    GLuint emission;
    GLuint update_args;
    GLuint simulation;
    GLuint fill_indices;
    GLuint calculate_dp;
    GLuint sort_step;
    GLuint sort_final;
    GLuint render_point_sprite;
    GLuint render_stretched_sprite;
  } pgm_;                                         //< Pipeline's shaders.

  struct {
    struct {
      GLint emitCount;
      GLint emitterType;
      GLint emitterPosition;
      GLint emitterDirection;
      GLint emitterRadius;
      GLint particleMinAge;
      GLint particleMaxAge;
    } emission;
    struct {
      GLint timeStep;
      GLint vectorFieldSampler;
      GLint bboxSize;
      GLint boundingVolume;

      GLint scatteringFactor;
      GLint vectorFieldFactor;
      GLint curlNoiseFactor;
      GLint curlNoiseScale;
      GLint velocityFactor;
      GLint enableScattering;
      GLint enableVectorField;
      GLint enableCurlNoise;
      GLint enableVelocityControl;
    } simulation;
    struct {
      GLint view;
    } calculate_dp;
    struct {
      GLint blockWidth;
      GLint maxBlockWidth;
    } sort_step;
    struct {
      GLint mvp;
      GLint minParticleSize;
      GLint maxParticleSize;
      GLint colorMode;
      GLint birthGradient;
      GLint deathGradient;
      GLint fadeCoefficient;
    } render_point_sprite;
    struct {
      GLint view;
      GLint mvp;
      GLint colorMode;
      GLint birthGradient;
      GLint deathGradient;
      GLint spriteStretchFactor;
      GLint fadeCoefficient;
    } render_stretched_sprite;
  } ulocation_;                                   //< Programs uniform location.

  ///
  /// @todo instead, use one array in GPUParticle that holds
  /// all storage ids and share them between subclasses.
  ///
  GLuint gl_indirect_buffer_id_;                  //< Indirect Dispatch / Draw buffer.
  GLuint gl_dp_buffer_id_;                        //< DotProduct buffer.
  GLuint gl_sort_indices_buffer_id_;              //< indices buffer (for sorting).

  GLuint vao_;                                    //< VAO for rendering.
  GLuint query_time_;                             //< QueryObject for benchmarking.

  bool simulated_;                                //< True if particles has been simulated.
  bool enable_sorting_;                           //< True if back-to-front sort is enabled.
  bool enable_vectorfield_;                       //< True if the vector field is used.
};

/* -------------------------------------------------------------------------- */

#endif // API_GPU_PARTICLE_H
