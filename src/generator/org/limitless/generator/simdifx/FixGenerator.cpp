//
// Created by Fredrik Dahlberg on 2026-06-06.
//

#include <filesystem>
#include <fstream>
#include <print>
#include <set>

#include "org/limitless/generator/simdifx/ConfigMapping.hpp"
#include "org/limitless/generator/simdifx/DataModel.hpp"

using namespace org::limitless::simdifx;
using namespace org::limitless::generator::simdifx;

static void generateTypes(const std::string& fileName,
                          const std::vector<Record>& enums);
static void generateDecoders(const std::string& fileName,
                             const std::vector<Record>& records);
static void generateEncoders(const std::string& fileName,
                             const std::vector<Record>& records);

static void generateMessageHandler(const std::string& fileName,
                                   const std::vector<Record>& records);

static void generateEngineConfig(const std::string& fileName,
                                 const pugi::xml_document& config,
                                 const Record& protocol);

static void generateConfig(const std::string& fileName,
                           const pugi::xml_document& config);

static void generateRecordDecoders(std::ostream& out, const Record& record);
static void generateRecordEncoders(std::ostream& out, const Record& record);

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::println("Usage: generator <session file> <output directory> <config file> <config output directory> [<application file>]");
        return 1;
    }

    pugi::xml_document doc;
    if (const auto result = doc.load_file(argv[1]); !result)
    {
        std::println("Session XML error: {}, dir = {}, file = {}", result.description(),
                     std::filesystem::current_path().c_str(), argv[1]);
        return 1;
    }

    DataModel model{};
    model.process(doc);

    if (argc >= 6)
    {
        pugi::xml_document appDoc;
        if (const auto result = appDoc.load_file(argv[5]); !result)
        {
            std::println("Application XML error: {}, file = {}", result.description(), argv[5]);
            return 1;
        }
        model.merge(appDoc);
    }

    std::string typesFile{argv[2]};
    typesFile.append("/FixTypes.hpp");
    generateTypes(typesFile, model.m_enums);

    std::string decodersFile{argv[2]};
    decodersFile.append("/FixMessageDecoders.hpp");
    generateDecoders(decodersFile, model.m_records);

    std::string handlerFile{argv[2]};
    handlerFile.append("/FixMessageHandler.hpp");
    generateMessageHandler(handlerFile, model.m_records);

    std::string encodersFile{argv[2]};
    encodersFile.append("/FixMessageEncoders.hpp");
    generateEncoders(encodersFile, model.m_records);

    pugi::xml_document config;
    const auto configResult = config.load_file(argv[3]);
    if (!configResult)
    {
        std::println("Config XML error: {}, file = {}", configResult.description(), argv[3]);
        return 1;
    }
    model.processVersions(config.child("config").child("versions"));
    std::filesystem::create_directories(argv[4]);

    std::string engineFile{argv[4]};
    engineFile.append("/FixEngine.hpp");
    generateEngineConfig(engineFile, config, model.m_protocol);

    std::string configFile{argv[4]};
    configFile.append("/FixConfig.hpp");
    generateConfig(configFile, config);

    return 0;
}

// Emits Result-style free helpers for a generated enum class:
//   name()     maps a value to its enumerator identifier,
//   code()     maps a value to its FIX wire code,
//   from() maps a FIX wire code back to a value.
// name()/code() switch on the value, mirroring the name(Result) helper in
// Types.hpp. from() takes a dummy enum argument so the generic decoder can
// dispatch to the right overload by argument-dependent lookup.
static void generateEnumFunctions(std::ostream& out, const std::string& name,
                                  const std::vector<Field>& fields)
{
    out << std::format("constexpr std::string_view name(const {} value)\n", name);
    out << "{\n";
    out << "    switch (value)\n    {\n";
    for (const auto& field : fields)
    {
        out << std::format("        case {}::{}: return \"{}\";\n", name, field.m_name, field.m_name);
    }
    out << "        default: return \"?\";\n";
    out << "    }\n}\n\n";

    out << std::format("constexpr std::string_view code(const {} value)\n", name);
    out << "{\n";
    out << "    switch (value)\n    {\n";
    for (const auto& field : fields)
    {
        out << std::format("        case {}::{}: return \"{}\";\n", name, field.m_name, field.m_type);
    }
    out << "        default: return \"?\";\n";
    out << "    }\n}\n\n";

    bool hasSingle = false;
    bool hasMulti = false;
    for (const auto& field : fields)
    {
        if (field.m_name == "Null")
        {
            continue;
        }
        (field.m_type.size() == 1 ? hasSingle : hasMulti) = true;
    }

    out << std::format("constexpr {} from(const std::string_view code, const {})\n", name, name);
    out << "{\n";
    if (hasSingle)
    {
        out << "    if (code.size() == 1)\n    {\n";
        out << "        switch (code[0])\n        {\n";
        for (const auto& field : fields)
        {
            if (field.m_name == "Null" || field.m_type.size() != 1)
            {
                continue;
            }
            out << std::format("            case '{}': return {}::{};\n", field.m_type, name, field.m_name);
        }
        out << "            default: break;\n        }\n    }\n";
    }
    if (hasMulti)
    {
        for (const auto& field : fields)
        {
            if (field.m_name == "Null" || field.m_type.size() == 1)
            {
                continue;
            }
            out << std::format("    if (code == \"{}\") return {}::{};\n", field.m_type, name, field.m_name);
        }
    }
    out << std::format("    return {}::Null;\n", name);
    out << "}\n\n";
}

