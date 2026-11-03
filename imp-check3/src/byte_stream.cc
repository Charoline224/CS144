#include "byte_stream.hh"
#include "debug.hh"
#include <iostream>
using namespace std;

ByteStream::ByteStream( uint64_t capacity )
   : capacity_( capacity ),
   buffer_(capacity),
   error_ (false),
   closed_ (false),
   totalbytes(0),
   totalread(0)
   {}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  // Your code here (and in each method below)
  int num = available_capacity();
  int len = size(data);
  buffer_.push(data.substr(0, min(num, len)));
  totalbytes +=  min(num, len);
  return;
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  closed_ = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  return closed_; // Your code here.
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{
  return buffer_.leftspace();
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{
  return totalbytes; // Your code here.
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  return buffer_.getview(); 
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  buffer_.pop(len);
  totalread += len;
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  return closed_ && buffer_.is_empty(); // Your code here.
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  return buffer_.occupiedspace(); // Your code here.
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  return totalread; // Your code here.
}
