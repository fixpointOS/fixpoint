#pragma once
#include "elfloader.hh"
#include "fixpointapi.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "mutex.hh"
#include "object.hh"
#include "overload.hh"
#include "program.hh"
#include "resource_limits.hh"
#include "runtimestorage.hh"
#include "types.hh"

#include <absl/container/flat_hash_map.h>
#include <glog/logging.h>

class Runner
{
public:
  virtual void init() {};
  virtual Handle<Object> apply( Handle<ObjectTree> handle, TreeData combination ) = 0;
  virtual ~Runner() {}
};

/**
 * The standard Fixpoint Runner, which links and loads ELF files generated by wasm2c.
 */
class WasmRunner : public Runner
{
public:
  virtual void init() override {}
  virtual ~WasmRunner() {}

  WasmRunner( Handle<Fix> trusted_compiler )
    : trusted_compiler_( trusted_compiler )
  {
    wasm_rt_init();
  }

  virtual Handle<Object> apply( Handle<ObjectTree> handle, TreeData combination ) override
  {
    // Check minrepo are all loaded
    if ( not fixpoint::storage->complete( handle ) ) {
      throw std::runtime_error( "Incomplete minrepo." );
    }

    std::optional<Handle<AnyTree>> function_tag {};

    auto rlimits = combination->at( 0 );
    while ( true ) {
      function_tag = handle::extract<ObjectTree>( combination->at( 1 ) )
                       .transform( []( auto h ) -> Handle<AnyTree> { return h; } )
                       .or_else( [&]() -> std::optional<Handle<AnyTree>> {
                         return handle::extract<ValueTree>( combination->at( 1 ) );
                       } );

      if ( not function_tag.has_value() ) {
        throw std::runtime_error( "Function is not an object/value tree." );
      }

      combination = fixpoint::storage->get( function_tag.value() );
      auto next_level = combination->at( 1 );

      if ( handle::extract<Blob>( next_level ).has_value() )
        break;
    }

    if ( not function_tag->visit<bool>( []( auto h ) { return h.is_tag(); } ) ) {
      throw std::runtime_error( "Procedure is not a tag." );
    }

    auto tag = fixpoint::storage->get( function_tag.value() );

    if ( tag->at( 0 ) != trusted_compiler_ ) {
      throw std::runtime_error( "Procedure is not generated by trusted compilation toolchain" );
    }

    if ( tag->at( 2 ) != Handle<Literal>( "Runnable" ).into<Fix>() ) {
      std::cerr << "Attempted to run non-Runnable procedure:" << std::endl;
      std::cerr << "- object: " << tag->at( 1 ) << std::endl;
      std::cerr << "- author: " << tag->at( 2 ) << std::endl;
      // std::cerr << "- type: " << std::string_view( storage_.get( tag->at( 2 ) ) ) << endl;
      Handle<Fix> handle = tag->at( 1 );
      auto data
        = handle::extract<Named>( handle ).transform( [&]( auto h ) { return fixpoint::storage->get( h ); } );
      auto literal = handle::extract<Literal>( handle );

      std::string_view view;
      if ( data.has_value() ) {
        view = { data.value()->data(), data.value()->size() };
      } else {
        view = literal.value().view();
      }

      bool is_printable
        = std::count_if( view.begin(), view.end(), []( unsigned char c ) { return std::isprint( c ); } );
      if ( is_printable == view.size() ) {
        std::cerr << "--- ERROR ---" << std::endl;
        std::cerr << view << std::endl;
        std::cerr << "-------------" << std::endl;
      } else {
        std::cerr << "Object is not printable." << std::endl;
      }
      throw std::runtime_error( "Procedure is not runnable" );
    }

    auto function_name = handle::extract<Blob>( tag->at( 1 ) ).value();

    bool program_linked = programs_.read()->contains( function_name );
    if ( !program_linked ) {
      auto program = function_name.visit<std::shared_ptr<Program>>(
        overload { [&]( Handle<Literal> f ) { return link_program( f.view() ); },
                   [&]( Handle<Named> f ) { return link_program( fixpoint::storage->get( f )->span() ); } } );
      programs_.write()->emplace( function_name, program );
    }

    auto program = programs_.read()->at( function_name );
    fixpoint::current_procedure = function_name;

    VLOG( 2 ) << handle << " rlimits are " << rlimits;
    // invalid resource limits are interpreted as 0
    auto limits = rlimits.try_into<Expression>()
                    .and_then( [&]( auto x ) { return x.template try_into<Object>(); } )
                    .and_then( [&]( auto x ) { return x.template try_into<Value>(); } )
                    .and_then( [&]( auto x ) { return x.template try_into<ValueTree>(); } )
                    .transform( [&]( auto x ) { return fixpoint::storage->get( x ); } );

    resource_limits::available_bytes
      = limits.and_then( [&]( auto x ) { return handle::extract<Literal>( x->at( 0 ) ); } )
          .transform( [&]( auto x ) { return uint64_t( x ); } )
          .value_or( 0 );

    VLOG( 1 ) << handle << " requested " << resource_limits::available_bytes << " bytes";
    auto result = program->execute( handle );
    VLOG( 2 ) << handle << " -> " << result;
    return result;
  }

private:
  SharedMutex<absl::flat_hash_map<Handle<Blob>, std::shared_ptr<Program>>> programs_ {};
  Handle<Fix> trusted_compiler_;
};

/**
 * For testing and development purposes: a Runner which interprets the first element of a combination as a function
 * pointer and directly jumps to it.
 */
class PointerRunner : public Runner
{

public:
  virtual void init() override {}
  virtual ~PointerRunner() {}

  virtual Handle<Object> apply( Handle<ObjectTree> handle, TreeData combination ) override
  {
    auto procedure = combination->at( 0 )
                       .try_into<Expression>()
                       .and_then( &Handle<Expression>::try_into<Object> )
                       .and_then( &Handle<Object>::try_into<Value> )
                       .and_then( &Handle<Value>::try_into<Blob> )
                       .and_then( &Handle<Blob>::try_into<Literal> )
                       .value();
    uint64_t addr( procedure );
    auto x = reinterpret_cast<Handle<Object> ( * )( Handle<ObjectTree> )>( addr );
    return x( handle );
  }
};
