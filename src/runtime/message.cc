#if 0
#include "message.hh"
#include "base64.hh"
#include "object.hh"
#include "parser.hh"
#include "runtime.hh"

using namespace std;

IncomingMessage::IncomingMessage( const Message::Opcode opcode, string&& payload )
  : Message( opcode )
  , payload_( std::move( payload ) )
{}

IncomingMessage::IncomingMessage( const Message::Opcode opcode, OwnedMutBlob&& payload )
  : Message( opcode )
  , payload_( std::move( payload ) )
{}

IncomingMessage::IncomingMessage( const Message::Opcode opcode, OwnedMutTree&& payload )
  : Message( opcode )
  , payload_( std::move( payload ) )
{}

OutgoingMessage::OutgoingMessage( const Message::Opcode opcode, Blob payload )
  : Message( opcode )
  , payload_( payload )
{}

OutgoingMessage::OutgoingMessage( const Message::Opcode opcode, Tree payload )
  : Message( opcode )
  , payload_( payload )
{}

OutgoingMessage::OutgoingMessage( const Message::Opcode opcode, string&& payload )
  : Message( opcode )
  , payload_( payload )
{}

void OutgoingMessage::serialize_header( string& out )
{
  out.resize( Message::HEADER_LENGTH );
  Serializer s( string_span::from_view( out ) );
  s.integer( payload_length() );
  s.integer( static_cast<uint8_t>( opcode() ) );

  if ( s.bytes_written() != Message::HEADER_LENGTH ) {
    throw runtime_error( "Wrong header length" );
  }
}

string_view OutgoingMessage::payload()
{
  return std::visit(
    [&]( auto& arg ) -> string_view {
      return { reinterpret_cast<const char*>( arg.data() ), payload_length() };
    },
    payload_ );
}

size_t OutgoingMessage::payload_length()
{
  if ( std::holds_alternative<Tree>( payload_ ) ) {
    return get<Tree>( payload_ ).size() * sizeof( Tree::element_type );
  } else {
    return std::visit( []( auto& arg ) -> size_t { return arg.size(); }, payload_ );
  }
}

Message::Opcode Message::opcode( string_view header )
{
  assert( header.size() == HEADER_LENGTH );
  return static_cast<Opcode>( header[8] );
}

size_t IncomingMessage::expected_payload_length( string_view header )
{
  Parser p { header };
  size_t payload_length = 0;
  p.integer( payload_length );

  if ( p.error() ) {
    throw runtime_error( "Unable to parse header" );
  }

  return payload_length;
}

OutgoingMessage OutgoingMessage::to_message( MessagePayload&& payload )
{
  return std::visit(
    []( auto&& p ) -> OutgoingMessage {
      using T = std::decay_t<decltype( p )>;
      return { T::OPCODE, serialize( p ) };
    },
    payload );
}

void MessageParser::complete_message()
{
  std::visit(
    [&]( auto&& arg ) { completed_messages_.emplace( Message::opcode( incomplete_header_ ), std::move( arg ) ); },
    incomplete_payload_ );

  expected_payload_length_.reset();
  incomplete_header_.clear();
  incomplete_payload_ = "";
  completed_payload_length_ = 0;
}

size_t MessageParser::parse( string_view buf )
{
  size_t consumed_bytes = buf.length();

  while ( not buf.empty() ) {
    if ( not expected_payload_length_.has_value() ) {
      const auto remaining_length = min( buf.length(), Message::HEADER_LENGTH - incomplete_header_.length() );
      incomplete_header_.append( buf.substr( 0, remaining_length ) );
      buf.remove_prefix( remaining_length );

      if ( incomplete_header_.length() == Message::HEADER_LENGTH ) {
        expected_payload_length_ = IncomingMessage::expected_payload_length( incomplete_header_ );

        if ( expected_payload_length_.value() == 0 ) {
          assert( Message::opcode( incomplete_header_ ) == Message::Opcode::REQUESTINFO );
          complete_message();
        } else {
          switch ( Message::opcode( incomplete_header_ ) ) {
            case Message::Opcode::RUN:
            case Message::Opcode::INFO:
            case Message::Opcode::RESULT:
            case Message::Opcode::PROPOSE_TRANSFER:
            case Message::Opcode::ACCEPT_TRANSFER: {
              incomplete_payload_ = "";
              get<string>( incomplete_payload_ ).resize( expected_payload_length_.value() );
              break;
            }

            case Message::Opcode::BLOBDATA: {
              incomplete_payload_ = OwnedMutBlob::allocate( expected_payload_length_.value() );
              break;
            }

            case Message::Opcode::TREEDATA: {
              incomplete_payload_ = OwnedMutTree::allocate( expected_payload_length_.value() / sizeof( Handle ) );
              break;
            }

            default:
              throw runtime_error( "Invalid combination of message type and payload size." );
          }
        }
      }
    } else {
      std::visit(
        [&]( auto& arg ) {
          char* data = const_cast<char*>( reinterpret_cast<const char*>( arg.data() ) ) + completed_payload_length_;
          const auto remaining_length
            = min( buf.length(), expected_payload_length_.value() - completed_payload_length_ );
          memcpy( data, buf.data(), remaining_length );

          buf.remove_prefix( remaining_length );
          completed_payload_length_ += remaining_length;

          if ( completed_payload_length_ == expected_payload_length_.value() ) {
            complete_message();
          }
        },
        incomplete_payload_ );
    }
  }

  return consumed_bytes;
}

