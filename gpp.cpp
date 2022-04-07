#include "gpp.hpp"

#include <iostream>
#include <string>
#include <fmt/format.h>

// the parsing code here does not depend on the actual implementation 
// of the graphics object, only that it follows a certain shape

struct handle_command
{
  // If the command type is only variant<buffer_allocation, buffer_upload> then 
  // the other "cases" won't even be generated
  template <typename C>
  void* operator()(C& command)
  {
    if constexpr (requires { C::static_buffer_allocation; })
    {
      std::cerr << "static buffer allocation requested\n";
      std::cerr << "  -> binding: " << command.binding << " ; sz: " << command.size << "\n";
      return new int{1};
    }
    else if constexpr (requires { C::dynamic_buffer_allocation; })
    {
      std::cerr << "dynamic buffer allocation requested\n";
      std::cerr << "  -> binding: " << command.binding << " ; sz: " << command.size << "\n";
      return new int{2};
    }
    else if constexpr (requires { C::immutable_buffer_allocation; })
    {
      std::cerr << "immutable buffer allocation requested\n";
      std::cerr << "  -> binding: " << command.binding << " ; sz: " << command.size << "\n";
      return new int{3};
    }
    else if constexpr (requires { C::texture_allocation; })
    {
      std::cerr << "texture allocation requested\n";
      return new int{4};
    }
    else if constexpr (requires { C::static_buffer_upload; })
    {
      std::cerr << "static buffer upload requested\n";
      std::cerr << "  -> handle: " << *(int*) command.handle << "\n";
      std::cerr << "  -> offset: " << command.offset << " ; sz: " << command.size << "\n";
      return nullptr;
    }
    else if constexpr (requires { C::dynamic_buffer_upload; })
    {
      std::cerr << "dynamic buffer upload requested\n";
      std::cerr << "  -> handle: " << *(int*) command.handle << "\n";
      std::cerr << "  -> offset: " << command.offset << " ; sz: " << command.size << "\n";
      return nullptr;
    }
    else if constexpr (requires { C::immutable_buffer_upload; })
    {
      std::cerr << "immutable buffer upload requested\n";
      std::cerr << "  -> handle: " << *(int*) command.handle << "\n";
      std::cerr << "  -> offset: " << command.offset << " ; sz: " << command.size << "\n";
      return nullptr;
    }
    else if constexpr (requires { C::texture_upload; })
    {
      std::cerr << "texture upload requested\n";
      std::cerr << "  -> handle: " << *(int*) command.handle << "\n";
      std::cerr << "  -> sz: " << command.size << "\n";
      return nullptr;
    }
  }
};

template <typename T>
void handle_update(T& object)
{
  for (auto& promise : object.update())
  {
    promise.feedback_value
        = std::visit(handle_command{}, promise.current_command);
  }
}


static constexpr std::string_view field_type(float) { return "float"; }
static constexpr std::string_view field_type(const float (&)[2]) { return "vec2"; }
static constexpr std::string_view field_type(const float (&)[3]) { return "vec3"; }
static constexpr std::string_view field_type(const float (&)[4]) { return "vec4"; }
static constexpr std::string_view field_type(int) { return "int"; }
static constexpr std::string_view field_type(const int (&)[2]) { return "ivec2"; }
static constexpr std::string_view field_type(const int (&)[3]) { return "ivec3"; }
static constexpr std::string_view field_type(const int (&)[4]) { return "ivec4"; }
struct write_input
{
  std::string& shader;
  
  template<typename T>
  void operator()(const T& field) 
  {
      shader += fmt::format(
          "layout(location = {}) in {} {};\n"
          , field.location()
          , field_type(field.data)
          , field.name());
  }
};

struct write_output
{
  std::string& shader;

  template<typename T>
  void operator()(const T& field) 
  {
      if constexpr(requires { field.location(); })
      {
      }
      if constexpr(requires { field.location(); })
      {
        shader += fmt::format(
            "layout(location = {}) out {} {};\n"
            , field.location()
            , field_type(field.data)
            , field.name());
      }
  }
};

struct write_binding
{
  std::string& shader;

  template<typename T>
  void operator()(const T& field) 
  {
      shader += fmt::format(
          "  {} {};\n"
          , field_type(field.value)
          , field.name());
  }
};

struct write_bindings
{
  std::string& shader;

  template<typename C>
  void operator()(const C& field) 
  {
    if constexpr (requires { C::sampler2D; }) {
      shader += fmt::format(
          "layout(binding = {}) uniform sampler2D {};\n\n"
          , field.binding()
          , field.name());
    }
    else if constexpr (requires { C::ubo; }) {
      shader += fmt::format(
          "layout({}, binding = {}) uniform {}\n{{\n"
          , "std140" // TODO
          , field.binding()
          , field.name());

      boost::pfr::for_each_field(field, write_binding{shader});

      shader += fmt::format("}};\n\n");
    } 
  }
};

int main() {
    examples::GpuFilterExample ex;

    std::string vstr = "#version 450\n\n";

    using layout = examples::GpuFilterExample::layout;
    static constexpr auto lay = layout{};
    boost::pfr::for_each_field(lay.vertex_input, write_input{vstr}); 
    boost::pfr::for_each_field(lay.vertex_output, write_output{vstr});
    vstr += "\n"; 
    boost::pfr::for_each_field(lay.bindings, write_bindings{vstr}); 
   
    std::cout << "\n --- Vertex --- \n\n" << vstr << ex.vertex() << std::endl;

    std::string fstr = "#version 450\n\n";

    boost::pfr::for_each_field(lay.fragment_input, write_input{fstr}); 
    boost::pfr::for_each_field(lay.fragment_output, write_output{fstr}); 
    fstr += "\n";
    boost::pfr::for_each_field(lay.bindings, write_bindings{fstr}); 
  
   std::cout << "\n --- Fragment --- \n\n" << fstr << ex.fragment() << std::endl;


   std::cout << "\n --- Fake commands --- \n" << std::endl;

   handle_update(ex);
 }