static void generateEnums(std::ostream& out, const Record& record)
{
    out << std::format("enum class {} : uint8_t\n", record.m_name);
    out << "{\n";
    const auto end = record.m_fields.end();
    for (auto value = record.m_fields.begin(); value != end; ++value)
    {
        out << std::format("    {}{}\n", value->m_name, value != end - 1 ? "," : "");
    }
    out << "};\n\n";

    generateEnumFunctions(out, record.m_name, record.m_fields);
}

static void generateProtocol(std::ostream& out, const Record& protocol)
{
    out << "enum class Protocol : uint8_t\n{\n";
    const auto end = protocol.m_fields.end();
    for (auto value = protocol.m_fields.begin(); value != end; ++value)
    {
        out << std::format("    {}{}\n", value->m_name, value != end - 1 ? "," : "");
    }
    out << "};\n\n";

    generateEnumFunctions(out, "Protocol", protocol.m_fields);

    for (const auto& field : protocol.m_fields)
    {
        if (field.m_name != "Null")
        {
            out << std::format("inline constexpr FixedString {} {{\"{}\"}};\n",
                               field.m_name, field.m_type);
        }
    }
    out << "\n";
}

static void generateTypes(const std::string& fileName,
                          const std::vector<Record>& enums)
{
    std::ofstream out(fileName);
    if (!out)
    {
        std::println("Types/Error: could not open '{}' for writing", fileName);
        return;
    }

    out << "// Generated by Generator. Do not edit by hand.\n";
    out << "#ifndef SIMD_FIX_FIX_TYPES_HPP\n";
    out << "#define SIMD_FIX_FIX_TYPES_HPP\n\n";
    out << "#include <cstdint>\n";
    out << "#include <string_view>\n\n";
    out << "#include \"org/limitless/simdifx/Types.hpp\"\n\n";
    out << "namespace org::limitless::simdifx::generated::messages {\n\n";
    for (const auto& type: enums)
    {
        generateEnums(out, type);
    }
    out << "} // namespace org::limitless::simdifx::generated::messages\n\n";
    out << "#endif //SIMD_FIX_FIX_TYPES_HPP\n";

    out.close();
}

static void generateDecoders(const std::string& fileName,
                             const std::vector<Record>& records)
{
    std::ofstream out(fileName);
    if (!out)
    {
        std::println("Decoder/Error: could not open '{}' for writing", fileName);
        return;
    }

    out << "// Generated by Generator. Do not edit by hand.\n";
    out << "#ifndef SIMD_FIX_MESSAGE_DECODERS_HPP\n";
    out << "#define SIMD_FIX_MESSAGE_DECODERS_HPP\n\n";
    out << "#include \"org/limitless/simdifx/detail/Expected.hpp\"\n\n";
    out << "#include \"org/limitless/simdifx/generated/config/FixEngine.hpp\"\n";
    out << "#include \"org/limitless/simdifx/generated/messages/FixTypes.hpp\"\n";
    out << "#include \"org/limitless/simdifx/decoder/DataDecoder.hpp\"\n";
    out << "#include \"org/limitless/simdifx/decoder/GroupDecoder.hpp\"\n";
    out << "#include \"org/limitless/simdifx/decoder/MessageDecoder.hpp\"\n\n";
    out << "namespace org::limitless::simdifx::generated::messages {\n\n";
    out << "using config::Protocol;\n";
    out << "using namespace org::limitless::simdifx::decoder;\n\n";
    for (auto& record: records)
    {
        generateRecordDecoders(out, record);
    }
    out << "} // namespace org::limitless::simdifx::generated::messages\n\n";
    out << "#endif //SIMD_FIX_MESSAGE_DECODERS_HPP\n";

    out.close();
}

