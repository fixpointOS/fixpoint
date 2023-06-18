#include <string_view>
#include <utility>

#include "base64.hh"
#include "handle.hh"
#include "job.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

#ifndef COMPILE
#define COMPILE "COMPILE MUST BE DEFINED"
#endif

#define TRUSTED_COMPILE Handle( base64::decode( COMPILE ) )

void RuntimeWorker::queue_job( Job job )
{
  runtimestorage_.work_++;
  jobs_.push( std::move( job ) );
  runtimestorage_.work_.notify_all();
}

bool RuntimeWorker::dequeue_job( Job& job )
{
  // Try to pop off the local queue, steal work if that fails, return false if no work can be found
  bool work = jobs_.pop( job );

  if ( !work )
    work = runtimestorage_.steal_work( job, thread_id_ );

  if ( work )
    runtimestorage_.work_--;

  return work;
}

void RuntimeWorker::eval( Handle hash, Handle name )
{
  switch ( name.get_content_type() ) {
    case ContentType::Blob: {
      progress( hash, name );
      break;
    }

    case ContentType::Tree: {
      if ( !name.is_strict() ) {
        progress( hash, name );
        break;
      }
      auto orig_tree = runtimestorage_.get_tree( name );
      Handle fill_operation( name, true, { FILL } );
      Handle fill_desired( name, false, { FILL } );

      runtimestorage_.fix_cache_.pending_start( fill_operation, name, orig_tree.size() );

      std::shared_ptr<std::atomic<int64_t>> pending = runtimestorage_.fix_cache_.get_pending( fill_operation );
      bool to_fill = false;
      int64_t post_sub = orig_tree.size();

      for ( size_t i = 0; i < orig_tree.size(); ++i ) {
        auto entry = orig_tree[i];

        if ( ( entry.is_strict() && !entry.is_blob() ) || ( entry.is_shallow() && entry.is_thunk() ) ) {
          to_fill = true;

          Handle desired( entry, false, { EVAL } );
          Handle operations( entry, true, { EVAL } );

          if ( runtimestorage_.fix_cache_.start_after( desired, fill_operation ) ) {
            queue_job( Job( entry, operations ) );
          } else {
            post_sub = pending->fetch_sub( 1 ) - 1;
          }
        } else {
          post_sub = pending->fetch_sub( 1 ) - 1;
        }
      }

      if ( !to_fill ) {
        progress( hash, name );
        return;
      }

      auto value_if_completed = runtimestorage_.fix_cache_.continue_after( fill_desired, hash );
      if ( value_if_completed.has_value() ) {
        progress( hash, value_if_completed.value() );
      } else {
        if ( post_sub == 0 ) {
          progress( fill_operation, name );
        }
      }
      break;
    }

    case ContentType::Thunk: {
      if ( name.is_lazy() ) {
        progress( hash, name );
        return;
      }
      Handle encode_name = Handle::get_encode_name( name );

      if ( name.is_strict() ) {
        Handle encode_desired( encode_name, false, { EVAL, APPLY } );
        Handle encode_operations( encode_name, true, { APPLY, EVAL } );

        Handle operations( name, true, { EVAL } );
        runtimestorage_.fix_cache_.continue_after( encode_desired, operations );
        progress( encode_operations, encode_name );
      } else {
        Handle encode_desired( encode_name, false, { EVAL, APPLY, MAKE_SHALLOW } );
        Handle encode_operations( encode_name, true, { MAKE_SHALLOW, APPLY, EVAL } );

        Handle operations( name, true, { EVAL } );
        runtimestorage_.fix_cache_.continue_after( encode_desired, operations );
        progress( encode_operations, encode_name );
      }

      break;
    }

    case ContentType::Tag: {
      if ( !name.is_strict() ) {
        progress( hash, name );
        return;
      }
      auto orig_tag = runtimestorage_.get_tree( name );
      auto first_entry = orig_tag[0];

      if ( !first_entry.is_blob() ) {
        Handle fill_operation( name, true, { FILL } );
        Handle fill_desired( name, false, { FILL } );

        runtimestorage_.fix_cache_.pending_start( fill_operation, name, 1 );
        std::shared_ptr<std::atomic<int64_t>> pending = runtimestorage_.fix_cache_.get_pending( fill_operation );

        Handle desired( first_entry, false, { EVAL } );
        Handle operations( first_entry, true, { EVAL } );

        runtimestorage_.fix_cache_.continue_after( fill_desired, hash );
        if ( runtimestorage_.fix_cache_.start_after( desired, fill_operation ) ) {
          queue_job( Job( first_entry, operations ) );
        } else {
          pending->fetch_sub( 1 );
          progress( fill_operation, name );
        }
      } else {
        progress( hash, name );
      }
      return;
    }

    default: {
      throw std::runtime_error( "Invalid content type." );
    }
  }
}

