#include "stream_reassembler.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t StreamReassembler::remaining_capacity() { return _capacity - unassembled_bytes() - _output.buffer_size(); }

void StreamReassembler::set_eof(const size_t index) {
    _eof = index;
    _eof_set = true;

    if (_eof == _index)
        _output.end_input();

    // Remove unassembled bytes with index after eof
    auto begin = _unassembled_bytes.rbegin();
    auto iter = _unassembled_bytes.rend();
    while (iter != begin && iter->index > _eof) {
        _unassembled_bytes.erase(++iter.base());
    }
}

void StreamReassembler::write_to_output(const Byte b) {
    _output.write(std::string(1, b.val));
    if (b.index == _eof && _eof_set)
        _output.end_input();
    ++_index;
}

// Clean the current state of the Reassembler.
// 1. Erase bytes that have already been written
// 2. Strip bytes from end of _unassembled_bytes if over the capacity
// 3. Flush bytes to output, if possible
//
// MUST be done in this order.
// 1. Must come before 2. to free up _unassembled_bytes capacity
// 2. Must come before 3. to prevent writing bytes that are over capacity
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

    // Strip bytes if over the capacity
    int bytes_over_capacity = _output.buffer_size() + unassembled_bytes() - _capacity;
    assert(unassembled_bytes() > bytes_over_capacity);
    while (bytes_over_capacity > 0) {
        _unassembled_bytes.pop_back();
        --bytes_over_capacity;
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
    : _output(capacity), _capacity(capacity), _bytes_written(0), _index(0), _eof(MAX_EOF), _eof_set(false) {
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t size = data.size();
    size_t last_index = index;
    if (size != 0)
        last_index += size - 1;

    if (eof)
        set_eof(last_index);

    if (last_index < _index || data.size() == 0)
        return;

    push_unassembled_bytes(data, index);
    clean();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes.size(); }

bool StreamReassembler::empty() const { return _unassembled_bytes.empty(); }