static void generateEncoders(const std::string& fileName,
                             const std::vector<Record>& records)
{
    std::ofstream out(fileName);
    if (!out)
    {
        std::println("Encoder/Error: could not open '{}' for writing", fileName);
        return;
    }

    out << "// Generated by Generator. Do not edit by hand.\n";
    out << "#ifndef SIMD_FIX_MESSAGE_ENCODERS_HPP\n";
    out << "#define SIMD_FIX_MESSAGE_ENCODERS_HPP\n\n";
    out << "#include \"org/limitless/simdifx/generated/config/FixEngine.hpp\"\n";
    out << "#include \"org/limitless/simdifx/generated/messages/FixTypes.hpp\"\n";
    out << "#include \"org/limitless/simdifx/encoder/DataEncoder.hpp\"\n";
    out << "#include \"org/limitless/simdifx/encoder/GroupEncoder.hpp\"\n";
    out << "#include \"org/limitless/simdifx/encoder/MessageEncoder.hpp\"\n";
    out << "#include \"org/limitless/simdifx/encoder/PayloadEncoder.hpp\"\n\n";
    out << "namespace org::limitless::simdifx::generated::messages {\n\n";
    out << "using namespace org::limitless::simdifx::encoder;\n\n";
    for (auto& record: records)
    {
        generateRecordEncoders(out, record);
    }

    out << "template <FixedString Protocol, FixedString Target, FixedString Sender>\n";
    out << "class FixPayloadEncoder\n";
    out << "{\n";
    out << "    PayloadEncoder<Protocol, Target, Sender> m_encoder;\n";
    out << "public:\n";
    out << "    FixPayloadEncoder() = default;\n\n";

    out << "    FixPayloadEncoder& wrap(const uint32_t offset, const std::span<uint8_t> data)\n";
    out << "    {\n";
    out << "        m_encoder.wrap(offset, data);\n";
    out << "        return *this;\n";
    out << "    }\n\n";

    out << "    [[nodiscard]] uint32_t offset() const\n";
    out << "    {\n";
    out << "        return m_encoder.offset();\n";
    out << "    }\n\n";

    out << "    template <typename MessageEncoderType>\n";
    out << "    MessageEncoderType& wrapMessage(MessageEncoderType& message) const\n";
    out << "    {\n";
    out << "        return m_encoder.wrapMessage(message);\n";
    out << "    }\n\n";

    out << "    template <typename MessageEncoderType>\n";
    out << "    MessageEncoderType& wrapHeader(MessageEncoderType& message,\n";
    out << "                                   const uint32_t sequenceNumber,\n";
    out << "                                   const std::chrono::milliseconds sendingTime) const\n";
    out << "    {\n";
    out << "        return m_encoder.wrapHeader(message, sequenceNumber, sendingTime);\n";
    out << "    }\n\n";

    out << "    template <EncodableMessage Message>\n";
    out << "    uint32_t encode(const Message& message)\n";
    out << "    {\n";
    out << "        return m_encoder.encode(message);\n";
    out << "    }\n\n";

    out << "    [[nodiscard]] uint32_t encodedLength() const\n";
    out << "    {\n";
    out << "        return m_encoder.encodedLength();\n";
    out << "    }\n";

    out << "};\n\n";
    out << "} // namespace org::limitless::simdifx::generated::messages\n\n";
    out << "#endif //SIMD_FIX_MESSAGE_ENCODERS_HPP\n";

    out.close();
}

static std::string uncap(const std::string& value)
{
    std::string result{value};
    if (!value.empty())
    {
        result[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[0])));
    }
    return result;
}

static std::string cap(const std::string& value)
{
    std::string result{value};
    if (!value.empty())
    {
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    }
    return result;
}

/**
 * Emits FixEngine.hpp from config.xml: the engine constants (identity, buffer
 * sizes, timing). Kept free of session includes so the engine headers
 * (Session/PayloadDecoder) can consume the constants without a cycle.
 */