void RuntimeWorker::apply( Handle hash, Handle name )
{
  Handle function_tag = runtimestorage_.get_tree( name ).at( 1 );
  while ( !runtimestorage_.get_tree( function_tag ).at( 1 ).is_blob() ) {
    function_tag = runtimestorage_.get_tree( function_tag ).at( 1 );
  }

  auto tag = runtimestorage_.get_tree( function_tag );
  if ( tag.at( 1 ) != TRUSTED_COMPILE || tag.at( 2 ) != Handle( "Runnable" ) ) {
    throw std::runtime_error( "Procedure is not generated by trusted compilation toolchain" );
  }

  Handle function_name = tag.at( 0 );
  Handle canonical_name = runtimestorage_.local_to_storage( function_name );
  if ( not runtimestorage_.name_to_program_.contains( canonical_name ) ) {
    /* Link program */
    Program program = link_program( runtimestorage_.get_blob( function_name ) );
    runtimestorage_.name_to_program_.put( canonical_name, std::move( program ) );
  }

  auto& program = runtimestorage_.name_to_program_.getMutable( canonical_name );
  runtimestorage_.set_current_procedure( canonical_name );
  __m256i output = program.execute( name );
  // Compute next operation
  progress( hash, output );
}

void RuntimeWorker::update_parent( Handle name )
{
  span_view<Handle> tree = runtimestorage_.get_tree( name );

  for ( size_t i = 0; i < tree.size(); ++i ) {
    auto entry = tree[i];
    if ( entry.is_strict() && !entry.is_blob() ) {
      Handle desired( entry, false, { EVAL } );
      tree.mutable_data()[i] = runtimestorage_.fix_cache_.get_name( desired );
    }
  }
}

void RuntimeWorker::fill( Handle hash, Handle name )
{
  switch ( name.get_content_type() ) {
    case ContentType::Tree: {
      auto orig_tree = runtimestorage_.get_tree( name );
      Handle new_name = runtimestorage_.add_tree( Tree( orig_tree.size() ) );
      span_view<Handle> tree = runtimestorage_.get_tree( new_name );

      for ( size_t i = 0; i < tree.size(); ++i ) {
        auto entry = orig_tree[i];
        tree.mutable_data()[i] = entry;

        if ( entry.is_strict() && !entry.is_blob() ) {
          Handle desired( entry, false, { EVAL } );
          tree.mutable_data()[i] = runtimestorage_.fix_cache_.get_name( desired );
        }
      }

      progress( hash, new_name );
      return;
    }

    case ContentType::Tag: {
      auto orig_tag = runtimestorage_.get_tree( name );
      Handle new_name = runtimestorage_.add_tag( Tree( orig_tag.size() ) );
      span_view<Handle> tag = runtimestorage_.get_tree( new_name );

      auto first_entry = orig_tag[0];
      Handle desired( first_entry, false, { EVAL } );
      tag.mutable_data()[0] = runtimestorage_.fix_cache_.get_name( desired );
      tag.mutable_data()[1] = orig_tag[1];
      tag.mutable_data()[2] = orig_tag[2];

      progress( hash, new_name );
      return;
    }

    default:
      throw std::runtime_error( "Invalid job type." );
  }
}

void RuntimeWorker::launch_jobs( std::queue<std::pair<Handle, Handle>> ready_jobs )
{
  while ( !ready_jobs.empty() ) {
    queue_job( Job( ready_jobs.front().first, ready_jobs.front().second ) );
    ready_jobs.pop();
  }
}

void RuntimeWorker::progress( Handle hash, Handle name )
{
  uint32_t current = hash.peek_operation();

  if ( current != NONE ) {
    if ( current == MAKE_SHALLOW ) {
      name = Handle::make_shallow( name );
      current = hash.peek_operation();
      hash.pop_operation();
    }

    hash.pop_operation();
    Handle nhash( name, false, { current } );
    int status = runtimestorage_.fix_cache_.try_run( nhash, hash );
    if ( status == 1 ) {
      switch ( current ) {
        case APPLY:
          apply( nhash, name );
          break;
        case EVAL:
          eval( nhash, name );
          break;
        case FILL:
          fill( nhash, name );
          break;
        default:
          throw std::runtime_error( "Invalid job type." );
      }
    } else if ( status == -1 ) {
      progress( hash, runtimestorage_.fix_cache_.get_name( nhash ) );
    }
  } else {
    launch_jobs( runtimestorage_.fix_cache_.complete( hash, name ) );
  }
}

void RuntimeWorker::work()
{
  // Wait till all threads have been primed before computation can begin
  runtimestorage_.threads_started_.wait( false );

  Job job;
  while ( runtimestorage_.threads_active_ ) {
    if ( dequeue_job( job ) ) {
      progress( job.hash, job.name );
    } else {
      runtimestorage_.work_.wait( 0 );
    }
  }
}
