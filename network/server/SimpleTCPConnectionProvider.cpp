/***************************************************************************
 *
 * Project         _____    __   ____   _      _
 *                (  _  )  /__\ (_  _)_| |_  _| |_
 *                 )(_)(  /(__)\  )( (_   _)(_   _)
 *                (_____)(__)(__)(__)  |_|    |_|
 *
 *
 * Copyright 2018-present, Leonid Stryzhevskyi, <lganzzzo@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#include "./SimpleTCPConnectionProvider.hpp"

#include "oatpp/network/Connection.hpp"

#include "oatpp/core/utils/ConversionUtils.hpp"
#include "oatpp/test/Checker.hpp"

#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

namespace oatpp { namespace network { namespace server {

SimpleTCPConnectionProvider::SimpleTCPConnectionProvider(v_word16 port, bool nonBlocking)
  : m_port(port)
  , m_nonBlocking(nonBlocking)
{
  m_serverHandle = instantiateServer();
  setProperty(PROPERTY_HOST, "localhost");
  setProperty(PROPERTY_PORT, oatpp::utils::conversion::int32ToStr(port));
}
  
oatpp::os::io::Library::v_handle SimpleTCPConnectionProvider::instantiateServer(){
  
  oatpp::os::io::Library::v_handle serverHandle;
  v_int32 ret;
  int yes = 1;
  
  struct sockaddr_in6 addr;
  
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(m_port);
  addr.sin6_addr = in6addr_any;
  
  serverHandle = socket(AF_INET6, SOCK_STREAM, 0);
  
  if(serverHandle < 0){
    return -1;
  }
  
  ret = setsockopt(serverHandle, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  if(ret < 0) {
    OATPP_LOGD("SimpleTCPConnectionProvider", "Warning failed to set %s for accepting socket", "SO_REUSEADDR");
  }
  
  ret = bind(serverHandle, (struct sockaddr *)&addr, sizeof(addr));
  
  if(ret != 0) {
    oatpp::os::io::Library::handle_close(serverHandle);
    throw std::runtime_error("Can't bind to address");
    return -1 ;
  }
  
  ret = listen(serverHandle, 10000);
  if(ret < 0) {
    oatpp::os::io::Library::handle_close(serverHandle);
    return -1 ;
  }
  
  fcntl(serverHandle, F_SETFL, 0);//O_NONBLOCK);
  
  return serverHandle;
  
}
  
std::shared_ptr<oatpp::data::stream::IOStream> SimpleTCPConnectionProvider::getConnection(){
  
  //oatpp::test::PerformanceChecker checker("Accept Checker");
  
  oatpp::os::io::Library::v_handle handle = accept(m_serverHandle, nullptr, nullptr);
  
  if (handle < 0) {
    v_int32 error = errno;
    if(error == EAGAIN || error == EWOULDBLOCK){
      return nullptr;
    } else {
      OATPP_LOGD("Server", "Error: %d", error);
      return nullptr;
    }
  }
  
#ifdef SO_NOSIGPIPE
  int yes = 1;
  v_int32 ret = setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(int));
  if(ret < 0) {
    OATPP_LOGD("SimpleTCPConnectionProvider", "Warning failed to set %s for socket", "SO_NOSIGPIPE");
  }
#endif
  
  int flags = 0;
  if(m_nonBlocking) {
    flags |= O_NONBLOCK;
  }
  
  fcntl(handle, F_SETFL, flags);
  
  return Connection::createShared(handle);
  
}

}}}
