#include "byte_stream.hh"

#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_buffer.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// Computes the number of bytes to actually read given len. Makes sure to not
// read more bytes than the current length of the buffer.
size_t ByteStream::bytes_to_read(const size_t len) const {
    size_t bytes_to_read = len;
    size_t buffer_size = this->buffer_size();

    if (bytes_to_read > buffer_size)
        bytes_to_read = buffer_size;

    return bytes_to_read;
}

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {
    this->_capacity = capacity;
    _buffer.reserve(capacity);
}

size_t ByteStream::write(const string &data) {
    size_t bytes_to_write = data.size();
    size_t remaining_capacity = this->remaining_capacity();

    if (bytes_to_write > remaining_capacity)
        bytes_to_write = remaining_capacity;

    _buffer.append(data.substr(0, bytes_to_write));

    _bytes_written += bytes_to_write;
    return bytes_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    const size_t bytes_to_peek = len;
    return _buffer.substr(0, this->bytes_to_read(bytes_to_peek));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    const size_t bytes_to_pop = len;
    _bytes_read += bytes_to_pop;
    _buffer = _buffer.substr(this->bytes_to_read(bytes_to_pop), _buffer.substr().size());
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const size_t bytes_to_read = this->bytes_to_read(len);

    std::string s = _buffer.substr(0, bytes_to_read);
    _buffer = _buffer.substr(bytes_to_read, _buffer.substr().size());

    _bytes_read += bytes_to_read;
    return s;
}

void ByteStream::end_input() { _eof = true; }

bool ByteStream::input_ended() const { return _eof; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _eof && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
