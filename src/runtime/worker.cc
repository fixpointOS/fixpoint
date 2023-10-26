#include <string_view>
#include <utility>

#include "base64.hh"
#include "handle.hh"
#include "operation.hh"
#include "runtime.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "task.hh"

#include "wasm-rt-content.h"

using namespace std;

#ifndef COMPILE
#define COMPILE "COMPILE MUST BE DEFINED"
#endif

optional<Handle> RuntimeWorker::await( Task target, Task current )
{
  return graph_.start_after( target, current );
}

bool RuntimeWorker::await_tree( Task task )
{
  Tree tree = storage_.get_tree( task.handle() );
  return graph_.start_after( tree, task );
}

std::optional<Handle> RuntimeWorker::do_eval( Task task )
{
  Handle handle = task.handle();
  if ( handle.is_lazy() ) {
    return handle;
  }
  optional<Handle> result;

  switch ( handle.get_content_type() ) {
    case ContentType::Blob:
      return handle;

    case ContentType::Tree:
      if ( !await_tree( task ) )
        return {};

      return await( Task::Fill( handle ), task );

    case ContentType::Tag: {
      return await( Task::Eval( handle.as_tree() ), task );
    }

    case ContentType::Thunk:
      Handle encode = handle.as_tree();

      result = await( Task::Eval( encode ), task );
      if ( not result )
        return {};

      result = await( Task::Apply( result.value() ), task );
      if ( not result )
        return {};

      if ( handle.is_shallow() )
        result = Handle::make_shallow( result.value() );

      return await( Task::Eval( result.value() ), task );
  }

  throw std::runtime_error( "unhandled case in eval" );
}

Handle RuntimeWorker::do_apply( Task task )
{
  auto name = task.handle();
  if ( not( name.is_strict() and name.is_tree() ) ) {
    if ( name.is_thunk() ) {
      throw std::runtime_error( "Attempted to apply a Thunk, not an Encode." );
    }
    throw std::runtime_error( "Attempted to apply something besides a strict tree." );
  }

  Tree current = storage_.get_tree( name );
  Handle function_tag;
  while ( true ) {
    assert( current.is_tag() or current.is_tree() );
    assert( current.size() >= 2 );
    function_tag = current[1];
    if ( function_tag.is_blob() ) {
      assert( current.is_tag() );
      break;
    }
    current = storage_.get_tree( function_tag );
  }

  auto tag = storage_.get_tree( function_tag );
  assert( tag.is_tag() );
  assert( tag.size() == 3 );

  const static Handle COMPILE_ELF = storage_.get_ref( "compile-elf" ).value();
  if ( tag[1] != COMPILE_ELF ) {
    throw std::runtime_error( "Procedure is not generated by trusted compilation toolchain: generated by "
                              + storage_.get_display_name( tag[1] ) );
  }
  if ( tag[2] != Handle( "Runnable" ) ) {
    cerr << "Attempted to run non-Runnable procedure:" << endl;
    cerr << "- object: " << tag[0] << endl;
    cerr << "- author: " << tag[1] << endl;
    auto type = tag[2].literal_blob();
    cerr << "- type: " << string_view( type.data(), type.size() ) << endl;
    Handle handle = tag[0];
    auto data = storage_.get_blob( handle );
    bool is_printable
      = std::count_if( data.begin(), data.end(), []( unsigned char c ) { return std::isprint( c ); } );
    if ( is_printable == data.size() ) {
      cerr << "--- ERROR ---" << endl;
      cerr << string_view( data.data(), data.size() ) << endl;
      cerr << "-------------" << endl;
    } else {
      cerr << "Object is not printable." << endl;
    }
    throw std::runtime_error( "Procedure is not runnable" );
  }

  Handle function_name = tag[0];
  Handle canonical_name = runtime_.storage().canonicalize( function_name );

  Blob function = storage_.get_blob( canonical_name );

  using Lambda = Handle ( * )( Runtime*, Handle );
  Lambda f = (Lambda)function.data();

  runtime_.set_current_procedure( canonical_name );
  return f( std::addressof( runtime_ ), name );
}

Handle RuntimeWorker::do_fill( Handle name )
{
  switch ( name.get_content_type() ) {
    case ContentType::Tree: {
      auto orig_tree = storage_.get_tree( name );
      auto [tree, new_name] = storage_.create_tree( orig_tree.size() );

      for ( size_t i = 0; i < tree.size(); ++i ) {
        auto entry = orig_tree[i];
        tree[i] = entry;

        if ( entry.is_strict() and !entry.is_blob() ) {
          tree[i] = graph_.get( Task::Eval( entry ) ).value();
        }
      }

      return new_name;
    }

    default:
      throw std::runtime_error( "Invalid content type for fill." );
  }
}

optional<Handle> RuntimeWorker::progress( Task task )
{
  switch ( task.operation() ) {
    case Operation::Apply:
      return do_apply( task );
    case Operation::Eval:
      return do_eval( task );
    case Operation::Fill:
      return do_fill( task.handle() );
  }
  throw std::runtime_error( "invalid operation for progress" );
}

void RuntimeWorker::work()
{
  current_thread_id_ = thread_id_;

  try {
    while ( true ) {
      auto task = runq_.pop_or_wait();
      auto result = progress( task );
      if ( result )
        graph_.finish( std::move( task ), *result );
    }
  } catch ( ChannelClosed& ) {
    return;
  }
}
