#pragma once

#include <concepts>
#include <filesystem>
#include <gev/res/repo.hpp>
#include <gev/res/virtual_enable_shared_from_this.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gev
{
  class serializer;

  class serializable : public virtual_enable_shared_from_this
  {
  public:
    template<typename T>
    static void write_vector(std::vector<T> const& data, std::ostream& out)
    {
      write_size(data.size(), out);
      out.write(reinterpret_cast<char const*>(data.data()), data.size() * sizeof(T));
    }

    template<typename T>
    static void read_vector(std::vector<T>& data, std::istream& in)
    {
      std::size_t size = 0;
      read_size(size, in);
      data.resize(size);
      in.read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(T));
    }

    static void read_size(std::size_t& size, std::istream& in)
    {
      read_typed(size, in);
    }

    static void write_size(std::size_t size, std::ostream& out)
    {
      write_typed(size, out);
    }

    static void read_string(std::string& str, std::istream& in)
    {
      std::size_t size = 0;
      read_size(size, in);
      str.resize(size);
      in.read(str.data(), size);
    }

    static void write_string(std::string_view str, std::ostream& out)
    {
      write_size(str.size(), out);
      out.write(str.data(), str.size());
    }

    template<typename T>
    static void read_typed(T& size, std::istream& in)
    {
      char s[sizeof(T)]{};
      in.read(s, std::size(s));
      size = std::bit_cast<T>(s);
    }

    template<typename T>
    static void write_typed(T size, std::ostream& out)
    {
      char s[sizeof(T)]{};
      std::memcpy(s, &size, sizeof(T));
      out.write(s, std::size(s));
    }

    virtual void serialize(serializer& base, std::ostream& out) = 0;
    virtual void deserialize(serializer& base, std::istream& in) = 0;
  };

  template<typename T>
  std::shared_ptr<serializable> create_object()
  {
    return std::make_shared<T>();
  }

  template<typename T>
  std::shared_ptr<T> as(std::shared_ptr<serializable> s)
    requires std::derived_from<T, serializable>
  {
    return std::static_pointer_cast<T>(s);
  }

  enum class serialize_reference_type : std::uint8_t
  {
    direct,
    reference,
    null
  };

  class serializer
  {
  public:
    void init()
    {
      load_resources();
    }

    template<typename Fn, typename... Args>
    std::shared_ptr<serializable> initial_load(std::filesystem::path const& dst_file, Fn&& fn, Args&&... args)
    {
      auto const saved = load(dst_file);
      if (saved)
        return saved;

      auto const object = fn(std::forward<Args>(args)...);
      save(dst_file, object);
      return object;
    }

    std::optional<std::string_view> find_name(std::shared_ptr<serializable> const& obj)
    {
      auto const iter = _resources_by_object.find(obj);
      if (iter == _resources_by_object.end())
        return std::nullopt;
      return iter->second.name;
    }

    void save(std::filesystem::path const& file, std::shared_ptr<serializable> const& p)
    {
      auto const full_path = "assets" / file;
      std::filesystem::create_directories(full_path.parent_path());

      auto const name_str = file.string();
      auto const name_iter = _resources_by_name.find(name_str);
      if (name_iter != _resources_by_name.end())
        return;

      auto const obj_iter = _resources_by_object.find(p);
      if (obj_iter != _resources_by_object.end())
        return;

      std::ostringstream out;
      write(out, p);
      save_compressed(full_path, out);

      add_resource(name_str, p);
    }

    void write(std::ostream& stream, std::shared_ptr<serializable> const& p)
    {
      auto const iter = _names.find(typeid(*p).hash_code());
      if (iter == _names.end())
        throw std::runtime_error("Type not registered");

      auto const& name = iter->second;
      serializable::write_size(name.get(), stream);
      p->serialize(*this, stream);
    }

    void write_direct_or_reference(std::ostream& out, std::shared_ptr<serializable> const& p)
    {
      if (!p)
      {
        serializable::write_typed(serialize_reference_type::null, out);
        return;
      }

      auto const name = find_name(p);
      if (!name)
      {
        serializable::write_typed(serialize_reference_type::direct, out);
        write(out, p);
      }
      else
      {
        serializable::write_typed(serialize_reference_type::reference, out);
        serializable::write_string(*name, out);
      }
    }

    std::shared_ptr<serializable> read_direct_or_reference(std::istream& in)
    {
      serialize_reference_type ref = serialize_reference_type::direct;
      serializable::read_typed(ref, in);
      if (ref == serialize_reference_type::null)
      {
        return nullptr;
      }
      if (ref == serialize_reference_type::direct)
      {
        return read(in);
      }
      else
      {
        std::string name;
        serializable::read_string(name, in);
        return load(name);
      }
    }

    std::shared_ptr<serializable> read(std::istream& in)
    {
      std::size_t type = 0ull;
      serializable::read_size(type, in);
      auto const object = create(resource_id(type));
      if (object)
        object->deserialize(*this, in);
      else
        throw std::runtime_error("Type not registered");
      return object;
    }

    std::shared_ptr<serializable> load(std::filesystem::path file)
    {
      auto const full_path = "assets" / file;

      if (!exists(full_path))
        return nullptr;
      
      auto const name_str = file.string();
      auto const iter = _resources_by_name.find(name_str);
      if ((iter != _resources_by_name.end()) && !iter->second.expired())
        return iter->second.lock();

      auto in = load_compressed(full_path);
      auto const object = read(static_cast<std::istream&>(in));

      add_resource(name_str, object);

      return object;
    }

    template<typename T>
    void register_type(resource_id id)
    {
      auto& info = _type_infos[id.get()];
      _names[typeid(T).hash_code()] = id;
      info.create = &create_object<T>;
    }

  private:
    std::shared_ptr<serializable> create(resource_id id)
    {
      auto const iter = _type_infos.find(id.get());
      if (iter != _type_infos.end())
        return iter->second.create();
      return nullptr;
    }

    void add_resource(std::string name, std::shared_ptr<serializable> res)
    {
      std::string view = _resource_names.emplace_back(std::move(name));
      _resources_by_name[view] = res;
      _resources_by_object[res].name = view;
      save_resources();
    }

    void load_resources()
    {
      if (!std::filesystem::exists("assets/__meta.gevbin"))
        return;

      auto names_stream = load_compressed("assets/__meta.gevbin");

      std::string name;
      while (!names_stream.eof())
      {
        serializable::read_string(name, names_stream);
        if (name.empty())
          continue;
        _resources_by_name[name] = {};
      }
    }

    void save_resources()
    {
      std::ostringstream str;
      for (auto const& name : _resource_names)
      {
        serializable::write_string(name, str);
      }
      save_compressed("assets/__meta.gevbin", str);
    }

    void save_compressed(std::filesystem::path const& file, std::ostringstream& data);
    std::stringstream load_compressed(std::filesystem::path const& file);

    struct info
    {
      std::shared_ptr<serializable> (*create)();
    };

    struct resource
    {
      std::string name;
    };

    std::unordered_map<std::size_t, resource_id> _names;
    std::unordered_map<std::size_t, info> _type_infos;

    std::unordered_map<std::string, std::weak_ptr<serializable>> _resources_by_name;
    std::unordered_map<std::shared_ptr<serializable>, resource> _resources_by_object;
    std::vector<std::string> _resource_names;
  };
}    // namespace gev