static void generateEngineConfig(const std::string& fileName,
                                 const pugi::xml_document& config,
                                 const Record& protocol)
{
    std::ofstream out(fileName);
    out << "// Generated by Generator from config.xml. Do not edit by hand.\n";
    out << "#ifndef SIMD_FIX_ENGINE_HPP\n";
    out << "#define SIMD_FIX_ENGINE_HPP\n\n";
    out << "#include <chrono>\n";
    out << "#include <cstdint>\n";
    out << "#include <string_view>\n\n";
    out << "#include \"org/limitless/simdifx/Types.hpp\"\n\n";

    out << "namespace org::limitless::simdifx::generated::config {\n\n";
    generateProtocol(out, protocol);
    const auto engine = config.child("config").child("engine");
    out << std::format("inline constexpr FixedString EngineCompId{{\"{}\"}};\n\n",
                       engine.attribute("compId").as_string());
    out << std::format("inline constexpr std::uint32_t MaxMessageSize = {};\n",
                       engine.child("message").attribute("maxSize").as_uint());
    out << std::format("inline constexpr std::uint32_t MaxFields = {};\n",
                       engine.child("payload").attribute("maxFields").as_uint());
    out << std::format("inline constexpr std::chrono::milliseconds KeepAlivePeriod{{{}}};\n",
                       engine.child("keepAlive").attribute("millis").as_uint());
    out << std::format("inline constexpr std::chrono::milliseconds HeartbeatPeriod{{{}}};\n\n",
                       engine.child("heartbeat").attribute("millis").as_uint());

    out << "} // namespace org::limitless::simdifx::generated::config\n\n";
    out << "#endif //SIMD_FIX_ENGINE_HPP\n";
}

/**
 * Emits FixConfig.hpp from config.xml: one session type alias per <session> row
 * (role -> ClientSession/ServerSession, Sender = engine CompID, Target = remote,
 * plus the storage/transport/application policies), a variant over them, and a
 * name -> session registry. Includes FixEngine.hpp so the engine constants are
 * visible here too.
 */
static void generateConfig(const std::string& fileName, const pugi::xml_document& config)
{
    const auto root = config.child("config");
    const std::string compId = root.child("engine").attribute("compId").as_string();

    struct Built { std::string name; std::string alias; std::string storage; std::string heartbeat; };
    std::vector<Built> sessions;
    std::vector<std::string> aliases;
    std::set<std::string> headers;
    bool hasSessionHeaders = false;

    // local is always the engine: Sender = EngineCompId, Target = remote.
    for (const auto session : root.child("sessions").children("session"))
    {
        const std::string name = session.attribute("name").as_string();
        const std::string role = session.attribute("role").as_string();
        const std::string beginString = session.attribute("beginString").as_string();
        const std::string remote = session.attribute("remote").as_string();
        const auto storage = resolveClass(StorageClasses, session.attribute("storage").as_string());
        const auto transport = resolveClass(TransportClasses, session.attribute("transport").as_string());
        const auto application = resolveClass(ApplicationClasses, session.attribute("application").as_string());
        const std::string klass = sessionClass(role);
        const std::string alias = cap(name) + "Session";

        const std::string sessionHeader = session.attribute("header").as_string();
        if (!sessionHeader.empty())
        {
            hasSessionHeaders = true;
        }
        for (const auto& header : {storage.header, transport.header, application.header, sessionHeader})
        {
            if (!header.empty())
            {
                headers.insert(header);
            }
        }

        aliases.push_back(std::format("using {} = {}<{}, \"{}\", \"{}\", {}, {}, {}>;\n",
                                      alias, klass, beginString, compId, remote,
                                      storage.type, transport.type, application.type));
        sessions.push_back({name, alias, storage.type, session.attribute("heartbeat").as_string()});
    }

    std::ofstream out(fileName);
    out << "// Generated by Generator from config.xml. Do not edit by hand.\n";
    out << "#ifndef SIMD_FIX_CONFIG_HPP\n";
    out << "#define SIMD_FIX_CONFIG_HPP\n\n";
    out << "#include \"org/limitless/simdifx/generated/config/FixEngine.hpp\"\n";

    if (!hasSessionHeaders)
    {
        out << "\nnamespace org::limitless::simdifx::generated::config {\n\n";
        out << "} // namespace org::limitless::simdifx::generated::config\n\n";
        out << "#endif //SIMD_FIX_CONFIG_HPP\n";
        return;
    }

    out << "#include <string_view>\n";
    out << "#include <unordered_map>\n";
    out << "#include <variant>\n\n";
    for (const auto& header : headers)
    {
        out << std::format("#include \"{}\"\n", header);
    }
    out << "\n";
    out << "namespace org::limitless::simdifx::generated::config {\n\n";
    out << "using namespace org::limitless::simdifx::session;\n\n";

    for (const auto& alias : aliases)
    {
        out << alias;
    }

    out << "\nusing AnySession = std::variant<";
    for (size_t i = 0; i < sessions.size(); ++i)
    {
        out << sessions[i].alias << (i + 1 < sessions.size() ? ", " : "");
    }
    out << ">;\n\n";

    out << "[[nodiscard]] inline std::unordered_map<std::string_view, AnySession> sessions()\n";
    out << "{\n";
    out << "    std::unordered_map<std::string_view, AnySession> registry;\n";
    for (const auto& s : sessions)
    {
        if (s.heartbeat.empty())
        {
            out << std::format("    registry.emplace(\"{}\", {}{{}});\n", s.name, s.alias);
        }
        else
        {
            out << std::format("    registry.emplace(\"{}\", {}::Builder{{{}{{}}}}"
                               ".heartbeatPeriod(std::chrono::milliseconds{{{}}}).build());\n",
                               s.name, s.alias, s.storage, s.heartbeat);
        }
    }
    out << "    return registry;\n";
    out << "}\n\n";

    out << "} // namespace org::limitless::simdifx::generated::config\n\n";
    out << "#endif //SIMD_FIX_CONFIG_HPP\n";
}

