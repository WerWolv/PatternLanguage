#pragma once

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

namespace pl::fmt {

    class Formatter {
    public:
        explicit Formatter(std::string name) : m_name(std::move(name)) { }
        virtual ~Formatter() = default;

        [[nodiscard]] const std::string &getName() const {
            return this->m_name;
        }

        [[nodiscard]] virtual std::string getFileExtension() const = 0;
        [[nodiscard]] virtual std::vector<u8> format(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) = 0;

    private:
        std::string m_name;
    };

}