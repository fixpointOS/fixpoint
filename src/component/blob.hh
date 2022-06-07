#pragma once

#include <cstdlib>
#include <memory>
#include <string_view>

class Blob
{
private:
  char* data_;
  uint32_t size_;
  bool own_;

public:
  Blob( char* data, const size_t size, const bool own = true )
    : data_( std::move( data ) )
    , size_( size )
    , own_( own )
  {
  }

  Blob( uint8_t* data, const size_t size, const bool own = true )
    : data_( std::move( reinterpret_cast<char*>( data ) ) )
    , size_( size )
    , own_( own )
  {
  }

  Blob( std::unique_ptr<char> data, const size_t size )
    : Blob( data.release(), size, true )
  {
  }

  Blob( std::unique_ptr<uint8_t> data, const size_t size )
    : Blob( data.release(), size, true )
  {
  }

  Blob( std::string_view str )
    : Blob( const_cast<char*>( str.data() ), str.size(), false )
  {
  }

  Blob( Blob&& other )
    : Blob( other.data_, other.size_, true )
  {
    if ( this != &other ) {
      other.own_ = false;
    }
  }

  Blob& operator=( Blob&& other )
  {
    data_ = other.data_;
    size_ = other.size_;
    own_ = true;

    if ( this != &other ) {
      other.own_ = false;
    }

    return *this;
  }

  Blob( const Blob& ) = delete;
  Blob& operator=( const Blob& ) = delete;

  operator std::string_view() const { return { data_, size_ }; }
  const char* data() const { return data_; }
  size_t size() const { return size_; }

  ~Blob()
  {
    if ( own_ ) {
      free( data_ );
    };
  }
};

template<typename T>
Blob make_blob( const T& t )
{
  char* t_storage = static_cast<char*>( malloc( sizeof( T ) ) );
  if ( not t_storage ) {
    throw std::bad_alloc();
  }
  memcpy( t_storage, &t, sizeof( T ) );
  return { t_storage, sizeof( T ) };
}
