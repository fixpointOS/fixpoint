#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>
#include <string.h>

void out( const char* s )
{
  fixpoint_unsafe_io( s, strlen( s ) );
}

__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )

{
  externref nil = create_blob_rw_mem_0( 0 );

  attach_tree_ro_table_0( combination );

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0( 1 ); */
  externref input = get_ro_table_0( 2 );

  attach_blob_ro_mem_0( input );
  size_t size = get_length( input );
  char* file = (char*)malloc( size );
  ro_mem_0_to_program_mem( file, 0, size );

  uint64_t counts[256];
  memset( counts, 0, sizeof( counts ) );

  for ( size_t i = 0; i < size; i++ ) {
    counts[file[i]]++;
  }

  if ( grow_rw_mem_0_pages( 1 ) == -1 ) {
    out( "count words: grow error" );
  }
  program_mem_to_rw_mem_0( 0, counts, sizeof( counts ) );

  return create_blob_rw_mem_0( sizeof( counts ) );
}
