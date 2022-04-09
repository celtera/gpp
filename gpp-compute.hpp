#pragma once
#include "helpers.hpp"
#include <halp/static_string.hpp>
#include <halp/controls.hpp>
#include <avnd/common/member_reflection.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>

namespace examples
{

struct GpuComputeExample
{
  // halp_meta is a short hand for defining a static function:
  // #define halp_meta(name, val) static constexpr auto name() return { val; }
  halp_meta(name, "My GPU pipeline");
  halp_meta(uuid, "03bce361-a2ca-4959-95b4-6aac3b6c07b5");

  static constexpr int downscale = 16;

  // Define the layout of our pipeline in C++ simply through the structure of a struct
  struct layout
  {
    halp_meta(local_size_x, 16)
    halp_meta(local_size_y, 16)
    halp_meta(local_size_z, 1)
    halp_flags(compute);

    struct bindings
    {
      // Each binding is a struct member
      struct {
        halp_meta(name, "my_buf");
        halp_meta(binding, 0);
        halp_flags(std140, buffer, load, store);

        using color = float[4];
        gpu::uniform<"result", color*> values;
      } my_buf;

      // Define the members of our ubos
      struct custom_ubo {
        halp_meta(name, "custom");
        halp_meta(binding, 1);
        halp_flags(std140, ubo);

        gpu::uniform<"width", int> width;
        gpu::uniform<"height", int> height;
      } ubo;

      struct  {
        halp_meta(name, "img")
        halp_meta(format, "rgba32f")
        halp_meta(binding, 2);
        halp_flags(image2D, readonly);
      } image;
    } bindings;
  };

  using bindings = decltype(layout::bindings);
  using uniforms = decltype(bindings::ubo);

  // Definition of our ports which will get parsed by the
  // software that instantiate this class
  struct {
      // Here we use some helper types in the usual fashion
      gpu::image_input_port<"Image", &bindings::image> tex;

      gpu::uniform_control_port<
        halp::hslider_i32<"Width", halp::range{1, 1000, 100}>
        , &uniforms::width
      > width;

      gpu::uniform_control_port<
        halp::hslider_i32<"Height", halp::range{1, 1000, 100}>
        , &uniforms::height
      > height;
  } inputs;

  // The output port on which we write the average color
  struct {
    struct {
      halp_meta(name, "color")
      float value[4];
    } color_out;
  } outputs;

  std::string_view compute()
  {
    return R"_(
void main()
{
  // Note: the algorithm is most likely wrong as I know FUCK ALL
  // about compute shaders ; fixes welcome ;p

  ivec2 call = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = vec4(0,0,0,0);

  for(int i = 0; i < gl_WorkGroupSize.x; i++)
  {
    for(int j = 0; j < gl_WorkGroupSize.y; j++)
    {
      uint x = call.x * gl_WorkGroupSize.x + i;
      uint y = call.y * gl_WorkGroupSize.y + j;

      if (x < width && y < height)
      {
        color += imageLoad(img, ivec2(x,y));
      }
    }
  }

  if(gl_LocalInvocationIndex < ((width * height) / gl_WorkGroupSize.x * gl_WorkGroupSize.y))
  {
    result[gl_GlobalInvocationID.y * gl_WorkGroupSize.x + gl_GlobalInvocationID.x] = color;
  }
}
)_";
  }

  // Allocate and update buffers
  gpu::co_update update()
  {
    // Deallocate if the size changed
    const int w = this->inputs.width / downscale;
    const int h = this->inputs.height / downscale;
    if(last_w != w || last_h != h)
    {
      if(this->buf) {
        co_yield gpu::buffer_release{.handle = buf};
        buf = nullptr;
      }
      last_w = w;
      last_h = h;
    }

    if(w > 0 && h > 0)
    {
      // No buffer: reallocate
      const int bytes = w * h * sizeof(float) * 4;
      if(!this->buf)
      {
        this->buf = co_yield gpu::static_allocation{
           .binding = lay.bindings.my_buf.binding()
         , .size = bytes
        };
      }
    }
  }

  // Relaease allocated data
  gpu::co_release release()
  {
    if(buf) {
      co_yield gpu::buffer_release{.handle = buf};
      buf = nullptr;
    }
  }

  // Do the GPU dispatch call
  gpu::co_dispatch dispatch()
  {
    if(!buf)
      co_return;

    const int w = this->inputs.width / downscale;
    const int h = this->inputs.height / downscale;
    const int bytes = w * h * sizeof(float) * 4;

    // Run a pass
    co_yield gpu::begin_compute_pass{};

    co_yield gpu::compute_dispatch{.x = 1, .y = 1, .z = 1};

    // Request an asynchronous readback
    gpu::buffer_awaiter readback = co_yield gpu::readback_buffer{
        .handle = buf
      , .offset = 0
      , .size = bytes
    };

    co_yield gpu::end_compute_pass{};

    // The readback can be fetched once the compute pass is done
    // (this needs to be improved in terms of asyncness)
    auto [data, size] = co_yield readback;

    using color = float[4];
    auto flt = reinterpret_cast<const color*>(data);

    // finish summing on the cpu
    auto& final = outputs.color_out.value;
    std::ranges::fill(final, 0.f);

    for(int i = 0; i < w * h; i++) {
      for(int j = 0; j < 4; j++)
        final[j] += flt[i][j];
    }

    double pixels_total = this->inputs.width * this->inputs.height;
    final[0] /= pixels_total;
    final[1] /= pixels_total;
    final[2] /= pixels_total;
    final[3] /= pixels_total;
  }

private:
  static constexpr auto lay = layout{};
  int last_w{}, last_h{};
  gpu::buffer_handle buf{};
  std::vector<float> zeros{};
};

}
