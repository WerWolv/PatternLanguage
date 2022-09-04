#pragma once

#include <pl/core/errors/evaluator_errors.hpp>

namespace pl::lib::libstd::types {

    struct Endian {
        Endian(u128 value) {
            switch (value) {
                case 0: this->m_endian = std::endian::native;   break;
                case 1: this->m_endian = std::endian::big;      break;
                case 2: this->m_endian = std::endian::little;   break;
                default:
                    core::err::E0012.throwError("Invalid endian value.", "Try one of the values in the std::core::Endian enum.");
            }
        }

        operator std::endian() const {
            return this->m_endian;
        }

    private:
        std::endian m_endian;
    };

}