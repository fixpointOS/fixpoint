#include "runtimes.hh"
#include "handle.hh"
#include "object.hh"
#include "overload.hh"
#include "types.hh"

using namespace std;

shared_ptr<ReadOnlyRT> ReadOnlyRT::init()
{
  auto runtime = std::make_shared<ReadOnlyRT>();
  runtime->repository_.emplace();
  runtime->executor_.emplace( std::thread::hardware_concurrency(), runtime );
  return runtime;
}

shared_ptr<ReadWriteRT> ReadWriteRT::init()
{
  auto runtime = std::make_shared<ReadWriteRT>();
  runtime->repository_.emplace();
  runtime->executor_.emplace( std::thread::hardware_concurrency(), runtime );
  return runtime;
}

template<typename T, typename S>
optional<T> ReadOnlyRT::get( Handle<S> name )
{
  if ( executor_->contains( name ) ) {
    return executor_->get( name );
  } else if ( repository_->contains( name ) ) {
    auto data = repository_->get( name );
    executor_->put( name, *data );
    return *data;
  }

  return executor_->get( name );
}

optional<BlobData> ReadOnlyRT::get( Handle<Named> name )
{
  return get<BlobData, Named>( name );
}
optional<TreeData> ReadOnlyRT::get( Handle<AnyTree> name )
{
  return get<TreeData, AnyTree>( name );
}
optional<Handle<Object>> ReadOnlyRT::get( Handle<Relation> name )
{
  return get<Handle<Object>, Relation>( name );
}

template<typename T, typename S>
void ReadOnlyRT::put( Handle<S> name, T data )
{
  return executor_->put( name, data );
}

void ReadOnlyRT::put( Handle<Named> name, BlobData data )
{
  return put<BlobData, Named>( name, data );
}
void ReadOnlyRT::put( Handle<AnyTree> name, TreeData data )
{
  return put<TreeData, AnyTree>( name, data );
}
void ReadOnlyRT::put( Handle<Relation> name, Handle<Object> data )
{
  return put<Handle<Object>, Relation>( name, data );
}

template<typename T>
bool ReadOnlyRT::contains( T name )
{
  return executor_->contains( name ) || repository_->contains( name );
}

bool ReadOnlyRT::contains( Handle<Named> handle )
{
  return contains<Handle<Named>>( handle );
}
bool ReadOnlyRT::contains( Handle<AnyTree> handle )
{
  return contains<Handle<AnyTree>>( handle );
}
bool ReadOnlyRT::contains( Handle<Relation> handle )
{
  return contains<Handle<Relation>>( handle );
}
bool ReadOnlyRT::contains( const std::string_view label )
{
  return contains<const std::string_view>( label );
}

Handle<Fix> ReadOnlyRT::labeled( const std::string_view label )
{
  if ( repository_->contains( label ) ) {
    return repository_->labeled( label );
  }
  return executor_->labeled( label );
}

Handle<Value> ReadOnlyRT::execute( Handle<Relation> x )
{
  return executor_->execute( x );
}

Handle<Value> ReadWriteRT::execute( Handle<Relation> x )
{
  auto res = executor_->execute( x );

  executor_->visit( x, [this]( Handle<AnyDataType> h ) {
    h.visit<void>(
      overload { []( Handle<Literal> ) {},
                 [&]( auto handle ) { repository_->put( handle, executor_->get( handle ).value() ); } } );
  } );
  return res;
}

shared_ptr<Client> Client::init( const Address& address )
{
  auto runtime = make_shared<Client>();
  runtime->repository_.emplace();
  runtime->network_worker_.emplace( runtime );
  runtime->network_worker_->connect( address );
  runtime->remote_ = runtime->network_worker_->get_remote( address );
  runtime->executor_.emplace( 0, runtime );
  return runtime;
}

shared_ptr<Server> Server::init( const Address& address )
{
  auto runtime = std::make_shared<Server>();
  runtime->repository_.emplace();
  runtime->network_worker_.emplace( runtime );
  runtime->network_worker_->start_server( address );
  runtime->executor_.emplace( std::thread::hardware_concurrency(), runtime );
  return runtime;
}

