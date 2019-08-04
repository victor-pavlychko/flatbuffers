/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// independent from idl_parser, since this code is not needed for most clients

#include "flatbuffers/code_generators.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

#include <unordered_set>

namespace flatbuffers {

// Pedantic warning free versions of toupper() and tolower().
inline char ToUpper(char c) { return static_cast<char>(::toupper(c)); }
inline char ToLower(char c) { return static_cast<char>(::tolower(c)); }

static std::string GeneratedFileName(const std::string &path,
                                     const std::string &file_name,
                                     const std::string &suffix) {
  return path + file_name + "_" + suffix;
}

namespace swift {
class SwiftGenerator : public BaseGenerator {
 public:
  SwiftGenerator(const Parser &parser, const std::string &path,
                 const std::string &file_name)
      : BaseGenerator(parser, path, file_name, "", "::") {
    static const char *const keywords[] = {
      "alignas",
      "alignof",
      "and",
      "and_eq",
      "asm",
      "atomic_cancel",
      "atomic_commit",
      "atomic_noexcept",
      "auto",
      "bitand",
      "bitor",
      "bool",
      "break",
      "case",
      "catch",
      "char",
      "char16_t",
      "char32_t",
      "class",
      "compl",
      "concept",
      "const",
      "constexpr",
      "const_cast",
      "continue",
      "co_await",
      "co_return",
      "co_yield",
      "decltype",
      "default",
      "delete",
      "do",
      "double",
      "dynamic_cast",
      "else",
      "enum",
      "explicit",
      "export",
      "extern",
      "false",
      "float",
      "for",
      "friend",
      "goto",
      "if",
      "import",
      "inline",
      "int",
      "long",
      "module",
      "mutable",
      "namespace",
      "new",
      "noexcept",
      "not",
      "not_eq",
      "nullptr",
      "operator",
      "or",
      "or_eq",
      "private",
      "protected",
      "public",
      "register",
      "reinterpret_cast",
      "requires",
      "return",
      "short",
      "signed",
      "sizeof",
      "static",
      "static_assert",
      "static_cast",
      "struct",
      "switch",
      "synchronized",
      "template",
      "this",
      "thread_local",
      "throw",
      "true",
      "try",
      "typedef",
      "typeid",
      "typename",
      "union",
      "unsigned",
      "using",
      "virtual",
      "void",
      "volatile",
      "wchar_t",
      "while",
      "xor",
      "xor_eq",
      nullptr,
    };
    for (auto kw = keywords; *kw; kw++) keywords_.insert(*kw);
  }

  std::string GenIncludeGuard() const {
    // Generate include guard.
    std::string guard = file_name_;
    // Remove any non-alpha-numeric characters that may appear in a filename.
    struct IsAlnum {
      bool operator()(char c) const { return !is_alnum(c); }
    };
    guard.erase(std::remove_if(guard.begin(), guard.end(), IsAlnum()),
                guard.end());
    guard = "FLATBUFFERS_GENERATED_SWIFT_" + guard;
    guard += "_";
    // For further uniqueness, also add the namespace.
    auto name_space = parser_.current_namespace_;
    for (auto it = name_space->components.begin();
         it != name_space->components.end(); ++it) {
      guard += *it + "_";
    }
    guard += "H_";
    std::transform(guard.begin(), guard.end(), guard.begin(), ToUpper);
    return guard;
  }

  void GenIncludeDependencies() {
    int num_includes = 0;
    for (auto it = parser_.native_included_files_.begin();
         it != parser_.native_included_files_.end(); ++it) {
      code_h_ += "#include \"" + *it + "\"";
      num_includes++;
    }
    for (auto it = parser_.included_files_.begin();
         it != parser_.included_files_.end(); ++it) {
      if (it->second.empty()) continue;
      auto noext = flatbuffers::StripExtension(it->second);
      auto basename = flatbuffers::StripPath(noext);

      code_h_ += "#include \"" + parser_.opts.include_prefix +
               (parser_.opts.keep_include_path ? noext : basename) +
               "_generated.h\"";
      num_includes++;
    }
    if (num_includes) code_h_ += "";
  }

