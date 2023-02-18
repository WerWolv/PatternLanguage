#pragma once

#include <span>
#include <vector>
#include <pl.hpp>

#include <pl/helpers/types.hpp>

namespace pl::hlp::bf {

    constexpr static inline unsigned long long operator""_Bytes(unsigned long long bytes) noexcept {
        return bytes;
    }

    constexpr static inline unsigned long long operator""_KiB(unsigned long long kiB) noexcept {
        return operator""_Bytes(kiB * 1024);
    }

    constexpr static inline unsigned long long operator""_MiB(unsigned long long MiB) noexcept {
        return operator""_KiB(MiB * 1024);
    }

    constexpr static inline unsigned long long operator""_GiB(unsigned long long GiB) noexcept {
        return operator""_MiB(GiB * 1024);
    }

    class BufferedReader {
    public:
        explicit BufferedReader(core::Evaluator* ctx, size_t bufferSize = 16_MiB, u64 sectionId = ptrn::Pattern::MainSectionId)
                : m_provider(ctx), m_bufferAddress(ctx->getDataBaseAddress()), m_section(sectionId),
                  m_maxBufferSize(bufferSize),
                  m_endAddress(ctx->getDataBaseAddress() + ctx->getDataSize() - 1LLU), m_buffer(bufferSize) {

        }

        void seek(u64 address) {
            this->m_bufferAddress += address;
        }

        void setEndAddress(u64 address) {
            if (address >= this->m_provider->getDataSize())
                address = this->m_provider->getDataSize() - 1;

            this->m_endAddress = address;
        }

        [[nodiscard]] std::vector<u8> read(u64 address, size_t size) {
            if (size > this->m_buffer.size()) {
                std::vector<u8> result;
                result.resize(size);

                this->m_provider->readData(address, result.data(), result.size(), this->m_section);

                return result;
            }

            this->updateBuffer(address, size);

            auto result = &this->m_buffer[address -  this->m_bufferAddress];

            return { result, result + std::min(size, this->m_buffer.size()) };
        }

    private:
        void updateBuffer(u64 address, size_t size) {
            if (address > this->m_endAddress)
                return;

            if (!this->m_bufferValid || address < this->m_bufferAddress || address + size > (this->m_bufferAddress + this->m_buffer.size())) {
                const auto remainingBytes = (this->m_endAddress - address) + 1;
                if (remainingBytes < this->m_maxBufferSize)
                    this->m_buffer.resize(remainingBytes);
                else
                    this->m_buffer.resize(this->m_maxBufferSize);

                this->m_provider->readData(address, this->m_buffer.data(), this->m_buffer.size(), this->m_section);
                this->m_bufferAddress = address;
                this->m_bufferValid = true;
            }
        }

    private:
        core::Evaluator *m_provider;

        u64 m_bufferAddress = 0x00;
        u64 m_section;
        size_t m_maxBufferSize;
        bool m_bufferValid = false;
        u64 m_endAddress;
        std::vector<u8> m_buffer;
    };
}