Handle<Value> Client::execute( Handle<Relation> x )
{
  auto send = [&]( Handle<AnyDataType> h ) {
    h.visit<void>( overload { []( Handle<Literal> ) {},
                              []( Handle<Relation> ) {},
                              [&]( auto x ) { remote_->put( x, executor_->get( x ).value() ); } } );
  };

  x.visit<void>( overload { [&]( Handle<Apply> x ) { executor_->visit_minrepo( x.unwrap<ObjectTree>(), send ); },
                            [&]( Handle<Eval> x ) { executor_->visit( x.unwrap<Object>(), send ); } } );

  remote_->get( x );
  return executor_->execute( x );
}

template<typename T, typename S>
optional<T> Client::get( Handle<S> name )
{
  if ( executor_->contains( name ) )
    return executor_->get( name );

  if ( repository_->contains( name ) ) {
    auto data = repository_->get( name );
    executor_->put( name, *data );
    return *data;
  }

  return remote_->get( name );
}

optional<BlobData> Client::get( Handle<Named> name )
{
  return get<BlobData, Named>( name );
}
optional<TreeData> Client::get( Handle<AnyTree> name )
{
  return get<TreeData, AnyTree>( name );
}
optional<Handle<Object>> Client::get( Handle<Relation> name )
{
  return get<Handle<Object>, Relation>( name );
}

template<typename T, typename S>
void Client::put( Handle<S> name, T data )
{
  executor_->put( name, data );
}

void Client::put( Handle<Named> name, BlobData data )
{
  return put<BlobData, Named>( name, data );
}
void Client::put( Handle<AnyTree> name, TreeData data )
{
  return put<TreeData, AnyTree>( name, data );
}
void Client::put( Handle<Relation> name, Handle<Object> data )
{
  return put<Handle<Object>, Relation>( name, data );
}

template<typename T>
bool Client::contains( T name )
{
  return executor_->contains( name ) || repository_->contains( name ) || remote_->contains( name );
}

bool Client::contains( Handle<Named> handle )
{
  return contains<Handle<Named>>( handle );
}
bool Client::contains( Handle<AnyTree> handle )
{
  return contains<Handle<AnyTree>>( handle );
}
bool Client::contains( Handle<Relation> handle )
{
  return contains<Handle<Relation>>( handle );
}
bool Client::contains( const std::string_view label )
{
  return contains<const std::string_view>( label );
}

Handle<Fix> Client::labeled( const std::string_view label )
{
  if ( repository_->contains( label ) ) {
    return repository_->labeled( label );
  }
  return executor_->labeled( label );
}

template<typename T, typename S>
optional<T> Server::get( Handle<S> name )
{
  if ( executor_->contains( name ) )
    return executor_->get( name );

  if ( repository_->contains( name ) )
    return repository_->get( name );

  return executor_->get( name );
}

optional<BlobData> Server::get( Handle<Named> name )
{
  return get<BlobData, Named>( name );
}
optional<TreeData> Server::get( Handle<AnyTree> name )
{
  return get<TreeData, AnyTree>( name );
}
optional<Handle<Object>> Server::get( Handle<Relation> name )
{
  return get<Handle<Object>, Relation>( name );
}

template<typename T, typename S>
void Server::put( Handle<S> name, T data )
{
  executor_->put( name, data );
  for ( const auto& connection : network_worker_->connections_.read().get() ) {
    connection.second->put( name, data );
  }
}

void Server::put( Handle<Named> name, BlobData data )
{
  return put<BlobData, Named>( name, data );
}
void Server::put( Handle<AnyTree> name, TreeData data )
{
  return put<TreeData, AnyTree>( name, data );
}
void Server::put( Handle<Relation> name, Handle<Object> data )
{
  return put<Handle<Object>, Relation>( name, data );
}

template<typename T>
bool Server::contains( T name )
{
  return executor_->contains( name ) || repository_->contains( name );
}

bool Server::contains( Handle<Named> handle )
{
  return contains<Handle<Named>>( handle );
}
bool Server::contains( Handle<AnyTree> handle )
{
  return contains<Handle<AnyTree>>( handle );
}
bool Server::contains( Handle<Relation> handle )
{
  return contains<Handle<Relation>>( handle );
}
bool Server::contains( const std::string_view label )
{
  return contains<const std::string_view>( label );
}

Handle<Fix> Server::labeled( const std::string_view label )
{
  if ( repository_->contains( label ) ) {
    return repository_->labeled( label );
  }
  return executor_->labeled( label );
}
