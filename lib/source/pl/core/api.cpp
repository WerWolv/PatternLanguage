#include <pl/api.hpp>

namespace pl::api {

    u32 Source::idCounter;

    Section::IOError Section::read(u64 address, std::span<u8> into) const {
        ChunkReader reader = [=](std::span<const u8> chunk) mutable {
            std::memcpy(into.data(), chunk.data(), chunk.size());
            
            into = into.subspan(chunk.size());
            return std::nullopt; // Continue
        };
        return read(address, into.size(), reader);
    }

    Section::IOError Section::read(u64 fromAddress, size_t size, ChunkReader& reader) const {
        const auto fail = [=](const std::string& reason) {
            u64 from = fromAddress;
            u128 to = fromAddress + size;
            
            return fmt::format("Read 0x{:X}-0x{:X} (of {} bytes) failed: {}", from, to, to - from, reason);
        };

        auto maxSize = std::numeric_limits<decltype(fromAddress)>::max() - fromAddress;
        if (maxSize < size) {
            return fail(fmt::format("Attempted to read {} bytes past the address space.", size - maxSize));
        }
        
        auto end = fromAddress + size;
        if (this->size() < end) {
            return fail(fmt::format("Attempted to read {} bytes past section end.", end - this->size()));
        }
        
        auto readError = readRaw(fromAddress, size, reader);
        if (readError) {
            return fail(*readError);
        }
        return std::nullopt;
    }

    Section::IOError Section::write(bool expand, u64 address, std::span<const u8> from) {
        ChunkWriter writer = [=](std::span<u8> chunk) mutable {
            std::memcpy(chunk.data(), from.data(), chunk.size());
            
            from = from.subspan(chunk.size());
            return std::nullopt; // Continue
        };
        return write(expand, address, from.size(), writer);
    }

    Section::IOError Section::write(bool expand, u64 toAddress, size_t size, ChunkWriter& writer) {
        const auto fail = [=](const std::string& reason) {
            u64 from = toAddress;
            u128 to = toAddress + size;
            
            return fmt::format("Write 0x{:X}-0x{:X} (of {} bytes) failed: {}", from, to, to - from, reason);
        };
        
        auto maxSize = std::numeric_limits<decltype(toAddress)>::max() - toAddress;
        if (maxSize < size) {
            return fail(fmt::format("Attempted to write {} bytes past the address space.", size - maxSize));
        }
        
        auto end = toAddress + size;
        if (this->size() <= end) {
            if (!expand) {
                return fail(fmt::format("Attempted to write {} bytes past the section end. Expansion was not allowed", end - this->size()));
            }
            
            if (auto error = resize(end)) {
                return fail(fmt::format("Unable to allocate required storage. {}", *error));
            }
        }
        
        auto writeError = writeRaw(toAddress, size, writer);
        if (writeError) {
            return fail(*writeError);
        }
        return std::nullopt;
    }

    Section::IOError Section::write(bool expand, u64 address, size_t size, u64 fromAddress, api::Section& fromSection) {
        IOError result;
        
        u64 readFront = fromAddress;
        std::span<u8> writableChunk;
        
        ChunkReader reader = [&](std::span<const u8> chunk) {
            std::memcpy(writableChunk.data(), chunk.data(), chunk.size());
            
            readFront += chunk.size();
            return std::nullopt; // Continue
        };
        ChunkWriter writer = [&](std::span<u8> chunk) {
            writableChunk = chunk;
            return fromSection.read(readFront, writableChunk.size(), reader);
        };
        
        return write(expand, address, size, writer);
    }

}
