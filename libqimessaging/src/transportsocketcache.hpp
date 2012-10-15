#pragma once
/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#ifndef  TRANSPORT_CACHE_HPP_
# define TRANSPORT_CACHE_HPP_

#include <qimessaging/transportsocket.hpp>
#include <qitype/future.hpp>
#include <boost/thread/mutex.hpp>
#include <string>

namespace qi {

  struct TransportSocketConnection {
    qi::TransportSocketPtr              socket;
    qi::Promise<qi::TransportSocketPtr> promise;
    int                                 attempts;
    int                                 success;
    unsigned int                        connectLink;
    unsigned int                        disconnectLink;
  };

  /**
   * @brief The TransportCache class maintain a cache of TransportSocket
   * @internal
   *
   * getSocket will return a connected endpoint for the associated endpoint.
   *
   * -> if the endpoint is already connected return it.
   * -> if the connection is pending wait for the result
   * -> if the socket do not exist, create it, and try to connect it
   * -> if the socket is disconnected try to reconnect it
   */
  class TransportSocketCache {
  public:
    ~TransportSocketCache();

    void init();
    void close();

    qi::Future<qi::TransportSocketPtr> socket(const std::string &endpoint);
    qi::Future<qi::TransportSocketPtr> socket(const std::vector<std::string> &endpoints);

  protected:
    //TransportSocket
    void onSocketConnected(TransportSocketPtr client, const std::string &endpoint);
    void onSocketDisconnected(int error, TransportSocketPtr client, const std::string &endpoint);

  private:
    //maintain a cache of remote connections
    typedef std::map< std::string, TransportSocketConnection > TransportSocketConnectionMap;
    TransportSocketConnectionMap _sockets;
    boost::mutex                 _socketsMutex;
    bool                         _dying;
  };

};

#endif   /* !TRANSPORT_CACHE_PP_ */
