/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/
#include <iostream>
#include <vector>
#include <map>
#include <set>

#include <qimessaging/genericobject.hpp>
#include <qimessaging/transportserver.hpp>
#include <qimessaging/transportsocket.hpp>
#include <qimessaging/servicedirectory.hpp>
#include <qimessaging/session.hpp>
#include <qimessaging/datastream.hpp>
#include <qimessaging/serviceinfo.hpp>
#include <qimessaging/objecttypebuilder.hpp>
#include "transportserver_p.hpp"
#include "serverresult.hpp"
#include "session_p.hpp"
#include <qi/os.hpp>
#include <qi/log.hpp>
#include <qimessaging/url.hpp>
#include "servicedirectory_p.hpp"
#include "signal_p.hpp"
#include "server.hpp"

namespace qi
{


  ServiceDirectoryPrivate::ServiceDirectoryPrivate()
    : _sdbo(boost::shared_ptr<ServiceDirectoryBoundObject>(new ServiceDirectoryBoundObject))
  {
    _server.addObject(1, _sdbo);
  }

  ServiceDirectoryPrivate::~ServiceDirectoryPrivate()
  {
  }


  qi::ObjectPtr createSDP(ServiceDirectoryBoundObject* self) {
    qi::ObjectTypeBuilder<ServiceDirectoryBoundObject> ob;

    ob.advertiseMethod("service", &ServiceDirectoryBoundObject::service);
    ob.advertiseMethod("services", &ServiceDirectoryBoundObject::services);
    ob.advertiseMethod("registerService", &ServiceDirectoryBoundObject::registerService);
    ob.advertiseMethod("unregisterService", &ServiceDirectoryBoundObject::unregisterService);
    ob.advertiseMethod("serviceReady", &ServiceDirectoryBoundObject::serviceReady);
    return ob.object(self);
  }

  ServiceDirectoryBoundObject::ServiceDirectoryBoundObject()
    : ServiceBoundObject(1, createSDP(this), qi::MetaCallType_Direct)
    , servicesCount(0)
    , currentSocket()
  {
  }

  ServiceDirectoryBoundObject::~ServiceDirectoryBoundObject()
  {
  }

  void ServiceDirectoryBoundObject::onSocketDisconnected(TransportSocketPtr socket, int error)
  {
    // if services were connected behind the socket
    std::map<TransportSocketPtr, std::vector<unsigned int> >::iterator it;
    if ((it = socketToIdx.find(socket)) == socketToIdx.end())
    {
      return;
    }
    // Copy the vector, iterators will be invalidated.
    std::vector<unsigned int> ids = it->second;
    // Always start at the beginning since we erase elements on unregisterService
    // and mess up the iterator
    for (std::vector<unsigned int>::iterator it2 = ids.begin();
         it2 != ids.end();
         ++it2)
    {
      qiLogInfo("qimessaging.ServiceDirectory") << "service "
                                                << connectedServices[*it2].name()
                                                << " (#" << *it2 << ") disconnected"
                                                << std::endl;
      unregisterService(*it2);
    }
    socketToIdx.erase(it);
    ServiceBoundObject::onSocketDisconnected(socket, error);
  }

  std::vector<ServiceInfo> ServiceDirectoryBoundObject::services()
  {
    std::vector<ServiceInfo> result;
    std::map<unsigned int, ServiceInfo>::const_iterator it;

    for (it = connectedServices.begin(); it != connectedServices.end(); ++it)
      result.push_back(it->second);

    return result;
  }

  ServiceInfo ServiceDirectoryBoundObject::service(const std::string &name)
  {
    std::map<unsigned int, ServiceInfo>::const_iterator servicesIt;
    std::map<std::string, unsigned int>::const_iterator it;

    it = nameToIdx.find(name);
    if (it == nameToIdx.end())
      return ServiceInfo();

    unsigned int idx = it->second;

    servicesIt = connectedServices.find(idx);
    if (servicesIt == connectedServices.end())
      return ServiceInfo();

    return servicesIt->second;
  }

  unsigned int ServiceDirectoryBoundObject::registerService(const ServiceInfo &svcinfo)
  {
    std::map<std::string, unsigned int>::iterator it;
    it = nameToIdx.find(svcinfo.name());
    if (it != nameToIdx.end())
    {
      qiLogWarning("qimessaging.ServiceDirectory")  << "service " << svcinfo.name()
                                                    << " is already registered (#" << it->second << ")" << std::endl;
      return 0;
    }

    unsigned int idx = ++servicesCount;
    nameToIdx[svcinfo.name()] = idx;
    // Do not add serviceDirectory on the map (socket() == null)
    if (idx != qi::Message::Service_ServiceDirectory)
    {
      socketToIdx[socket()].push_back(idx);
    }
    pendingServices[idx] = svcinfo;
    pendingServices[idx].setServiceId(idx);

    qiLogInfo("qimessaging.ServiceDirectory")  << "service " << svcinfo.name() << " registered (#" << idx << ")" << std::endl;
    std::vector<std::string>::const_iterator jt;
    for (jt = svcinfo.endpoints().begin(); jt != svcinfo.endpoints().end(); ++jt)
    {
      qiLogDebug("qimessaging.ServiceDirectory") << svcinfo.name() << " is now on " << *jt << std::endl;
    }

    return idx;
  }

