#include "stream_reassembler.hh"

#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>

#define MAX_EOF std::numeric_limits<size_t>::max()

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

/* static uint64_t _time = 0; */
/* static uint64_t _output_count = 0; */
/* auto end = std::chrono::high_resolution_clock::now(); */
/* auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start); */

/* _time += duration.count(); */
/* if (_output_count % 10000000 == 0) */
/*     std::cerr << _output_count << " time: " << _time << "\n"; */
/* ++_output_count; */

using namespace std;

StreamReassembler::ByteString &StreamReassembler::ByteString::operator=(const ByteString &other) {
    this->index = other.index;
    this->str = std::string(other.str);
    return *this;
}

// Coalesces ByteStrings a and b if their indices overlap. Returns true if able to coalesce and
// stores the resulting ByteString in res. Return false if unable to colesce and does not alter res.
bool StreamReassembler::coalesce(const ByteString a, const ByteString b, ByteString &res) {
    size_t a_index_right = a.index + a.str.size();
    size_t b_index_right = b.index + b.str.size();

    // a is superset of b
    if (a.index <= b.index && a_index_right >= b_index_right) {
        res = a;
        return true;
    }

    // b is superset of a
    else if (b.index <= a.index && b_index_right >= a_index_right) {
        res = b;
        return true;
    }

    // a is left of b
    else if (a.index < b.index && a_index_right >= b.index) {
        res.index = a.index;
        res.str = a.str + b.str.substr(b_index_right - a_index_right, string::npos);
        return true;
    }

    // b is left of a
    else if (b.index < a.index && b_index_right >= a.index) {
        res.index = b.index;
        res.str = b.str + a.str.substr(a_index_right - b_index_right, string::npos);
        return true;
    }

    // can't coalesce
    return false;
}

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

void StreamReassembler::write_to_output(const ByteString b) {
    size_t bytes_written = _output.write(b.str);
    _index += bytes_written;
    if (_index == _eof && _eof_set)
        _output.end_input();
}

// Clean the current state of the Reassembler.
// 1. Erase bytes that have already been written
// 2. Flush bytes to output, if possible
void StreamReassembler::clean() {
    // TODO: probably don't need to do a lot of things here
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
    ByteString b{index, data};

    if (index < _index) {
        size_t begin_index = _index - index;
        // Don't write to indices before the lastest written index.
        if (begin_index < data.size()) {
            b.str = b.str.substr(begin_index, string::npos);
            b.index += begin_index;
        }
        // No new indices to be written
        else
            return;
    }

    // Don't write bytes that would extend beyond capacity.
    size_t right_bound = _index + _capacity - _output.buffer_size() - b.index;
    b.str = b.str.substr(0, right_bound);

    // Insert bytes into _unassembled_bytes list sorted
    size_t b_index_right = b.index + b.str.size();
    while (iter != end) {
        ByteString res;
        if (b_index_right < iter->index) {
            _unassembled_bytes.insert(iter, b);
            return;
        }

        // Attempt to coalesce
        else if (coalesce(b, *iter, res)) {
            // Attempt to colaesce with the next substring. This can happen if a substring
            // complete fills the hole in between existing substrings.
            auto next = iter;
            ++next;
            if (next != end && coalesce(res, *next, res))
                _unassembled_bytes.erase(next);

            *iter = res;
            return;
        }
        ++iter;
    }
    _unassembled_bytes.push_back(b);
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

size_t StreamReassembler::unassembled_bytes() const {
    size_t s = 0;
    for (ByteString b : _unassembled_bytes)
        s += b.str.size();
    return s;
}

bool StreamReassembler::empty() const { return _unassembled_bytes.empty(); }
