#pragma once
#include "helpers.hpp"


namespace examples
{

// Define the layout of our pipeline in C++ simply thorugh the structure of a struct

struct layout
{
  // Define the vertex inputs
  struct
  {
    // Of course in practice we'd use some custom types or macros here, 
    // this is just to show that there's no magic or special type involved
    struct {
      static constexpr int location() { return 0; }
      halp_meta(name, "v_position");
      float data[3];
    } vertex;

    struct {
      static constexpr int location() { return 1; }
      halp_meta(name, "v_normal");
      float data[2];
    } normal;

    struct {
      static constexpr int location() { return 2; }
      halp_meta(name, "v_texcoord");
      float data[2];
    } texcoord;
  } vertex_input;

  // Define the vertex outputs
  struct { 
    struct {
      static constexpr int location() { return 0; }
      halp_meta(name, "texcoord");
      float data[2];
    } texcoord;
  } vertex_output;

  // Define the fragment inputs
  struct { 
    struct {
      static constexpr int location() { return 0; }
      halp_meta(name, "texcoord");
      float data[2];
    } texcoord;
  } fragment_input;

  // Define the fragment outputs
  struct
  {
    struct {
      static constexpr int location() { return 0; }
      halp_meta(name, "fragColor");
      float data[4];
    } fragColor;
  } fragment_output;


  // Define the ubos, samplers, etc.
  struct
  {
    struct {
      halp_meta(name, "custom");
      enum layout { std140 };
      enum type { ubo };
      static constexpr int binding() { return 0; }
      struct
      {
        halp_meta(name, "Foo");
        enum widget { xy_pad };
        float value[2];
      } pad;

      struct
      {
        halp_meta(name, "Bar");
        enum widget { hslider };
        float value;
      } slider;
    } custom_ubo;

    struct
    {
      halp_meta(name, "tex");
      enum type { sampler2D };
      static constexpr int binding() { return 1; }
    } texture_input;
  } bindings;
};



struct GpuFilterExample
{
  halp_meta(name, "My GPU pipeline");

  std::string_view vertex()
  {
    return R"_(
void main()
{
}
)_";
  }
  std::string_view fragment()
  {
    return R"_(
void main()
{
  fragColor = texture(tex, texcoord) * Bar + Foo.x;
}
)_";
  }

  static constexpr examples::layout layout{};

  std::vector<float> buf;

  void* buf_handle{};

  gpu::update_coroutine update()
  {
    constexpr int ubo_size = gpu::std140_size<decltype(layout.bindings.custom_ubo)>();
    if (!buf_handle)
    {
      // Resize our local, CPU-side buffer
      buf.resize(ubo_size);

      // Request the creation of a GPU buffer
      this->buf_handle = co_yield gpu::buffer_allocation{
          .binding = layout.bindings.custom_ubo.binding()
        , .size = ubo_size};
    }

    // Upload some data into it
    co_yield gpu::buffer_upload{
        .handle = buf_handle,
        .offset = 0,
        .size = ubo_size,
        .data = buf.data()};
  }
};

}
