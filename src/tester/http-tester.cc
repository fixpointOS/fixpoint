#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#include "base64.hh"
#include "eventloop.hh"
#include "http_server.hh"
#include "operation.hh"
#include "runtimestorage.hh"
#include "socket.hh"
#include "tester-utils.hh"

using namespace std;
using namespace boost::property_tree;

static constexpr size_t mebi = 1024 * 1024;

Handle parse_handle( string id )
{
  string delimiter = "|";
  size_t pos;
  vector<uint64_t> numbers;
  for ( size_t i = 0; i < 4; i++ ) {
    pos = id.find( delimiter );
    if ( pos == string::npos ) {
      numbers.push_back( stoull( id, nullptr, 16 ) );
      break;
    }
    numbers.push_back( stoull( id.substr( 0, pos ), nullptr, 16 ) );
    id = id.substr( pos + 1 );
  }
  if ( numbers.size() != 4 ) {
    cerr << "Received invalid handle";
    throw runtime_error( "Parsing invalid handle" );
  }
  return Handle( numbers[0], numbers[1], numbers[2], numbers[3] );
}

void kick_off( span_view<char*> args )
{
  vector<ReadOnlyFile> open_files;

  // make the combination from the given arguments
  Handle encode_name = parse_args( args, open_files );

  if ( not args.empty() ) {
    throw runtime_error( "unexpected argument: "s + args.at( 0 ) );
  }

  // add the combination to the store, and print it
  cout << "Combination:\n" << pretty_print( encode_name ) << "\n";

  auto& runtime = RuntimeStorage::get_instance();

  // make a Thunk that points to the combination
  Handle thunk_name = runtime.add_thunk( Thunk { encode_name } );

  runtime.eval_thunk_nonblocking( thunk_name );

  cout << "Thunk name: " << base64::encode( thunk_name ) << endl;

  cout << flush;
}

ptree list_dependees( Handle handle )
{
  ptree pt;
  for ( auto& dependee : RuntimeStorage::get_instance().get_dependees( Task( handle, Operation::Eval ) ) ) {
    ptree dependee_tree;
    dependee_tree.push_back( ptree::value_type( "handle", base64::encode( dependee.handle() ) ) );
    dependee_tree.put( "operation", uint8_t( dependee.operation() ) );
    pt.push_back( ptree::value_type( "", dependee_tree ) );
  }

  ptree data;
  data.push_back( ptree::value_type( "data", pt ) );
  return data;
}

ptree get_value( Handle handle, Operation operation )
{
  ptree pt;
  optional<Handle> result = RuntimeStorage::get_instance().get_status( Task( handle, operation ) );
  if ( result.has_value() ) {
    pt.push_back( ptree::value_type( "data", base64::encode( result.value() ) ) );
  } else {
    pt.push_back( ptree::value_type( "data", "" ) );
  }

  return pt;
}

// gets one definition if it exists. expects a=b&c=d or a=b
optional<pair<string, string>> find_definition( string& url,
                                                const char query_separator,
                                                const char definition_separator )
{
  if ( url.length() == 0 ) {
    return {};
  }
  size_t end_pos = url.find( query_separator );
  string definition = "";
  if ( end_pos == string::npos ) {
    end_pos = url.length();
    definition = url;
    url.erase();
  } else {
    definition = url.substr( 0, end_pos );
    url.erase( 0, end_pos + 1 );
  }

  size_t split_pos = definition.find( definition_separator );
  if ( split_pos == string::npos ) {
    throw runtime_error( "invalid url param, expected definition_separator" );
  }
  return { { definition.substr( 0, split_pos ), definition.substr( split_pos + 1 ) } };
}

// get the parameters from the url into a map.
// requires a url of the form blahblablah?a=b&d=91
unordered_map<string, string> decode_url_params( string url )
{
  size_t pos = url.find( "?" );
  if ( pos == string::npos ) {
    return {};
  }
  url.erase( 0, pos + 1 );
  unordered_map<string, string> result = {};
  auto definition = find_definition( url, '&', '=' );
  while ( definition.has_value() ) {
    result.insert( definition.value() );
    definition = find_definition( url, '&', '=' );
  }

  return result;
}

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );

  TCPSocket server_socket {};
  server_socket.set_reuseaddr();
  server_socket.bind( { "0", args.at( 1 ) } );
  server_socket.listen();

  args.remove_prefix( 2 );

  cerr << "Listening on port " << server_socket.local_address().port() << "\n";

  TCPSocket connection = server_socket.accept();
  cerr << "Connection received from " << connection.peer_address().to_string() << "\n";

  EventLoop events;

  RingBuffer client_buffer { mebi }, server_buffer { mebi };

  HTTPServer http_server;
  HTTPRequest request_in_progress;

  events.add_rule(
    "Read bytes from client",
    connection,
    Direction::In,
    [&] { client_buffer.push_from_fd( connection ); },
    [&] { return not client_buffer.writable_region().empty(); } );

  events.add_rule(
    "Parse bytes from client buffer",
    [&] {
      if ( http_server.read( client_buffer, request_in_progress ) ) {
        cerr << "Got request: " << request_in_progress.request_target << "\n";
        std::string target = request_in_progress.request_target;
        string response = "Hello, user.";

        auto map = decode_url_params( target );
        if ( target == "/start" ) {
          kick_off( args );
          response = "done";
        } else if ( target.starts_with( "/list" ) ) {
          stringstream ptree;
          write_json( ptree, list_dependees( base64::decode( map.at( "handle" ) ) ) );
          response = ptree.str();
        } else if ( target.starts_with( "/get" ) ) {
          stringstream ptree;
          write_json( ptree,
                      get_value( base64::decode( map.at( "handle" ) ), Operation( stoul( map.at( "op" ) ) ) ) );
          response = ptree.str();
        }
        HTTPResponse the_response;
        the_response.http_version = "HTTP/1.1";
        the_response.status_code = "200";
        the_response.reason_phrase = "OK";
        the_response.headers.content_type = "text/html";
        the_response.headers.content_length = response.size();
        the_response.body = response;
        http_server.push_response( move( the_response ) );
      }
    },
    [&] { return not client_buffer.readable_region().empty(); } );

  events.add_rule(
    "Serialize bytes from server into server buffer",
    [&] { http_server.write( server_buffer ); },
    [&] { return not http_server.responses_empty() and not server_buffer.writable_region().empty(); } );

  events.add_rule(
    "Write bytes to client",
    connection,
    Direction::Out,
    [&] { server_buffer.pop_to_fd( connection ); },
    [&] { return not server_buffer.readable_region().empty(); } );

  while ( events.wait_next_event( -1 ) != EventLoop::Result::Exit ) {}

  cerr << "Exiting...\n";
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " PORT entry...\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base64-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
  cerr << "            | compile:<filename>\n";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  try {
    if ( argc <= 2 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    span_view<char*> args = { argv, static_cast<size_t>( argc ) };
    program_body( args );
  } catch ( const exception& e ) {
    cerr << argv[0] << ": " << e.what() << "\n\n";
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
