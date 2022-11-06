#pragma once

#include <pl/pattern_language.hpp>
#include <pl/pattern_visitor.hpp>

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_wide_string.hpp>

#include <string>
#include <utility>

namespace pl::gen::fmt {

    class FormatterPatternVisitor : public pl::PatternVisitor {
    public:
        void enableMetaInformation(bool enable) { this->m_metaInformation = enable; }
        [[nodiscard]] bool isMetaInformationEnabled() const { return this->m_metaInformation; }

        std::vector<std::pair<std::string, std::string>> getMetaInformation(ptrn::Pattern *pattern) const {
            if (!this->m_metaInformation)
                return { };

            std::vector<std::pair<std::string, std::string>> result;

            result.emplace_back("__type", ::fmt::format("{}", pattern->getTypeName()));
            result.emplace_back("__address", ::fmt::format("{}", pattern->getOffset()));
            result.emplace_back("__size", ::fmt::format("{}", pattern->getSize()));
            result.emplace_back("__color", ::fmt::format("#{:08X}", pattern->getColor()));
            result.emplace_back("__endian", ::fmt::format("{}", pattern->getEndian() == std::endian::little ? "little" : "big"));
            if (const auto &comment = pattern->getComment(); !comment.empty())
                result.emplace_back("__comment", comment);

            return result;
        }

    private:
        bool m_metaInformation = false;
    };

    class Formatter {
    public:
        explicit Formatter(std::string name) : m_name(std::move(name)) { }
        virtual ~Formatter() = default;

        [[nodiscard]] const std::string &getName() const {
            return this->m_name;
        }

        [[nodiscard]] virtual std::string getFileExtension() const = 0;
        [[nodiscard]] virtual std::vector<u8> format(const PatternLanguage &runtime) = 0;

        void enableMetaInformation(bool enable) { this->m_metaInformation = enable; }
        [[nodiscard]] bool isMetaInformationEnabled() const { return this->m_metaInformation; }

    private:
        std::string m_name;
        bool m_metaInformation = false;
    };

}