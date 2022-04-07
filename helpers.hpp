#pragma once

#include <boost/pfr/core.hpp>
#include <coroutine>
#include <cstdlib>
#include <variant>
#include <vector>

#include <string_view>

// Quick helper macro
#define halp_flag(flag) enum { flag }
#define halp_meta(name, value) static consteval auto name() { return value; }

// std::generator is not implemented yet, so polyfill it more or less
namespace gpu
{
template <typename Out, typename In>
class generator
{
public:
  // Types used by the coroutine
  struct promise_type
  {
    Out current_command;
    In feedback_value;

    struct awaiter : std::suspend_always
    {
      friend promise_type;
      constexpr In await_resume() const { return p.feedback_value; }

      promise_type& p;
    };

    generator get_return_object()
    {
      return generator{handle::from_promise(*this)};
    }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    awaiter yield_value(Out value) noexcept
    {
      current_command = value;
      return awaiter{{}, *this};
    }

    void return_void() noexcept { }

    // Disallow co_await in generator coroutines.
    void await_transform() = delete;

    [[noreturn]] static void unhandled_exception() { std::abort(); }
  };

  using handle = std::coroutine_handle<promise_type>;

  // To enable begin / end
  class iterator
  {
  public:
    explicit iterator(const handle& coroutine) noexcept
        : m_coroutine{coroutine}
    {
    }

    void operator++() noexcept { m_coroutine.resume(); }

    auto& operator*() const noexcept { return m_coroutine.promise(); }

    bool operator==(std::default_sentinel_t) const noexcept
    {
      return !m_coroutine || m_coroutine.done();
    }

  private:
    handle m_coroutine;
  };

  // Constructors
  explicit generator(handle coroutine)
      : m_coroutine{std::move(coroutine)}
  {
  }

  generator() noexcept = default;
  generator(const generator&) = delete;
  generator& operator=(const generator&) = delete;

  generator(generator&& other) noexcept
      : m_coroutine{other.m_coroutine}
  {
    other.m_coroutine = {};
  }

  generator& operator=(generator&& other) noexcept
  {
    if (this != &other)
    {
      if (m_coroutine)
      {
        m_coroutine.destroy();
      }

      m_coroutine = other.m_coroutine;
      other.m_coroutine = {};
    }
    return *this;
  }

  ~generator()
  {
    if (m_coroutine)
    {
      m_coroutine.destroy();
    }
  }

  // Range-based for loop support.
  iterator begin() noexcept
  {
    if (m_coroutine)
    {
      m_coroutine.resume();
    }

    return iterator{m_coroutine};
  }

  std::default_sentinel_t end() const noexcept { return {}; }

private:
  handle m_coroutine;
};
}



namespace gpu
{
// basic enum and type definition
enum class layouts
{
  std140
};
enum class samplers
{
  sampler1D,
  sampler2D,
  sampler3D,
  samplerCube,
  sampler2DRect
};
enum class buffer_type
{
  immutable_buffer,
  static_buffer,
  dynamic_buffer
};
enum class buffer_usage
{
  vertex,
  index,
  uniform,
  storage
};
enum class binding_stage
{
  vertex,
  fragment
};

enum class default_attributes
{
  position,
  normal,
  tangent,
  texcoord,
  color
};
/*
enum class default_uniforms
{
  // Render-level default uniforms
  clip_space_matrix // float[16]
  , texcoord_adjust // float[2]
  , render_size // float[2]

  // Process-level default uniforms
  , time // float
  , time_delta // float
  , progress // float
  , pass_index // int
  , frame_index // int
  , date // float[4]
  , mouse // float[4]
  , channel_time // float[4]
  , sample_rate // float
};




*/




// Define our commands
struct dynamic_vertex_allocation
{
  enum { allocation, dynamic, vertex };
  int binding;
  int size;
};
struct dynamic_vertex_upload
{
  enum { upload, dynamic, vertex };
  void* handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_index_allocation
{
  enum { allocation, dynamic, index };
  int binding;
  int size;
};
struct dynamic_index_upload
{
  enum { upload, dynamic, index };
  void* handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_ubo_allocation
{
  enum { allocation, dynamic, ubo };
  int binding;
  int size;
};
struct dynamic_ubo_upload
{
  enum { upload, dynamic, ubo };
  void* handle;
  int offset;
  int size;
  void* data;
};

struct sampler_allocation
{
  enum { allocation, sampler };
};
struct texture_allocation
{
  enum { allocation, texture };
  int binding;
  int width;
  int height;
};

struct texture_upload
{
  enum { upload, texture };
  void* handle;
  int offset;
  int size;
  void* data;
};






// Define what the update() can do
using action = std::variant<
dynamic_vertex_allocation, dynamic_vertex_upload,
dynamic_index_allocation, dynamic_index_upload,
dynamic_ubo_allocation, dynamic_ubo_upload,
sampler_allocation,
texture_allocation, texture_upload
>;
using handle = void*;
using co_update = gpu::generator<action, handle>;








// Some utilities

template <typename T>
consteval int binding()
{
  return T::binding();
}
template <typename T>
consteval int std140_size()
{
  int sz = 0;
  constexpr int field_count = boost::pfr::tuple_size_v<T>;
  auto func = [&](auto field)
  {
    switch (sizeof(field.value))
    {
      case 4:
        sz += 4;
        break;
      case 8:
        if (sz % 8 != 0)
          sz += 4;
        sz += 8;
        break;
      case 12:
        while (sz % 16 != 0)
          sz += 4;
        sz += 12;
        break;
      case 16:
        while (sz % 16 != 0)
          sz += 4;
        sz += 16;
        break;
    }
  };

  if constexpr (field_count > 0)
  {
    [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>)
    {
      constexpr T t{};
      (func(boost::pfr::get<Index, T>(t)), ...);
    }
    (std::make_index_sequence<field_count>{});
  }
  return sz;
}
}
