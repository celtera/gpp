#pragma once
#include "helpers.hpp"
#include <halp/static_string.hpp>
#include <halp/controls.hpp>
#include <avnd/common/member_reflection.hpp>

namespace examples
{
// Define the layout of our pipeline in C++ simply through the structure of a struct
struct layout
{
  halp_flags(graphics);
  struct vertex_input {
    gpp_attribute(0, v_position, float[3], position) pos;
    gpp_attribute(1, v_texcoord, float[2], texcoord) tex;
  } vertex_input;

  struct vertex_output {
    gpp_attribute(0, texcoord,float[2], texcoord) tex;
    gpu::vertex_position_out position;
  } vertex_output;

  struct fragment_input {
    gpp_attribute(0, texcoord, float[2], texcoord) tex;
  } fragment_input;

  struct fragment_output {
    gpp_attribute(0, fragColor, float[4], color) col;
  } fragment_output;

  // Define the ubos, samplers, etc.
  struct bindings
  {
    struct custom_ubo {
      halp_meta(name, "custom");
      halp_flags(std140, ubo);

      static constexpr int binding() { return 0; }

      gpu::uniform<"foo", float[2]> pad;
      gpu::uniform<"bar", float> slider;
    } ubo;

    gpu::sampler<"tex", 1> texture_input;
  } bindings;
};

struct GpuFilterExample
{
  halp_meta(name, "My GPU pipeline");
  halp_meta(uuid, "6addd716-5c9f-4b2d-a9ef-6b7a3dcf8697");

  using layout = examples::layout;
  using bindings = decltype(layout::bindings);
  using uniforms = decltype(bindings::ubo);

  static constexpr auto lay = layout{};

  struct {
    // If samplers & buffers are referenced here the GPU side of things
    // will be automatically allocated as they are expect to come from "outside"
    gpu::uniform_control_port<halp::hslider_f32<"Alpha">, &uniforms::slider> bright;

    // It's also possible to have purely CPU-side controls to manage e.g. texture sizes, etc...
    halp::hslider_f32<"Other"> other;

  } inputs;

  struct {
    gpu::color_attachment_port<"Main out", &layout::fragment_output::col> fragColor;
  } outputs;

  std::string_view vertex()
  {
    return R"_(
void main()
{
  texcoord = v_texcoord;
  gl_Position = vec4(v_position.x / 3., v_position.y / 3, 0.0, 1.);
}
)_";
  }

  std::string_view fragment()
  {
    return R"_(
void main()
{
  fragColor = vec4(texture(tex, texcoord.xy * foo).rgb, bar) ;
}
)_";
  }

  std::vector<uint8_t> tex;

  gpu::texture_handle tex_handle{};

  gpu::co_update update()
  {
    // In this example we test the automatic UBO filling with the inputs declared above.

    // Here the surrounding environment makes sure that the UBO already has a handle
    auto ubo = co_yield gpu::get_ubo_handle{
      .binding = lay.bindings.ubo.binding()
    };

    // Upload some data into it, using an input (non-uniform) of our node
    using namespace std;
    float xy[2] = {cos(inputs.other), sin(inputs.other)};

    co_yield gpu::dynamic_ubo_upload{
        .handle = ubo,
        .offset = 0,
        .size = sizeof(xy),
        .data = &xy
    };

    // The sampler is not used by the inputs block, so we have to allocate it ourselves
    int sz = 16*16*4;
    if(!tex_handle)
    {
      this->tex_handle = co_yield gpu::texture_allocation{
          .binding = lay.bindings.texture_input.binding()
        , .width = 16
        , .height = 16
      };
    }

    // And upload some data
    tex.resize(sz);
    for(int i = 0; i < sz; i++)
      tex[i] = rand();

    co_yield gpu::texture_upload{
        .handle = tex_handle
      , .offset = 0
      , .size = sz
      , .data = tex.data()
    };
  }


  gpu::co_release release()
  {
    co_yield gpu::texture_release{.handle = tex_handle};
  }
};

}