static void generateMessageHandler(const std::string& fileName, const std::vector<Record>& records)
{
    std::ofstream out(fileName);
    if (!out)
    {
        std::println("Error: could not open '{}' for writing", fileName);
        return;
    }

    std::vector<Record> messages{};
    for (const auto& record: records)
    {
        if (!record.m_id.empty())
        {
            messages.push_back(record);
        }
    }
    out << "// Generated by Generator. Do not edit by hand.\n";
    out << "#ifndef SIMD_FIX_MESSAGE_HANDLER_HPP\n";
    out << "#define SIMD_FIX_MESSAGE_HANDLER_HPP\n\n";
    out << "#include \"org/limitless/simdifx/generated/messages/FixMessageDecoders.hpp\"\n\n";
    out << "namespace org::limitless::simdifx::generated::messages {\n\n";
    out << "using simdifx::Result;\n\n";
    out << "template <typename Role, typename Message>\n";
    out << "concept HandlesMessage = requires(Role& role, Message& message)\n";
    out << "{\n";
    out << "    { role.handle(message) } -> std::same_as<Result>;\n";
    out << "};\n\n";
    out << "template <typename MessageHandler>\n";
    out << "class FixMessageHandler\n{\n";
    out << "protected:\n";
    out << "    const SessionContext* m_context{};\n\n";
    out << "public:\n";
    out << "    template <typename Message>\n";
    out << "    Result receive(Message&& message)\n";
    out << "    {\n";
    out << "        using Decoded = std::remove_reference_t<Message>;\n";
    out << "        if constexpr (HandlesMessage<MessageHandler, Decoded>)\n";
    out << "        {\n";
    out << "            return static_cast<MessageHandler*>(this)->handle(std::forward<Message>(message));\n";
    out << "        }\n";
    out << "        else\n";
    out << "        {\n";
    out << "            return Result::Success;\n";
    out << "        }\n";
    out << "    }\n\n";
    out << "    void setSessionContext(const SessionContext& context)\n";
    out << "    {\n";
    out << "        m_context = &context;\n";
    out << "    }\n\n";
    out << "    Result handle(const TokenizedMessage& message)\n";
    out << "    {\n";
    out << "        auto status = Result::InvalidMessageType;\n";
    out << "        switch (message.messageId())\n";
    out << "        {\n";
    for (auto& message: messages)
    {
        auto localName = uncap(message.m_name);
        out << std::format("            case {}Decoder::MessageId:\n", message.m_name);
        out << "            {\n";
        out << std::format("                {}Decoder {}{{m_context, message}};\n", message.m_name, localName);
        out << std::format("                status = {}.validate();\n", localName);
        out << "                if (status == Result::Success)\n";
        out << "                {\n";
        out << std::format("                    status = receive({});\n", localName);
        out << "                }\n";
        out << "                break;\n";
        out << "            }\n";
    }
    out << "            default:\n";
    out << "                break;\n";
    out << "        }\n";
    out << "        return status;\n";
    out << "    }\n";
    out << "};\n\n";
    out << "} // namespace org::limitless::simdifx::generated::messages\n\n";
    out << "#endif //SIMD_FIX_MESSAGE_HANDLER_HPP\n";
}

