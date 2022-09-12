#pragma once

#include <pl/codegens/codegen.hpp>
#include <pl/codegens/codegen_kaitai.hpp>

namespace pl::gen::code {

    // Available code generators. Add new ones here to make them available
    using CodeGenerators = std::tuple<
            CodegenKaitai
    >;



    template<size_t N = 0>
    auto createCodeGenerators(std::array<std::unique_ptr<pl::gen::code::Codegen>, std::tuple_size_v<CodeGenerators>> &&result = {}) {
        auto formatter = std::unique_ptr<pl::gen::code::Codegen>(new typename std::tuple_element<N, CodeGenerators>::type());

        result[N] = std::move(formatter);

        if constexpr (N + 1 < std::tuple_size_v<CodeGenerators>) {
            return createFormatters<N + 1>(std::move(result));
        } else {
            return result;
        }
    }

}