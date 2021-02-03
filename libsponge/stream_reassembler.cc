#include "stream_reassembler.hh"

#include <cassert>
#include <limits>

#define MAX_EOF std::numeric_limits<size_t>::max()

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t StreamReassembler::remaining_capacity() { return _capacity - unassembled_bytes() - _output.buffer_size(); }

void StreamReassembler::set_eof(const size_t eof) {
    _eof = eof;
    _eof_set = true;

    // EOF already met
    if (_eof <= _index + 1)
        _output.end_input();

    // Remove unassembled bytes with index after eof
    auto begin = _unassembled_bytes.rbegin();
    auto iter = _unassembled_bytes.rend();
    while (iter != begin && iter->index >= _eof) {
        _unassembled_bytes.erase(++iter.base());
    }
}

void StreamReassembler::write_to_output(const Byte b) {
    _output.write(std::string(1, b.val));
    if (b.index + 1 == _eof && _eof_set)
        _output.end_input();
    ++_index;
}

// Clean the current state of the Reassembler.
// 1. Erase bytes that have already been written
// 2. Flush bytes to output, if possible
void StreamReassembler::clean() {
    if (empty())
        return;

    // Strip written bytes
    auto iter = _unassembled_bytes.begin();
    auto end = _unassembled_bytes.end();
    while (iter != end && iter->index < _index) {
        auto temp_iter = iter;
        ++iter;
        _unassembled_bytes.erase(temp_iter);
    }

    // Flush bytes to output, if possible
    iter = _unassembled_bytes.begin();
    end = _unassembled_bytes.end();
    while (iter != end && iter->index == _index) {
        write_to_output(*iter);
        auto temp_iter = iter;
        ++iter;
        _unassembled_bytes.erase(temp_iter);
    }
}

void StreamReassembler::push_unassembled_bytes(const std::string &data, const uint64_t index) {
    auto iter = _unassembled_bytes.begin();
    auto end = _unassembled_bytes.end();
    size_t data_index = index;
    size_t data_end = data_index + data.size();

    // Don't write bytes that would extend beyond the capacity.
    size_t right_bound = _index + _capacity - _output.buffer_size();
    if (data_end > right_bound)
        data_end = right_bound;

    // Insert bytes into _unassembled_bytes list sorted
    while (data_index < data_end) {
        // Append the byte or insert in sorted order
        if (iter == end || data_index < iter->index) {
            Byte b;
            b.index = data_index;
            b.val = data[data_index - index];
            _unassembled_bytes.insert(iter, b);
            ++data_index;
        }

        // Ignore the byte if it already exists
        else if (data_index == iter->index)
            ++data_index;

        // Otherwise advance through _unassembled_bytes
        else
            ++iter;
    }
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _index(0), _eof(MAX_EOF), _eof_set(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t size = data.size();
    size_t last_index = index + size;

    if (eof)
        set_eof(last_index);

    // Ignore segments that are entirely outside of window or empty.
    if (last_index < _index || size == 0)
        return;

    push_unassembled_bytes(data, index);
    clean();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes.size(); }

bool StreamReassembler::empty() const { return _unassembled_bytes.empty(); }