Handle parse_handle( Parser& parser )
{
  string handle;
  handle.resize( 43 );
  parser.string( string_span::from_view( handle ) );

  if ( parser.error() ) {
    throw runtime_error( "Failed to parse handle." );
  }

  return base64::decode( handle );
}

Operation parse_operation( Parser& parser )
{
  uint8_t operation;
  parser.integer( operation );

  if ( parser.error() ) {
    throw runtime_error( "Failed to parse opeartion." );
  }

  return static_cast<Operation>( operation );
}

RunPayload RunPayload::parse( Parser& parser )
{
  return { .task { parse_handle( parser ), parse_operation( parser ) } };
}

void RunPayload::serialize( Serializer& serializer ) const
{
  serializer.string( base64::encode( task.handle() ) );
  serializer.integer( static_cast<uint8_t>( task.operation() ) );
}

ResultPayload ResultPayload::parse( Parser& parser )
{
  return { .task { parse_handle( parser ), parse_operation( parser ) }, .result { parse_handle( parser ) } };
}

void ResultPayload::serialize( Serializer& serializer ) const
{
  serializer.string( base64::encode( task.handle() ) );
  serializer.integer( static_cast<uint8_t>( task.operation() ) );
  serializer.string( base64::encode( result ) );
}

InfoPayload InfoPayload::parse( Parser& parser )
{
  uint32_t parallelism;
  parser.integer( parallelism );

  if ( parser.error() ) {
    throw runtime_error( "Failed to parse opeartion." );
  }

  return { { .parallelism = parallelism } };
}

void InfoPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( parallelism );
}

template<Message::Opcode O>
TransferPayload<O> TransferPayload<O>::parse( Parser& parser )
{
  Handle handle = parse_handle( parser );
  Operation operation = parse_operation( parser );
  TransferPayload payload { .todo = Task( handle, operation ) };
  bool has_result = false;
  parser.integer<bool>( has_result );
  if ( has_result )
    payload.result = parse_handle( parser );
  size_t count = 0;
  parser.integer<size_t>( count );
  payload.handles.reserve( count );
  for ( size_t i = 0; i < count; i++ ) {
    payload.handles.push_back( parse_handle( parser ) );
  }
  return payload;
}

template<Message::Opcode O>
void TransferPayload<O>::serialize( Serializer& serializer ) const
{
  serializer.string( base64::encode( todo.handle() ) );
  serializer.integer<uint8_t>( static_cast<uint8_t>( todo.operation() ) );
  serializer.integer<bool>( result.has_value() );
  if ( result.has_value() ) {
    serializer.string( base64::encode( *result ) );
  }
  serializer.integer<size_t>( handles.size() );
  for ( const auto& h : handles ) {
    serializer.string( base64::encode( h ) );
  }
}

template<Message::Opcode O>
using TxP = TransferPayload<O>;

static constexpr Message::Opcode ACCEPT = Message::Opcode::ACCEPT_TRANSFER;
static constexpr Message::Opcode PROPOSE = Message::Opcode::PROPOSE_TRANSFER;

template TxP<PROPOSE> TxP<PROPOSE>::parse( Parser& parser );
template void TxP<PROPOSE>::serialize( Serializer& serializer ) const;
template TxP<ACCEPT> TxP<ACCEPT>::parse( Parser& parser );
template void TxP<ACCEPT>::serialize( Serializer& serializer ) const;
#endif