static void generateConstructors(std::ostream& out, const Record& record, const std::string& codec)
{
    if (!record.m_records.empty())
    {
        out << "public:\n";
    }
    if (record.m_parent == RecordType::Message)
    {
        out << std::format("    {}{}() : \n", record.m_name, codec);
    }
    else
    {
        out << std::format("    explicit {}{}(Field{}& {}) : \n", record.m_name, codec, codec, uncap(codec));
    }
    if (record.m_parent == RecordType::Component || record.m_parent == RecordType::Group)
    {
        out << std::format("        {}{}{{{}}}{}\n",
                           name(record.m_parent), codec, uncap(codec),
                           record.m_records.empty() ? "" : ",");
    }
    if (!record.m_records.empty())
    {
        const auto& back = record.m_records.back();
        for (auto const& field: record.m_records)
        {
            out << std::format("        m_{}{{{}{}}}{}\n",
                               uncap(field.m_name), record.m_parent == RecordType::Message ? "m_" : "",
                               uncap(codec), field.m_name != back.m_name ? "," : "");
        }
    }
    out << "    {\n    }\n\n";

    if (record.m_parent == RecordType::Message && codec == "Decoder")
    {
        out << std::format("    {}{}(const SessionContext* context, const TokenizedMessage& message) :\n", record.m_name, codec);
        out << std::format("        Message{}{{context, message}}{}\n", codec, record.m_records.empty() ? "" : ",");
        if (!record.m_records.empty())
        {
            const auto& back = record.m_records.back();
            for (auto const& field: record.m_records)
            {
                out << std::format("        m_{}{{m_{}}}{}\n",
                                   uncap(field.m_name), uncap(codec),
                                   field.m_name != back.m_name ? "," : "");
            }
        }
        out << "    {\n    }\n\n";
    }

    if (!record.m_records.empty() || record.m_parent == RecordType::Group)
    {
        const auto name = std::format("{}{}", record.m_name, codec);
        out << std::format("    {}(const {}&) = delete;\n", name, name);
        out << std::format("    {}& operator=(const {}&) = delete;\n", name, name);
        out << std::format("    {}({}&&) = delete;\n", name, name);
        out << std::format("    {}& operator=({}&&) = delete;\n\n", name, name);
    }
}

static bool isCacheableField(const org::limitless::generator::simdifx::Field& field)
{
    return field.m_category != Category::Counter &&
           field.m_category != Category::Struct &&
           field.m_tag != 8 && field.m_tag != 9 && field.m_tag != 35;
}

static bool isRequiredCacheableField(const org::limitless::generator::simdifx::Field& field)
{
    return field.m_presence == Presence::Required && isCacheableField(field);
}

static std::string fieldReturnType(const org::limitless::generator::simdifx::Field& field)
{
    const auto isEnum = field.m_category == Category::Enum;
    return isEnum ? field.m_type
                  : std::string{type(field.m_category)};
}

static std::string decoderCall(const org::limitless::generator::simdifx::Field& field, const Record& record)
{
    const auto parent = name(record.m_parent);
    const auto isEnum = field.m_category == Category::Enum;
    const auto mandatory = std::string{field.m_presence == Presence::Required ? "true" : "false"};
    std::string arg = isEnum ? std::format(", {}", field.m_type) : "";
    return std::format("m_decoder.get{}<{}, {}{}, RecordType::{}>()",
                       name(field.m_category), field.m_tag, mandatory, arg, parent);
}

static void generateStructFields(std::ostream& out, const Record& record, const std::string& codec)
{
    bool hasPrivate = !record.m_records.empty();
    if (hasPrivate)
    {
        out << "private:\n";
        for (auto& comp: record.m_records)
        {
            out << std::format("    {}{} m_{};\n\n", comp.m_type, codec, uncap(comp.m_name));
        }
    }

    if (codec == "Decoder" && record.m_parent == RecordType::Message)
    {
        bool first = true;
        for (const auto& field : record.m_fields)
        {
            if (isCacheableField(field))
            {
                if (!hasPrivate && first)
                {
                    out << "private:\n";
                    hasPrivate = true;
                }
                first = false;
                out << std::format("    int8_t m_{}Index{{-1}};\n", uncap(field.m_name));
            }
        }
        if (!first)
        {
            out << "\n";
        }
    }
}

static void generateWrapNextDecoders(std::ostream& out, const Record& record)
{
    if (record.m_parent == RecordType::Group)
    {
        out << std::format("    {}Decoder& wrap()\n", record.m_name);
        out << "    {\n";
        out << std::format("        GroupDecoder::wrap<{}>();\n", record.m_tag);
        out << "        return *this;\n";
        out << "    }\n\n";

        out << std::format("    {}Decoder& next()\n", record.m_name);
        out << "    {\n";
        out << std::format("        GroupDecoder::next();\n", record.m_tag);
        out << "        return *this;\n";
        out << "    }\n\n";
    }
}

