#include "sha256.hh"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#include "PicoSHA2/picosha2.h"
#include "timer.hh"

using namespace std;

string sha256::encode( std::span<const char> input )
{
  GlobalScopeTimer<Timer::Category::Hash> record_timer;
  string digest( picosha2::k_digest_size, 0 );
  picosha2::hash256( input, digest );
  return digest;
}
