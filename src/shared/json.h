#include <cassert>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <vector>

#ifndef TYPE_EXTRACTOR_JSON_H
#define TYPE_EXTRACTOR_JSON_H

inline std::string
json_from_map(const std::map<std::string, std::string> &properties) {
  std::string json = "{";
  for (const auto &pair : properties) {
    json += std::format(R"("{}":"{}",)", pair.first, pair.second);
  }
  if (!properties.empty()) {
    json.pop_back(); // Remove the last comma
  }
  json += "}";
  return json;
}

inline std::string json_from_vector(const std::vector<std::string> &elements) {
  std::string json = "[";
  for (const auto &element : elements) {
    json += std::format(R"("{}",)", element);
  }
  if (!elements.empty()) {
    json.pop_back(); // Remove the last comma
  }
  json += "]";
  return json;
}

typedef std::pair<std::optional<std::string>, std::string> DeclIDAndTypeName;
inline std::string
json_decl_id_and_type_name(const DeclIDAndTypeName &declIDAndTypeName) {
  const auto &declID = declIDAndTypeName.first;
  const auto &typeName = declIDAndTypeName.second;
  return declID ? std::format(R"({{"declID":"{}","typeName":"{}"}})", *declID,
                              typeName)
                : std::format(R"({{"declID":null,"typeName":"{}"}})", typeName);
}

typedef std::vector<std::pair<std::string, DeclIDAndTypeName>> OrderedTypesMap;

inline std::string json_ordered_types_map(const OrderedTypesMap &types) {
  std::string json = "[";
  for (const auto &pair : types) {
    auto typeIDAndName = json_decl_id_and_type_name(pair.second);
    json +=
        std::format(R"({{"name":"{}","type":{}}},)", pair.first, typeIDAndName);
  }
  if (!types.empty()) {
    json.pop_back(); // Remove the last comma
  }
  json += "]";
  return json;
}

inline std::string json_type_decl(const std::string &kind,
                                  const DeclIDAndTypeName &declIDAndTypeName,
                                  std::string &properties,
                                  const std::string &pseudoRoot,
                                  const std::vector<std::string> &location) {
  const auto declIDAndTypeNameStr =
      json_decl_id_and_type_name(declIDAndTypeName);
  assert(properties.front() == '{' &&
         "Properties should start with '{' for JSON formatting");
  // This should be a JSON object string, so we replace '{' with
  // '{"kind":"<kind>",' to add "kind" as the first property.
  // HACK: This is a bit fragile, but it works for our use case.
  properties.replace(properties.find('{'), 1,
                     std::format(R"({{"kind":"{}",)", kind));
  return std::format(
      R"({{"type":{},"properties":{},"pseudoRoot":"{}","location":{}}})",
      declIDAndTypeNameStr, properties, pseudoRoot, json_from_vector(location));
}

inline std::string
json_typdef_decl(const DeclIDAndTypeName &declIDAndTypeName,
                 const DeclIDAndTypeName &underlyingDeclIDAndTypeName,
                 const std::string &pseudoRoot,
                 const std::vector<std::string> &location) {
  // TODO: Determine if we should parse the type string further.
  auto declIDAndTypeNameStr = json_decl_id_and_type_name(declIDAndTypeName);
  auto underlyingDeclIDAndTypeNameStr =
      json_decl_id_and_type_name(underlyingDeclIDAndTypeName);
  auto properties =
      std::format(R"({{"underlyingType":{}}})", underlyingDeclIDAndTypeNameStr);
  return json_type_decl("Typedef", declIDAndTypeName, properties, pseudoRoot,
                        location);
}

typedef std::tuple<int64_t, int64_t, DeclIDAndTypeName> StructField;
inline std::string json_struct_field(const StructField &field) {
  auto [offset, size, declIDAndTypeName] = field;
  auto declIDAndTypeNameStr = json_decl_id_and_type_name(declIDAndTypeName);
  return std::format(R"({{"offset":{},"size":{},"type":{}}})", offset, size,
                     declIDAndTypeNameStr);
}

inline std::string json_struct_fields(
    const std::vector<std::pair<std::string, StructField>> &fields) {
  if (fields.empty()) {
    return "[]"; // Return empty array if no fields
  }
  std::vector<std::string> fieldJsons(fields.size());
  std::transform(fields.begin(), fields.end(), fieldJsons.begin(),
                 [](const std::pair<std::string, StructField> &pair) {
                   return std::format(R"("name":"{}","field":{})", pair.first,
                                      json_struct_field(pair.second));
                 });
  std::string json = "[";
  for (const auto &fieldJson : fieldJsons) {
    json += std::format(R"({{{}}},)", fieldJson);
  }
  json.pop_back(); // Remove the last comma
  json += "]";
  return json;
}

inline std::string
json_struct_decl(const DeclIDAndTypeName &declIDAndTypeName,
                 std::vector<std::pair<std::string, StructField>> &fields,
                 const std::string &pseudoRoot,
                 const std::vector<std::string> &location) {
  auto properties =
      std::format(R"({{"fields":{}}})", json_struct_fields(fields));
  return json_type_decl("Struct", declIDAndTypeName, properties, pseudoRoot,
                        location);
}

inline std::string json_union_decl(const DeclIDAndTypeName &declIDAndTypeName,
                                   const OrderedTypesMap &members,
                                   const std::string &pseudoRoot,
                                   const std::vector<std::string> &location) {
  auto properties =
      std::format(R"({{"members":{}}})", json_ordered_types_map(members));
  return json_type_decl("Union", declIDAndTypeName, properties, pseudoRoot,
                        location);
}

typedef std::pair<std::string, uint64_t> EnumEntry;

inline std::string json_enum_entry(const EnumEntry &entry) {
  return std::format(R"({{"name":"{}","value":{}}})", entry.first,
                     entry.second);
}

typedef std::vector<EnumEntry> EnumEntries;

inline std::string json_enum_entries(const EnumEntries &entries) {
  if (entries.empty()) {
    return "[]"; // Return empty array if no entries
  }
  std::string json = "[";
  for (const auto &entry : entries) {
    json += std::format(R"({},)", json_enum_entry(entry));
  }
  json.pop_back(); // Remove the last comma
  json += "]";
  return json;
}

inline std::string json_enum_decl(const DeclIDAndTypeName &declIDAndTypeName,
                                  const DeclIDAndTypeName &backingType,
                                  const EnumEntries &entries,
                                  const std::string &pseudoRoot,
                                  const std::vector<std::string> &location) {
  auto properties = std::format(R"({{"backingType":{},"entries":{}}})",
                                json_decl_id_and_type_name(backingType),
                                json_enum_entries(entries));
  return json_type_decl("Enum", declIDAndTypeName, properties, pseudoRoot,
                        location);
}

inline std::string
json_function_decl(const DeclIDAndTypeName &declIDAndTypeName,
                   const DeclIDAndTypeName &returnType,
                   const OrderedTypesMap &params, const std::string &pseudoRoot,
                   const std::vector<std::string> &location) {
  auto properties = std::format(R"({{"returnType":{},"params":{}}})",
                                json_decl_id_and_type_name(returnType),
                                json_ordered_types_map(params));
  return json_type_decl("Function", declIDAndTypeName, properties, pseudoRoot,
                        location);
}

#endif // TYPE_EXTRACTOR_JSON_H