static void generateWrapNextEncoders(std::ostream& out, const Record& record)
{
    if (record.m_parent == RecordType::Message)
    {
        out << std::format("    {}Encoder& wrap(const std::span<uint8_t> data, const uint32_t offset = 0)\n", record.m_name);
        out << "    {\n";
        out << "        MessageEncoder::wrap(data, offset);\n";
        out << "        return *this;\n";
        out << "    }\n\n";

        out << "    [[nodiscard]] std::string_view type() const\n";
        out << "    {\n";
        out << "        return MessageId;\n";
        out << "    }\n\n";
    }
    else if (record.m_parent == RecordType::Group)
    {
        out << std::format("    {}Encoder& wrap(const uint32_t count)\n", record.m_name);
        out << "    {\n";
        out << std::format("        GroupEncoder::wrap({}, count);\n", record.m_tag);
        out << "        return *this;\n";
        out << "    }\n\n";

        out << std::format("    {}Encoder& next()\n", record.m_name);
        out << "    {\n";
        out << std::format("        GroupEncoder::next();\n", record.m_tag);
        out << "        return *this;\n";
        out << "    }\n\n";
    }
}

static void generateGetters(std::ostream& out, const Record& record)
{
    for (const auto& field: record.m_fields)
    {
        auto methodName = field.m_name;
        methodName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(methodName[0])));
        if (field.m_category != Category::Counter && field.m_category != Category::Struct)
        {
            out << std::format("    [[nodiscard]] expected<{}, Result> {}() const\n",
                               fieldReturnType(field), methodName);
            out << "    {\n";
            if (record.m_parent == RecordType::Message && isCacheableField(field))
            {
                const auto memberName = uncap(field.m_name);
                const auto optional = field.m_presence != Presence::Required;
                if (optional)
                {
                    out << std::format("        if (m_{}Index < 0)\n", memberName);
                    out << "        {\n";
                    out << "            return unexpected{Result::Success};\n";
                    out << "        }\n";
                }
                if (field.m_category == Category::Enum)
                {
                    out << std::format("        return m_decoder.enumAt<{}>(m_{}Index);\n",
                                       field.m_type, memberName);
                }
                else if (field.m_category == Category::UTCTimeOnly)
                {
                    out << std::format("        return m_decoder.timeOnlyAt(m_{}Index);\n", memberName);
                }
                else if (field.m_category == Category::UTCDateOnly)
                {
                    out << std::format("        return m_decoder.dateOnlyAt(m_{}Index);\n", memberName);
                }
                else
                {
                    out << std::format("        return m_decoder.valueAt<{}>(m_{}Index);\n",
                                       fieldReturnType(field), memberName);
                }
            }
            else
            {
                out << std::format("        return {};\n", decoderCall(field, record));
            }
            out << "    }\n\n";
        }
    }
    for (const auto& comp: record.m_records)
    {
        auto fieldName = uncap(comp.m_name);
        if (comp.m_category == Category::Data)
        {
            out << std::format("    [[nodiscard]] DataDecoder& {}()\n    {{\n", fieldName);
            out << std::format("        return m_{}.wrap<{}, {}>();\n",
                               fieldName, comp.m_length, comp.m_tag);
            out << std::format("    }}\n\n");
        }
        else
        {
            out << std::format("    [[nodiscard]] {}Decoder& {}()\n    {{\n", comp.m_type, fieldName);
            out << std::format("        return m_{}", fieldName);
            if (comp.m_tag != 0)
            {
                out << std::format(".wrap()");
            }
            out << ";\n";
            out << std::format("    }}\n\n");
        }
    }
}

static void generateFieldEncoders(std::ostream& out, const Record& record)
{
    for (const auto& field: record.m_fields)
    {
        if (field.m_tag != 8 && field.m_tag != 9 && field.m_tag != 10 &&
            field.m_tag != 35 && field.m_tag != 49 && field.m_tag != 56 &&
            field.m_category != Category::Counter && field.m_category != Category::Struct)
        {
            const auto isEnum = field.m_category == Category::Enum;
            const auto category = isEnum ?
                                      field.m_type :
                                      std::string{type(field.m_category)};
            out << std::format("    {}Encoder& {}(const {} value)\n",
                               record.m_name, uncap(field.m_name), category);
            out << "    {\n";
            if (isEnum)
            {
                const auto required = field.m_presence == Presence::Required ? "true" : "false";
                out << std::format("        m_encoder.encode<\"{}\", {}, {}>(value);\n",
                                   field.m_tag, required, field.m_type);
            }
            else if (field.m_category == Category::Decimal)
            {
                out << std::format("        m_encoder.encode<\"{}\">(value);\n",
                                   field.m_tag);
            }
            else if (field.m_category == Category::UTCTimeOnly)
            {
                out << std::format("        m_encoder.encodeUTCTimeOnly<\"{}\">(value);\n",
                                   field.m_tag);
            }
            else if (field.m_category == Category::UTCDateOnly)
            {
                out << std::format("        m_encoder.encodeUTCDateOnly<\"{}\">(value);\n",
                                   field.m_tag);
            }
            else
            {
                out << std::format("        m_encoder.encode<\"{}\", {}>(value);\n",
                                   field.m_tag, type(field.m_category));
            }
            out << "        return *this;\n";
            out << "    }\n\n";
        }
    }
    for (const auto& comp: record.m_records)
    {
        const auto fieldName = uncap(comp.m_name);
        if (comp.m_category == Category::Data)
        {
            out << std::format("    {}Encoder& {}(const std::span<const uint8_t> data)\n",
                               record.m_name, fieldName);
            out << "    {\n";
            out << std::format("        m_{}.encode<\"{}\", \"{}\">(data);\n",
                               fieldName, comp.m_length, comp.m_tag);
            out << "        return *this;\n";
            out << "    }\n\n";
        }
        else
        {
            const auto group = comp.m_tag != 0;
            out << std::format("    {}Encoder& {}(", comp.m_type, fieldName);
            if (group)
            {
                out << "const uint32_t count";
            }
            out << ")\n    {\n";
            out << std::format("        return m_{}", fieldName);
            if (group)
            {
                out << ".wrap(count)";
            }
            out << ";\n    }\n\n";
        }
    }
}

