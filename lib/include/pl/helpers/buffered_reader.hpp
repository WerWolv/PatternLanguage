#pragma once

#include <span>
#include <vector>

#include <pl/core/evaluator.hpp>

#include <wolv/io/buffered_reader.hpp>

namespace pl::hlp {

    struct ReaderData {
        core::Evaluator *evaluator;
        u64 sectionId;
    };

    inline void evaluatorReaderFunction(ReaderData *data, void *buffer, u64 address, size_t size) {
        data->evaluator->readData(address, buffer, size, data->sectionId);
    }

    class MemoryReader : public wolv::io::BufferedReader<ReaderData, evaluatorReaderFunction> {
    public:
        using BufferedReader::BufferedReader;

        MemoryReader(core::Evaluator *evaluator, u64 sectionId, size_t bufferSize = 0x100000) : BufferedReader(&this->m_readerData, evaluator->getDataSize(), bufferSize) {
            this->m_readerData.evaluator = evaluator;
            this->m_readerData.sectionId = sectionId;
        }

    private:
        ReaderData m_readerData;
    };

}