  // Iterate through all definitions we haven't generate code for (enums,
  // structs, and tables) and output them to a single file.
  bool generate() {
    code_h_.Clear();
    code_h_ += "// " + std::string(FlatBuffersGeneratedWarning()) + "\n\n";

    const auto include_guard = GenIncludeGuard();
    code_h_ += "#ifndef " + include_guard;
    code_h_ += "#define " + include_guard;
    code_h_ += "";
    code_h_ += "#import \"flatbuffers_swift.h\"";
    code_h_ += "";

    code_mm_.Clear();
    code_mm_ += "// " + std::string(FlatBuffersGeneratedWarning()) + "\n\n";
    code_mm_ += "#import \"" + file_name_ + "_generated.h\"";
    code_mm_ += "#import \"" + file_name_ + "_swift_generated.h\"";
    code_mm_ += "";

    std::map<std::string, Type> arrays;

    for (auto it = parser_.structs_.vec.begin();
         it != parser_.structs_.vec.end(); ++it) {
      const auto &struct_def = **it;
      for (auto jt = struct_def.fields.vec.begin();
           jt != struct_def.fields.vec.end(); ++jt) {
        const auto &field = **jt;
        for (auto type = field.value.type;
             type.base_type == BASE_TYPE_VECTOR;
             type = type.VectorType()) {
          if (type.VectorType().base_type == BASE_TYPE_VECTOR ||
              type.VectorType().base_type == BASE_TYPE_STRUCT ||
              type.VectorType().base_type == BASE_TYPE_UNION) {
            arrays[GenTypeInternal(field.value.type)] = field.value.type;
          }
        }
      }
    }

    // Generate forward declarations for all structs/tables, since they may
    // have circular references.
    for (auto it = parser_.structs_.vec.begin();
         it != parser_.structs_.vec.end(); ++it) {
      const auto &struct_def = **it;
      if (!struct_def.generated) {
        GenStructDecl(struct_def);
      }
    }

    for (auto it = arrays.begin();
         it != arrays.end(); ++it) {
      const auto &type = it->second;
      GenArrayDecl(type);
    }

    // Generate code for all structs and tables.
    for (auto it = parser_.structs_.vec.begin();
         it != parser_.structs_.vec.end(); ++it) {
      const auto &struct_def = **it;
      if (!struct_def.generated) {
        GenStructFields(struct_def);
      }
    }

    for (auto it = arrays.begin();
         it != arrays.end(); ++it) {
      const auto &type = it->second;
      GenArrayFields(type);
    }

    code_h_ += "@interface FlatBufferBuilder (XXX)";
    code_mm_ += "@implementation FlatBufferBuilder (XXX)";
    code_mm_ += "";

    for (auto it = parser_.structs_.vec.begin();
         it != parser_.structs_.vec.end(); ++it) {
      const auto &struct_def = **it;
      if (!struct_def.generated && !struct_def.fixed) {
        GenBuilders(struct_def);
      }
    }

    for (auto it = arrays.begin();
         it != arrays.end(); ++it) {
      const auto &type = it->second;
      GenArrayBuilders(type);
    }

    if (parser_.root_struct_def_) {
      GenFinish(*parser_.root_struct_def_);
    }

    code_h_ += "@end";
    code_h_ += "";
    code_mm_ += "@end";
    code_mm_ += "";

    // Close the include guard.
    code_h_ += "#endif  // " + include_guard;

    const auto file_path_h = GeneratedFileName(path_, file_name_, "swift_generated.h");
    const auto final_code_h = code_h_.ToString();
    const auto file_path_mm = GeneratedFileName(path_, file_name_, "swift_generated.mm");
    const auto final_code_mm = code_mm_.ToString();
    const auto file_path_swift = GeneratedFileName(path_, file_name_, "swift_generated.swift");
    const auto final_code_swift = code_swift_.ToString();
    return
      SaveFile(file_path_h.c_str(), final_code_h, false) &&
      SaveFile(file_path_mm.c_str(), final_code_mm, false) &&
      SaveFile(file_path_swift.c_str(), final_code_swift, false);
  }

 private:
  CodeWriter code_h_;
  CodeWriter code_mm_;
  CodeWriter code_swift_;

  std::unordered_set<std::string> keywords_;

  // Translates a qualified name in flatbuffer text format to the same name in
  // the equivalent C++ namespace.
  static std::string TranslateNameSpace(const std::string &qualified_name) {
    std::string swift_qualified_name = qualified_name;
    size_t start_pos = 0;
    while ((start_pos = swift_qualified_name.find('.', start_pos)) !=
           std::string::npos) {
      swift_qualified_name.replace(start_pos, 1, "::");
    }
    return swift_qualified_name;
  }

  void GenComment(const std::vector<std::string> &dc, const char *prefix = "") {
    std::string text;
    ::flatbuffers::GenComment(dc, &text, nullptr, prefix);
    code_h_ += text + "\\";
  }

  std::string EscapeKeyword(const std::string &name) const {
    return keywords_.find(name) == keywords_.end() ? name : name + "_";
  }