static constexpr std::pair<int32_t, std::string_view> HeaderErrorCodes[] = {
    {49, "InvalidSenderCompId"},
    {56, "InvalidTargetCompId"},
    {34, "InvalidSequenceNumber"},
    {52, "InvalidSendingTime"},
};

static std::string_view headerErrorCode(const int32_t tag)
{
    for (const auto& [t, code] : HeaderErrorCodes)
    {
        if (t == tag)
        {
            return code;
        }
    }
    return {};
}

static void generateCheckRequired(std::ostream& out, const Record& record)
{
    if (record.m_parent != RecordType::Message)
    {
        return;
    }

    out << "    [[nodiscard]] Result validate()\n";
    out << "    {\n";

    for (const auto& field : record.m_fields)
    {
        if (!isRequiredCacheableField(field))
        {
            continue;
        }
        const auto memberName = uncap(field.m_name);
        const auto errorCode = headerErrorCode(field.m_tag);
        const auto resultName = errorCode.empty() ? "RequiredFieldMissing" : std::string{errorCode};
        out << std::format("        m_{}Index = static_cast<int8_t>(m_decoder.findIndex<{}, RecordType::Message>());\n",
                           memberName, field.m_tag);
        out << std::format("        if (m_{}Index < 0)\n", memberName);
        out << "        {\n";
        out << std::format("            return Result::{};\n", resultName);
        out << "        }\n";
    }

    for (const auto& field : record.m_fields)
    {
        if (field.m_presence == Presence::Required || !isCacheableField(field))
        {
            continue;
        }
        const auto memberName = uncap(field.m_name);
        out << std::format("        m_{}Index = static_cast<int8_t>(m_decoder.findIndex<{}, RecordType::Message>());\n",
                           memberName, field.m_tag);
    }

    out << "        return validateSession();\n";
    out << "    }\n\n";
}

static void generateRecordDecoders(std::ostream& out, const Record& record)
{
    out << std::format("struct {}Decoder : ", record.m_name);
    out << std::format("{}Decoder\n{{\n", name(record.m_parent));

    generateStructFields(out, record, "Decoder");
    generateConstructors(out, record, "Decoder");
    if (record.m_parent == RecordType::Message)
    {
        if (record.m_id.size() == 1)
        {
            out << std::format("    static constexpr uint16_t MessageId = '{}';\n\n", record.m_id);
        }
        else
        {
            out << std::format("    static constexpr uint16_t MessageId = '{}' | ('{}' << 8);\n\n",
                               record.m_id[0], record.m_id[1]);
        }
    }
    generateWrapNextDecoders(out, record);
    generateCheckRequired(out, record);
    generateGetters(out, record);
    out << "};\n\n";
}

static void generateRecordEncoders(std::ostream& out, const Record& record)
{
    out << std::format("struct {}Encoder : ", record.m_name);
    out << std::format("{}Encoder\n{{\n", name(record.m_parent));

    generateStructFields(out, record, "Encoder");
    generateConstructors(out, record, "Encoder");
    if (record.m_parent == RecordType::Message)
    {
        out << std::format("    static constexpr std::string_view MessageId = \"{}\";\n\n", record.m_id);
    }
    generateWrapNextEncoders(out, record);
    generateFieldEncoders(out, record);
    out << "};\n\n";
}