#pragma once
/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#ifndef _SRC_OBJECT_P_HPP_
#define _SRC_OBJECT_P_HPP_

#include <iostream>
#include <string>
#include <boost/thread/recursive_mutex.hpp>

#include <qi/atomic.hpp>

#include <qi/type/api.hpp>
#include <qi/signature.hpp>
#include <qi/future.hpp>
#include <qi/anyobject.hpp>
#include <qi/type/metasignal.hpp>
#include <qi/type/metamethod.hpp>
#include <qi/signal.hpp>

namespace qi {

  class EventLoop;
  class ManageablePrivate
  {
  public:
    ManageablePrivate();
    // SignalLinks that target us. Needed to be able to disconnect upon destruction
    std::vector<SignalSubscriber>       registrations;
    mutable boost::mutex                        registrationsMutex;
    Manageable::TimedMutexPtr           objectMutex; //returned by mutex()
    bool                                dying;
    // Event loop in which calls are made if set
    EventLoop                          *eventLoop;

    bool statsEnabled;
    bool traceEnabled;
    ObjectStatistics stats;
    qi::Atomic<int> traceId;
  };

};

#endif  // _SRC_OBJECT_P_HPP_
