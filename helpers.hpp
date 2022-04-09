#pragma once
#include <halp/static_string.hpp>
#include <boost/pfr/core.hpp>
#include <coroutine>
#include <cstdlib>
#include <variant>
#include <vector>
#include <string_view>

// Quick helper macro
#define halp_flag(flag) enum { flag }
#define halp_flags(...) enum { __VA_ARGS__ }
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

    template<typename Ret>
    struct awaiter : std::suspend_always
    {
      friend promise_type;
      constexpr auto await_resume() const {
          return std::get<Ret>(p.feedback_value);
      }

      promise_type& p;
    };

    generator get_return_object()
    {
      return generator{handle::from_promise(*this)};
    }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    template<typename T>
    auto yield_value(T value) noexcept
    {
      current_command = value;
      using ret = typename T::return_type;
      if constexpr(std::is_same_v<ret, void>)
        return std::suspend_always{};
      else
        return awaiter<ret>{{}, *this};
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

template <typename Out>
class generator<Out, void>
{
public:
  // Types used by the coroutine
  struct promise_type
  {
    Out current_command;
    generator get_return_object()
    {
      return generator{handle::from_promise(*this)};
    }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    template<typename T>
    std::suspend_always yield_value(T&& value) noexcept
    {
      current_command = std::move(value);
      return std::suspend_always{};
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





struct suspend
{
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<>) const noexcept { }
  void await_resume() const noexcept { }
};
//static const constexpr auto qSuspend = QSuspend{};

template<typename Promise>
class task
{
public:
  using promise_type = Promise;

  explicit task(std::coroutine_handle<Promise> handle) noexcept
      : m_handle{std::move(handle)}
  {
  }

  task(task&& other) noexcept : m_handle{std::exchange(other.m_handle, {})}
  {
  }

  task& operator=(task&& other) noexcept
  {
    m_handle = std::exchange(other.m_handle, {});
    return *this;
  }

  ~task()
  {
    if (m_handle)
      m_handle.destroy();
  }

  task(const task&) = delete;
  task& operator=(const task&) = delete;

  bool resume()
  {
    if (!m_handle.done())
      m_handle.resume();
    return !m_handle.done();
  }

private:
  std::coroutine_handle<Promise> m_handle;
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



struct buffer_handle_t;
using buffer_handle = buffer_handle_t*;
struct texture_handle_t;
using texture_handle = texture_handle_t*;
struct sampler_handle_t;
using sampler_handle = sampler_handle_t*;

// Define our commands
struct static_allocation
{
  enum { allocation, static_, storage };
  using return_type = buffer_handle;
  int binding;
  int size;
};

struct static_upload
{
  enum { upload, static_ };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_vertex_allocation
{
  enum { allocation, dynamic, vertex };
  using return_type = buffer_handle;
  int binding;
  int size;
};
struct dynamic_vertex_upload
{
  enum { upload, dynamic, vertex };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_index_allocation
{
  enum { allocation, dynamic, index };
  using return_type = buffer_handle;
  int binding;
  int size;
};
struct dynamic_index_upload
{
  enum { upload, dynamic, index };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_ubo_allocation
{
  enum { allocation, dynamic, ubo };
  using return_type = buffer_handle;
  int binding;
  int size;
};
struct dynamic_ubo_upload
{
  enum { upload, dynamic, ubo };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct sampler_allocation
{
  enum { allocation, sampler };
  using return_type = sampler_handle;
};
struct texture_allocation
{
  enum { allocation, texture };
  using return_type = texture_handle;
  int binding;
  int width;
  int height;
};

struct texture_upload
{
  enum { upload, texture };
  using return_type = void;
  texture_handle handle;
  int offset;
  int size;
  void* data;
};



struct get_ubo_handle
{
  enum { getter, ubo };
  using return_type = buffer_handle;
  int binding;
};
struct get_texture_handle
{
  enum { getter, texture };
  using return_type = texture_handle;
  int binding;
};




struct buffer_release
{
  enum { deallocation, vertex, index };
  using return_type = void;
  buffer_handle handle;
};

struct ubo_release
{
  enum { deallocation, ubo };
  using return_type = void;
  buffer_handle handle;
};

struct sampler_release
{
  enum { deallocation, texture };
  using return_type = void;
  texture_handle handle;
};

struct texture_release
{
  enum { deallocation, texture };
  using return_type = void;
  texture_handle handle;
};


struct begin_compute_pass
{
  enum { compute, begin };
  using return_type = void;
};

struct end_compute_pass
{
  enum { compute, end };
  using return_type = void;
};

struct compute_dispatch
{
  enum { compute, dispatch };
  using return_type = void;
  int x, y, z;
};


struct buffer_view { const char* data; std::size_t size; };
struct texture_view { const char* data; std::size_t size; };

struct buffer_readback_handle_t;
struct texture_readback_handle_t;
using buffer_readback_handle = buffer_readback_handle_t*;
using texture_readback_handle = texture_readback_handle_t*;


struct buffer_awaiter {
    enum { readback, await, buffer };
    using return_type = buffer_view;
    buffer_readback_handle handle;
};
struct texture_awaiter {
    enum { readback, await, texture };
    using return_type = texture_view;
    texture_readback_handle handle;
};
struct readback_buffer
{
  enum { readback, request, buffer };
  using return_type = buffer_awaiter;
  buffer_handle handle;
  int offset;
  int size;
};
struct readback_texture
{
  enum { readback, request, texture };
  using return_type = texture_awaiter;
  texture_handle handle;
};



// Define what the update() can do
using update_action = std::variant<
  static_allocation, static_upload,
  dynamic_vertex_allocation, dynamic_vertex_upload, buffer_release,
  dynamic_index_allocation, dynamic_index_upload,
  dynamic_ubo_allocation, dynamic_ubo_upload, ubo_release,
  sampler_allocation, sampler_release,
  texture_allocation, texture_upload, texture_release,
  get_ubo_handle
>;
using update_handle = std::variant<std::monostate, buffer_handle, texture_handle, sampler_handle>;
using co_update = gpu::generator<update_action, update_handle>;


// Define what the release() can do
using release_action = std::variant<
    buffer_release
  , ubo_release
  , sampler_release
  , texture_release
>;
using co_release = gpu::generator<release_action, void>;


// Define what the dispatch(), for compute, can do

using dispatch_action = std::variant<
  begin_compute_pass, end_compute_pass
, compute_dispatch
, readback_buffer, readback_texture
, buffer_awaiter, texture_awaiter
>;
using dispatch_handle = std::variant<
  std::monostate
, buffer_awaiter, texture_awaiter
, buffer_view, texture_view
>;
using co_dispatch = gpu::generator<dispatch_action, dispatch_handle>;



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


/// Quick way to define the pipeline layout
#define gpp_attribute(loc, n, type, ...) \
    struct { \
      halp_meta(name, #n); \
      halp_flag(__VA_ARGS__); \
      static constexpr int location() { return loc; } \
      using data_type = type; \
      data_type data; \
    }

#define gpp_compute(loc, buffer_n, n, type, ...) \
    struct { \
      halp_meta(name, #n); \
      halp_meta(buffer_name, #buffer_n); \
      halp_flag(__VA_ARGS__); \
      static constexpr int location() { return loc; } \
      using data_type = type; \
      data_type data; \
    }

// And some common types predefined for the most common use cases
namespace gpu
{
  struct vertex_position_out {
    halp_meta(name, "gl_Position");
    halp_flag(per_vertex);
    float data[4];
  };

  // To be used in the layout bindings
  template<halp::static_string lit, int bnd>
  struct sampler {
    static constexpr std::string_view name() { return lit.value; }
    halp_flag(sampler2D);
    static constexpr int binding() { return bnd; }
  };

  template<halp::static_string lit, int bnd>
  struct image {
      static constexpr std::string_view name() { return lit.value; }
      halp_flags(image2D, load, store);
      static constexpr int binding() { return bnd; }
  };

  template<halp::static_string lit, int bnd>
  struct image_input {
      static constexpr std::string_view name() { return lit.value; }
      halp_flags(image2D, readonly);
      static constexpr int binding() { return bnd; }
  };

  template<halp::static_string lit, int bnd>
  struct image_output {
      static constexpr std::string_view name() { return lit.value; }
      halp_flags(image2D, writeonly);
      static constexpr int binding() { return bnd; }
  };

  template<halp::static_string lit, typename T>
  struct uniform {
    static constexpr std::string_view name() { return lit.value; }
    T value;
  };


  // Those are to be used as the object ports
  template<halp::static_string lit, auto T>
  struct texture_input_port {
    static constexpr std::string_view name() { return lit.value; }
    static constexpr auto sampler() { return T; }
  };

  template<halp::static_string lit, auto T>
  struct image_input_port {
      static constexpr std::string_view name() { return lit.value; }
      static constexpr auto image() { return T; }
  };

  template<halp::static_string lit, auto T>
  struct color_attachment_port {
    static constexpr std::string_view name() { return lit.value; }
    static constexpr auto attachment() { return T; }
  };

  template<typename Widget, auto T>
  struct uniform_control_port : Widget {
    static constexpr auto uniform() { return T; }
  };
}
