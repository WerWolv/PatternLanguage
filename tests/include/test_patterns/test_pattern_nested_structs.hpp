#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>

#include <vector>

namespace pl::test {

    class TestPatternNestedStructs : public TestPattern {
    public:
        TestPatternNestedStructs() : TestPattern("NestedStructs") {
            constexpr static size_t HeaderStart = 0x0;
            constexpr static size_t HeaderSize = sizeof(u8);
            constexpr static size_t BodyStart = HeaderSize;
            constexpr static size_t BodySize = 0x89 - 1;

            auto data = create<PatternStruct>("Data", "data", HeaderStart, HeaderSize + BodySize, 0);
            {
                auto hdr = create<PatternStruct>("Header", "hdr", HeaderStart, HeaderSize, 0);
                {
                    std::vector<std::shared_ptr<Pattern>> hdrMembers {
                        std::shared_ptr(create<PatternUnsigned>("u8", "len", HeaderStart, sizeof(u8), 0))
                    };
                    hdr->setMembers(std::move(hdrMembers));
                }

                auto body = create<PatternStruct>("Body", "body", BodyStart, BodySize, 0);
                {
                    auto bodyArray = create<PatternArrayStatic>("u8", "arr", BodyStart, BodySize, 0);
                    bodyArray->setEntries(create<PatternUnsigned>("u8", "", BodyStart, sizeof(u8), 0), BodySize);
                    std::vector<std::shared_ptr<Pattern>> bodyMembers {
                        std::shared_ptr(std::move(bodyArray))
                    };
                    body->setMembers(std::move(bodyMembers));
                }

                std::vector<std::shared_ptr<Pattern>> dataMembers {
                    std::shared_ptr(std::move(hdr)),
                    std::shared_ptr(std::move(body))
                };
                data->setMembers(std::move(dataMembers));
            }

            addPattern(std::move(data));
        }
        ~TestPatternNestedStructs() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                fn end_of_body() {
                    u32 start = addressof(parent.parent.hdr);
                    u32 len = parent.parent.hdr.len;
                    u32 end = start + len;

                    return $ >= end;
                };

                struct Header {
                    u8 len;
                };

                struct Body {
                    u8 arr[while(!end_of_body())];
                };

                struct Data {
                    Header hdr;
                    Body body;
                };

                Data data @ 0x0;

                std::assert(data.hdr.len == 0x89, "Invalid length");
                std::assert(sizeof(data.body.arr) == 0x89 - 1, "Invalid size of body");
            )";
        }
    };

}