  std::string Name(const Definition &def) const {
    return EscapeKeyword(def.name);
  }

  std::string SelectorComponentName(const std::string &name, bool first) const {
    std::string result = EscapeKeyword(name);
    result[0] = first ? ToUpper(name[0]) : ToLower(result[0]);
    return result;
  }

  std::string SelectorArgumentName(const std::string &name) const {
    std::string result = EscapeKeyword(name);
    result[0] = ToLower(result[0]);
    return result;
  }

  std::string TemporaryArgumentName(const std::string &name) const {
    return SelectorArgumentName(name) + "__";
  }

  std::string GenTypeInternal(const Definition &def) const {
    return EscapeKeyword(def.name);
  }

  std::string GenTypeInternal(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
        return "FlatBufferUInt8";
      case BASE_TYPE_UTYPE:
        return "FlatBufferUInt8";
      case BASE_TYPE_BOOL:
        return "FlatBufferBool";
      case BASE_TYPE_CHAR:
        return "FlatBufferInt8";
      case BASE_TYPE_UCHAR:
        return "FlatBufferUInt8";
      case BASE_TYPE_SHORT:
        return "FlatBufferInt16";
      case BASE_TYPE_USHORT:
        return "FlatBufferUInt16";
      case BASE_TYPE_INT:
        return "FlatBufferInt32";
      case BASE_TYPE_UINT:
        return "FlatBufferUInt32";
      case BASE_TYPE_LONG:
        return "FlatBufferInt64";
      case BASE_TYPE_ULONG:
        return "FlatBufferUInt64";
      case BASE_TYPE_FLOAT:
        return "FlatBufferFloat";
      case BASE_TYPE_DOUBLE:
        return "FlatBufferDouble";

      case BASE_TYPE_STRING:
        return "FlatBufferString";
      case BASE_TYPE_VECTOR:
        return GenTypeInternal(type.VectorType()) + "Array";
      case BASE_TYPE_STRUCT:
        return GenTypeInternal(*type.struct_def);

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTypeFlatbuffersOffset(const Definition &def) const {
    return "flatbuffers::Offset<" + GenTypeFlatbuffers(def) + ">";
  }

  std::string GenTypeFlatbuffersOffset(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
      case BASE_TYPE_UTYPE:
      case BASE_TYPE_BOOL:
      case BASE_TYPE_CHAR:
      case BASE_TYPE_UCHAR:
      case BASE_TYPE_SHORT:
      case BASE_TYPE_USHORT:
      case BASE_TYPE_INT:
      case BASE_TYPE_UINT:
      case BASE_TYPE_LONG:
      case BASE_TYPE_ULONG:
      case BASE_TYPE_FLOAT:
      case BASE_TYPE_DOUBLE:
        return GenTypeFlatbuffers(type);

      case BASE_TYPE_STRING:
      case BASE_TYPE_VECTOR:
        return "flatbuffers::Offset<" + GenTypeFlatbuffers(type) + ">";
      case BASE_TYPE_STRUCT:
        return GenTypeFlatbuffersOffset(*type.struct_def);

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTypeFlatbuffers(const Definition &def) const {
    return WrapInNameSpace(def);
  }

  std::string GenTypeFlatbuffers(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
        return "uint8_t";
      case BASE_TYPE_UTYPE:
        return "uint8_t";
      case BASE_TYPE_BOOL:
        return "bool";
      case BASE_TYPE_CHAR:
        return "int8_t";
      case BASE_TYPE_UCHAR:
        return "uint8_t";
      case BASE_TYPE_SHORT:
        return "int16_t";
      case BASE_TYPE_USHORT:
        return "uint16_t";
      case BASE_TYPE_INT:
        return "int32_t";
      case BASE_TYPE_UINT:
        return "uint32_t";
      case BASE_TYPE_LONG:
        return "int64_t";
      case BASE_TYPE_ULONG:
        return "uint64_t";
      case BASE_TYPE_FLOAT:
        return "float";
      case BASE_TYPE_DOUBLE:
        return "double";

      case BASE_TYPE_STRING:
        return "flatbuffers::String";
      case BASE_TYPE_VECTOR:
        return "flatbuffers::Vector<" + GenTypeFlatbuffersOffset(type.VectorType()) + ">";
      case BASE_TYPE_STRUCT:
        return GenTypeFlatbuffers(*type.struct_def);

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTypeOffset(const Definition &def) const {
    return GenTypeInternal(def) + "Offset";
  }

  std::string GenTypeOffset(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
      case BASE_TYPE_UTYPE:
      case BASE_TYPE_BOOL:
      case BASE_TYPE_CHAR:
      case BASE_TYPE_UCHAR:
      case BASE_TYPE_SHORT:
      case BASE_TYPE_USHORT:
      case BASE_TYPE_INT:
      case BASE_TYPE_UINT:
      case BASE_TYPE_LONG:
      case BASE_TYPE_ULONG:
      case BASE_TYPE_FLOAT:
      case BASE_TYPE_DOUBLE:
        abort();

      case BASE_TYPE_STRING:
      case BASE_TYPE_VECTOR:
      case BASE_TYPE_STRUCT:
        return GenTypeInternal(type) + "Offset";

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTypeRef(const Definition &def) const {
    return GenTypeInternal(def) + "Ref";
  }

  std::string GenTypeRef(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
      case BASE_TYPE_UTYPE:
      case BASE_TYPE_BOOL:
      case BASE_TYPE_CHAR:
      case BASE_TYPE_UCHAR:
      case BASE_TYPE_SHORT:
      case BASE_TYPE_USHORT:
      case BASE_TYPE_INT:
      case BASE_TYPE_UINT:
      case BASE_TYPE_LONG:
      case BASE_TYPE_ULONG:
      case BASE_TYPE_FLOAT:
      case BASE_TYPE_DOUBLE:
        abort();

      case BASE_TYPE_STRING:
      case BASE_TYPE_VECTOR:
      case BASE_TYPE_STRUCT:
        return GenTypeInternal(type) + "Ref";

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTypeForGet(const Definition &def) const {
    return GenTypeRef(def);
  }

  std::string GenTypeForGet(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
        return "uint8_t";
      case BASE_TYPE_UTYPE:
        return "uint8_t";
      case BASE_TYPE_BOOL:
        return "bool";
      case BASE_TYPE_CHAR:
        return "int8_t";
      case BASE_TYPE_UCHAR:
        return "uint8_t";
      case BASE_TYPE_SHORT:
        return "int16_t";
      case BASE_TYPE_USHORT:
        return "uint16_t";
      case BASE_TYPE_INT:
        return "int32_t";
      case BASE_TYPE_UINT:
        return "uint32_t";
      case BASE_TYPE_LONG:
        return "int64_t";
      case BASE_TYPE_ULONG:
        return "uint64_t";
      case BASE_TYPE_FLOAT:
        return "float";
      case BASE_TYPE_DOUBLE:
        return "double";

      case BASE_TYPE_STRING:
        return "NSString *";
      case BASE_TYPE_VECTOR:
        return GenTypeRef(type);
      case BASE_TYPE_STRUCT:
        return GenTypeForGet(*type.VectorType().struct_def);

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTypeForSet(const Definition &def) const {
    return GenTypeOffset(def);
  }

  std::string GenTypeForSet(const StructDef &def) const {
    return def.fixed
      ? "const " + GenTypeInternal(def) + " *"
      : GenTypeOffset(def);
  }

  std::string GenTypeForSet(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
        return "uint8_t";
      case BASE_TYPE_UTYPE:
        return "uint8_t";
      case BASE_TYPE_BOOL:
        return "bool";
      case BASE_TYPE_CHAR:
        return "int8_t";
      case BASE_TYPE_UCHAR:
        return "uint8_t";
      case BASE_TYPE_SHORT:
        return "int16_t";
      case BASE_TYPE_USHORT:
        return "uint16_t";
      case BASE_TYPE_INT:
        return "int32_t";
      case BASE_TYPE_UINT:
        return "uint32_t";
      case BASE_TYPE_LONG:
        return "int64_t";
      case BASE_TYPE_ULONG:
        return "uint64_t";
      case BASE_TYPE_FLOAT:
        return "float";
      case BASE_TYPE_DOUBLE:
        return "double";

      case BASE_TYPE_STRING:
        return "FlatBufferStringOffset";
      case BASE_TYPE_VECTOR:
        return GenTypeOffset(type);
      case BASE_TYPE_STRUCT:
        return GenTypeForSet(*type.VectorType().struct_def);

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTypeForKey(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
        return "uint8_t";
      case BASE_TYPE_UTYPE:
        return "uint8_t";
      case BASE_TYPE_BOOL:
        return "bool";
      case BASE_TYPE_CHAR:
        return "int8_t";
      case BASE_TYPE_UCHAR:
        return "uint8_t";
      case BASE_TYPE_SHORT:
        return "int16_t";
      case BASE_TYPE_USHORT:
        return "uint16_t";
      case BASE_TYPE_INT:
        return "int32_t";
      case BASE_TYPE_UINT:
        return "uint32_t";
      case BASE_TYPE_LONG:
        return "int64_t";
      case BASE_TYPE_ULONG:
        return "uint64_t";
      case BASE_TYPE_FLOAT:
        return "float";
      case BASE_TYPE_DOUBLE:
        return "double";

      case BASE_TYPE_STRING:
        return "NSString *";
      case BASE_TYPE_VECTOR:
        abort();
      case BASE_TYPE_STRUCT:
        abort();

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  // Return a C++ type from the table in idl.h
  std::string GenTypeCastGet(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
      case BASE_TYPE_UTYPE:
      case BASE_TYPE_BOOL:
      case BASE_TYPE_CHAR:
      case BASE_TYPE_UCHAR:
      case BASE_TYPE_SHORT:
      case BASE_TYPE_USHORT:
      case BASE_TYPE_INT:
      case BASE_TYPE_UINT:
      case BASE_TYPE_LONG:
      case BASE_TYPE_ULONG:
      case BASE_TYPE_FLOAT:
      case BASE_TYPE_DOUBLE:
        return "value";

      case BASE_TYPE_STRING:
        return "[[NSString alloc] initWithBytesNoCopy:const_cast<char *>(value->c_str()) length:value->Length() encoding:NSUTF8StringEncoding freeWhenDone:NO]";
      case BASE_TYPE_VECTOR:
      case BASE_TYPE_STRUCT:
        return "{ .buf = value }";

      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  // Return a C++ type from the table in idl.h
  std::string GenTypeCastKey(const Type &type) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
      case BASE_TYPE_UTYPE:
      case BASE_TYPE_BOOL:
      case BASE_TYPE_CHAR:
      case BASE_TYPE_UCHAR:
      case BASE_TYPE_SHORT:
      case BASE_TYPE_USHORT:
      case BASE_TYPE_INT:
      case BASE_TYPE_UINT:
      case BASE_TYPE_LONG:
      case BASE_TYPE_ULONG:
      case BASE_TYPE_FLOAT:
      case BASE_TYPE_DOUBLE:
        return "key";

      case BASE_TYPE_STRING:
        return "key.UTF8String ?: \"\"";

      case BASE_TYPE_VECTOR:
        abort();
      case BASE_TYPE_STRUCT:
        abort();
      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  // Return a C++ type from the table in idl.h
  std::string GenTypeCastSet(const Type &type, const std::string &name) const {
    switch (type.base_type) {
      case BASE_TYPE_NONE:
      case BASE_TYPE_UTYPE:
      case BASE_TYPE_BOOL:
      case BASE_TYPE_CHAR:
      case BASE_TYPE_UCHAR:
      case BASE_TYPE_SHORT:
      case BASE_TYPE_USHORT:
      case BASE_TYPE_INT:
      case BASE_TYPE_UINT:
      case BASE_TYPE_LONG:
      case BASE_TYPE_ULONG:
      case BASE_TYPE_FLOAT:
      case BASE_TYPE_DOUBLE:
        return EscapeKeyword(name);

      case BASE_TYPE_STRING:
      case BASE_TYPE_VECTOR:
        return "{ " + EscapeKeyword(name) + ".offset }";
      case BASE_TYPE_STRUCT:
        if (type.struct_def->fixed) {
          return SelectorArgumentName(name) + " ? &" + TemporaryArgumentName(name) + " : nullptr";
        } else {
          return "{ " + EscapeKeyword(name) + ".offset }";
        }
      case BASE_TYPE_UNION:
        abort();
      case BASE_TYPE_ARRAY:
        abort();
      default:
        abort();
    }
  }

  std::string GenTemporaryStruct(const StructDef &struct_def, const std::string &name) const {
    std::string result = "auto " + TemporaryArgumentName(name) + " = " + SelectorArgumentName(name) + " ? " + GenTypeFlatbuffers(struct_def) + "(";
    bool first = true;
    for (auto it = struct_def.fields.vec.begin();
         it != struct_def.fields.vec.end(); ++it) {
      const auto &field = **it;
      if (!field.deprecated) {
        if (!first) { result += ", "; }
        result += name + "->" + Name(field);
        first = false;
      }
    }
    result += ") : " + GenTypeFlatbuffers(struct_def) + "();";
    return result;
  }

  std::string GenTemporaryString(const std::string &name, bool shared) const {
    std::string createString = shared ? "CreateSharedString" : "SharedString";
    return
      "auto " + TemporaryArgumentName(name) + " = " +
      EscapeKeyword(name) + " ? _fbb->" + createString + "(" + EscapeKeyword(name) + ".UTF8String) : 0;";
  }

  std::string GenParamSwift(const FieldDef &field, bool first) {
    return
      SelectorComponentName(field.name, first) + ":(" +
      GenTypeForSet(field.value.type) + ")" +
      SelectorArgumentName(field.name);
  }

  std::string GenCreateSelector(const StructDef &struct_def) {
    std::string result = "make" + GenTypeInternal(struct_def) + "With";
    bool first = true;
    for (auto it = struct_def.fields.vec.begin();
         it != struct_def.fields.vec.end(); ++it) {
      const auto &field = **it;
      if (!field.deprecated) {
        if (!first) { result += " "; }
        result += GenParamSwift(field, first);
        first = false;
      }
    }
    return result;
  }

  void GenBuilders(const StructDef &struct_def) {
    code_h_.SetValue("OFFSET_NAME", GenTypeOffset(struct_def));
    code_mm_.SetValue("OFFSET_NAME", GenTypeOffset(struct_def));
    code_mm_.SetValue("CREATE_NAME", WrapInNameSpace(struct_def.defined_namespace, "Create" + Name(struct_def)));

    code_h_.SetValue("SELECTOR_DECL", GenCreateSelector(struct_def));
    code_h_ += "- ({{OFFSET_NAME}}){{SELECTOR_DECL}};";

    code_mm_.SetValue("SELECTOR_DECL", GenCreateSelector(struct_def));
    code_mm_ += "- ({{OFFSET_NAME}}){{SELECTOR_DECL}} {";
    for (auto it = struct_def.fields.vec.begin();
         it != struct_def.fields.vec.end(); ++it) {
      const auto &field = **it;
      if (!field.deprecated && field.value.type.base_type == BASE_TYPE_STRUCT && field.value.type.struct_def->fixed) {
        code_mm_ += "  " + GenTemporaryStruct(*field.value.type.struct_def, field.name);
      }
    }
    code_mm_ += "  return { .offset = {{CREATE_NAME}}(*_fbb";
    for (auto it = struct_def.fields.vec.begin();
         it != struct_def.fields.vec.end(); ++it) {
      const auto &field = **it;
      if (!field.deprecated) {
        code_mm_.SetValue("FIELD_CAST", GenTypeCastSet(field.value.type, field.name));
        code_mm_ += "    , {{FIELD_CAST}}";
      }
    }
    code_mm_ += "  ).o };";
    code_mm_ += "}";
    code_mm_ += "";
  }

  void GenArrayBuilders(const Type &type) {
    code_h_.SetValue("OFFSET_NAME", GenTypeOffset(type));
    code_h_.SetValue("ELEMENT_NAME", GenTypeInternal(type.VectorType()));
    code_h_.SetValue("ELEMENT_OFFSET", GenTypeOffset(type.VectorType()));
    code_mm_.SetValue("OFFSET_NAME", GenTypeOffset(type));
    code_mm_.SetValue("ELEMENT_NAME", GenTypeInternal(type.VectorType()));
    code_mm_.SetValue("ELEMENT_OFFSET", GenTypeOffset(type.VectorType()));
    code_mm_.SetValue("ELEMENT_FLATBUF", GenTypeFlatbuffersOffset(type.VectorType()));

    code_h_ += "- ({{OFFSET_NAME}})make{{ELEMENT_NAME}}Array:(const {{ELEMENT_OFFSET}} *)elements count:(NSInteger)count;";

    code_mm_ += "- ({{OFFSET_NAME}})make{{ELEMENT_NAME}}Array:(const {{ELEMENT_OFFSET}} *)elements count:(NSInteger)count {";
    code_mm_ += "  return { .offset = _fbb->CreateVector(reinterpret_cast<const {{ELEMENT_FLATBUF}} *>(elements), count).o };";
    code_mm_ += "}";
    code_mm_ += "";

    if (type.VectorType().base_type == BASE_TYPE_STRUCT && type.VectorType().struct_def->has_key) {
      code_h_ += "- ({{OFFSET_NAME}})make{{ELEMENT_NAME}}SortedArray:({{ELEMENT_OFFSET}} *)elements count:(NSInteger)count;";

      code_mm_ += "- ({{OFFSET_NAME}})make{{ELEMENT_NAME}}SortedArray:({{ELEMENT_OFFSET}} *)elements count:(NSInteger)count {";
      code_mm_ += "  return { .offset = _fbb->CreateVectorOfSortedTables(reinterpret_cast<{{ELEMENT_FLATBUF}} *>(elements), count).o };";
      code_mm_ += "}";
      code_mm_ += "";
    }
  }

  // Generate an accessor struct with constructor for a flatbuffers struct.
  void GenStructDecl(const StructDef &struct_def) {
    GenComment(struct_def.doc_comment);
    code_h_.SetValue("REF_NAME", GenTypeRef(struct_def));
    code_h_.SetValue("OFFSET_NAME", GenTypeOffset(struct_def));

    if (struct_def.fixed) {
      code_h_.SetValue("STRUCT_NAME", GenTypeInternal(struct_def));
      code_h_ += "typedef struct {{STRUCT_NAME}} {";
      for (auto it = struct_def.fields.vec.begin();
           it != struct_def.fields.vec.end(); ++it) {
        const auto &field = **it;

        code_h_.SetValue("FIELD_NAME", Name(field));
        code_h_.SetValue("FIELD_TYPE", GenTypeForGet(field.value.type));
        GenComment(field.doc_comment, "  ");
        code_h_ += "  {{FIELD_TYPE}} {{FIELD_NAME}};";
      }
      code_h_ += "} {{STRUCT_NAME}};";
      code_h_ += "";
    }

    code_h_ += "typedef struct {{REF_NAME}} { const void *buf; } {{REF_NAME}};";
    code_h_ += "typedef struct {{OFFSET_NAME}} { const uint32_t offset; } {{OFFSET_NAME}};";
    code_h_ += "";
  }

  // Generate an accessor struct with constructor for a flatbuffers struct.
  void GenStructFields(const StructDef &struct_def) {
    code_h_.SetValue("REF_NAME", GenTypeRef(struct_def));
    code_mm_.SetValue("REF_NAME", GenTypeRef(struct_def));
    code_mm_.SetValue("FLATBUF_NAME", GenTypeFlatbuffers(struct_def));

    for (auto it = struct_def.fields.vec.begin();
         it != struct_def.fields.vec.end(); ++it) {
      const auto &field = **it;

      code_h_.SetValue("FIELD_NAME", Name(field));
      code_h_.SetValue("FIELD_TYPE", GenTypeForGet(field.value.type));
      code_mm_.SetValue("FIELD_NAME", Name(field));
      code_mm_.SetValue("FIELD_TYPE", GenTypeForGet(field.value.type));
      code_mm_.SetValue("FIELD_CAST", GenTypeCastGet(field.value.type));

      GenComment(field.doc_comment, "  ");
      code_h_ += "{{FIELD_TYPE}} {{REF_NAME}}_{{FIELD_NAME}}({{REF_NAME}} self_) NS_SWIFT_NAME(getter:{{REF_NAME}}.{{FIELD_NAME}}(self:));";

      code_mm_ += "{{FIELD_TYPE}} {{REF_NAME}}_{{FIELD_NAME}}({{REF_NAME}} self_) {";
      code_mm_ += "  auto value = reinterpret_cast<const {{FLATBUF_NAME}} *>(self_.buf)->{{FIELD_NAME}}();";
      code_mm_ += "  return {{FIELD_CAST}};";
      code_mm_ += "}";
      code_mm_ += "";
    }

    code_h_ += "";
  }

  // Generate an accessor struct with constructor for a flatbuffers struct.
  void GenArrayDecl(const Type &type) {
    code_h_.SetValue("REF_NAME", GenTypeRef(type));
    code_h_.SetValue("OFFSET_NAME", GenTypeOffset(type));

    code_h_ += "typedef struct {{REF_NAME}} { const void *buf; } {{REF_NAME}};";
    code_h_ += "typedef struct {{OFFSET_NAME}} { const uint32_t offset; } {{OFFSET_NAME}};";
    code_h_ += "";
  }

  // Generate an accessor struct with constructor for a flatbuffers struct.
  void GenArrayFields(const Type &type) {
    code_h_.SetValue("REF_NAME", GenTypeRef(type));
    code_h_.SetValue("OFFSET_NAME", GenTypeOffset(type));
    code_h_.SetValue("ELEMENT_NAME", GenTypeRef(type.VectorType()));
    code_mm_.SetValue("REF_NAME", GenTypeRef(type));
    code_mm_.SetValue("ELEMENT_NAME", GenTypeForGet(type.VectorType()));
    code_mm_.SetValue("ELEMENT_FLATBUF", GenTypeFlatbuffers(*type.VectorType().struct_def));

    code_h_ += "NSInteger {{REF_NAME}}_count({{REF_NAME}} self_) NS_SWIFT_NAME(getter:{{REF_NAME}}.count(self:));";
    code_h_ += "{{ELEMENT_NAME}} {{REF_NAME}}_subscript({{REF_NAME}} self_, NSInteger index) NS_SWIFT_NAME(getter:{{REF_NAME}}.subscript(self:_:));";

    code_mm_ += "NSInteger {{REF_NAME}}_count({{REF_NAME}} self_) {";
    code_mm_ += "  auto value = reinterpret_cast<const flatbuffers::Vector<flatbuffers::Offset<{{ELEMENT_FLATBUF}}>> *>(self_.buf)->Length();";
    code_mm_ += "  return static_cast<NSInteger>(value);";
    code_mm_ += "}";
    code_mm_ += "";
    code_mm_ += "{{ELEMENT_NAME}} {{REF_NAME}}_subscript({{REF_NAME}} self_, NSInteger index) {";
    code_mm_ += "  auto value = reinterpret_cast<const flatbuffers::Vector<flatbuffers::Offset<{{ELEMENT_FLATBUF}}>> *>(self_.buf)->Get(static_cast<flatbuffers::uoffset_t>(index));";
    code_mm_ += "  return { .buf = value };";
    code_mm_ += "}";
    code_mm_ += "";

    if (type.VectorType().base_type == BASE_TYPE_STRUCT && type.VectorType().struct_def->has_key) {
      code_mm_.SetValue("FIELD_CAST", GenTypeCastGet(type.VectorType()));

      for (auto it = type.VectorType().struct_def->fields.vec.begin();
           it != type.VectorType().struct_def->fields.vec.end(); ++it) {
        const auto &field = **it;
        if (field.key) {
          code_h_.SetValue("KEY_TYPE", GenTypeForKey(field.value.type));
          code_mm_.SetValue("KEY_TYPE", GenTypeForKey(field.value.type));
          code_mm_.SetValue("KEY_CAST", GenTypeCastKey(field.value.type));
          break;
        }
      }

      code_h_ += "{{ELEMENT_NAME}} {{REF_NAME}}_lookupByKey({{REF_NAME}} self_, {{KEY_TYPE}} key) NS_SWIFT_NAME({{REF_NAME}}.lookup(self:by:));";

      code_mm_ += "{{ELEMENT_NAME}} {{REF_NAME}}_lookupByKey({{REF_NAME}} self_, {{KEY_TYPE}} key) {";
      code_mm_ += "  auto value = reinterpret_cast<const flatbuffers::Vector<flatbuffers::Offset<{{ELEMENT_FLATBUF}}>> *>(self_.buf)->LookupByKey({{KEY_CAST}});";
      code_mm_ += "  return {{FIELD_CAST}};";
      code_mm_ += "}";
      code_mm_ += "";
    }

    code_h_ += "";
  }

  // Generate an accessor struct with constructor for a flatbuffers struct.
  void GenFinish(const StructDef &struct_def) {
    code_h_.SetValue("REF_NAME", GenTypeRef(struct_def));
    code_h_.SetValue("INTERNAL_NAME", GenTypeInternal(struct_def));
    code_h_.SetValue("OFFSET_NAME", GenTypeOffset(struct_def));
    code_mm_.SetValue("REF_NAME", GenTypeRef(struct_def));
    code_mm_.SetValue("INTERNAL_NAME", GenTypeInternal(struct_def));
    code_mm_.SetValue("OFFSET_NAME", GenTypeOffset(struct_def));
    code_mm_.SetValue("FLATBUF_NAME", GenTypeFlatbuffers(struct_def));

    code_h_ += "- (void)finishWith{{INTERNAL_NAME}}:({{OFFSET_NAME}})offset;";

    code_mm_ += "- (void)finishWith{{INTERNAL_NAME}}:({{OFFSET_NAME}})offset {";
    code_mm_ += "  _fbb->Finish(flatbuffers::Offset<{{FLATBUF_NAME}}>(offset.offset));";
    code_mm_ += "}";
    code_mm_ += "";
  }
};

}  // namespace swift

bool GenerateSwift(const Parser &parser, const std::string &path,
                 const std::string &file_name) {
  swift::SwiftGenerator generator(parser, path, file_name);
  return generator.generate();
}

std::string SwiftMakeRule(const Parser &parser, const std::string &path,
                        const std::string &file_name) {
  const auto filebase =
      flatbuffers::StripPath(flatbuffers::StripExtension(file_name));
  const auto included_files = parser.GetIncludedFilesRecursive(file_name);
  std::string make_rule = ""; // GeneratedFileName(path, filebase) + ": ";
//  for (auto it = included_files.begin(); it != included_files.end(); ++it) {
//    make_rule += " " + *it;
//  }
  return make_rule;
}

}  // namespace flatbuffers