  void ServiceDirectoryBoundObject::unregisterService(const unsigned int &idx)
  {
    // search the id before accessing it
    // otherwise operator[] create a empty entry
    std::map<unsigned int, ServiceInfo>::iterator it2;
    it2 = connectedServices.find(idx);
    if (it2 == connectedServices.end())
    {
      qiLogError("qimessaging.ServiceDirectory") << "Can't find service #" << idx;
      return;
    }

    std::map<std::string, unsigned int>::iterator it;
    it = nameToIdx.find(connectedServices[idx].name());
    if (it == nameToIdx.end())
    {
      qiLogError("Mapping error, service not in nameToIdx");
      return;
    }
    std::string serviceName = it2->second.name();
    qiLogInfo("qimessaging.ServiceDirectory") << "service "
      << serviceName
      << " (#" << idx << ") unregistered"
      << std::endl;
    nameToIdx.erase(it);
    connectedServices.erase(it2);

    // Find and remove serviceId into socketToIdx map
#if 0
    std::map<TransportSocketPtr , std::vector<unsigned int> >::iterator socketIt;
    for (socketIt = socketToIdx.begin(); socketIt != socketToIdx.end(); ++socketIt)
    {
      // notify every session that the service is unregistered
      qi::Message msg;
      msg.setType(qi::Message::Type_Event);
      msg.setService(qi::Message::Service_Server);
      msg.setObject(qi::Message::GenericObject_Main);
      msg.setEvent(qi::Message::ServiceDirectoryEvent_ServiceUnregistered);

      qi::Buffer     buf;
      qi::ODataStream d(buf);
      d << serviceName;

      if (d.status() == qi::ODataStream::Status_Ok)
      {
        msg.setBuffer(buf);
        if (!socketIt->first->send(msg))
        {
          qiLogError("qimessaging.Session") << "Error while unregister service, cannot send event.";
        }
      }

      std::vector<unsigned int>::iterator serviceIdxIt;
      for (serviceIdxIt = socketIt->second.begin();
           serviceIdxIt != socketIt->second.end();
           ++serviceIdxIt)
      {
        if (*serviceIdxIt == idx)
        {
          socketIt->second.erase(serviceIdxIt);
          break;
        }
      }
    }
#endif
  }

  void ServiceDirectoryBoundObject::serviceReady(const unsigned int &idx)
  {
    // search the id before accessing it
    // otherwise operator[] create a empty entry
    std::map<unsigned int, ServiceInfo>::iterator itService;
    itService = pendingServices.find(idx);
    if (itService == pendingServices.end())
    {
      qiLogError("qimessaging.ServiceDirectory") << "Can't find pending service #" << idx;
      return;
    }

    std::string serviceName = itService->second.name();
    connectedServices[idx] = itService->second;
    pendingServices.erase(itService);

#if 0
    std::map<TransportSocketPtr, std::vector<unsigned int> >::iterator socketIt;
    for (socketIt = socketToIdx.begin(); socketIt != socketToIdx.end(); ++socketIt)
    {
      qi::Message msg;
      msg.setType(qi::Message::Type_Event);
      msg.setService(qi::Message::Service_Server);
      msg.setObject(qi::Message::GenericObject_Main);
      msg.setEvent(qi::Message::ServiceDirectoryEvent_ServiceRegistered);

      qi::Buffer     buf;
      qi::ODataStream d(buf);
      d << serviceName;

      if (d.status() == qi::ODataStream::Status_Ok)
      {
        msg.setBuffer(buf);
        if (!socketIt->first->send(msg))
        {
          qiLogError("qimessaging.Session") << "Error while register service, cannot send event.";
        }
      }
    }
#endif
  }

ServiceDirectory::ServiceDirectory()
  : _p(new ServiceDirectoryPrivate())
{
}

ServiceDirectory::~ServiceDirectory()
{
  close();
  delete _p;
}

bool ServiceDirectory::listen(const qi::Url &address)
{
  bool b = _p->_server.listen(address);
  if (!b)
    return false;

  ServiceDirectoryBoundObject *sdbo = static_cast<ServiceDirectoryBoundObject*>(_p->_sdbo.get());

  ServiceInfo si;
  si.setName("ServiceDirectory");
  si.setServiceId(1);
  si.setMachineId(qi::os::getMachineId());
  si.setEndpoints(_p->_server.endpoints());
  unsigned int regid = sdbo->registerService(si);
  sdbo->serviceReady(1);
  //serviceDirectory must have id '1'
  assert(regid == 1);
  return true;
}

void ServiceDirectory::close() {
  _p->_server.close();
}

qi::Url ServiceDirectory::listenUrl() const {
  return _p->_server.listenUrl();
}


}; // !qi
