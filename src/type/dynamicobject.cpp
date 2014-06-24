/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/
#include <map>

#include <boost/make_shared.hpp>

#include <qi/api.hpp>
#include <qi/anyvalue.hpp>
#include <qi/type/typeinterface.hpp>
#include <qi/anyvalue.hpp>
#include <qi/anyobject.hpp>
#include <qi/anyfunction.hpp>
#include <qi/type/metaobject.hpp>
#include <qi/signal.hpp>
#include <qi/type/dynamicobject.hpp>


qiLogCategory("qitype.dynamicobject");

namespace qi
{

  class DynamicObjectPrivate
  {
  public:
    DynamicObjectPrivate();

    ~DynamicObjectPrivate();
    // get or create signal, or 0 if id is not an event
    SignalBase* createSignal(unsigned int id);
    PropertyBase* property(unsigned int id);
    bool                                dying;
    typedef std::map<unsigned int, std::pair<SignalBase*, bool> > SignalMap;
    typedef std::map<unsigned int, std::pair<AnyFunction, MetaCallType> > MethodMap;
    SignalMap           signalMap;
    MethodMap           methodMap;
    MetaObject          meta;
    ObjectThreadingModel threadingModel;

    typedef std::map<unsigned int, std::pair<PropertyBase*, bool> > PropertyMap;
    PropertyMap propertyMap;
  };

  DynamicObjectPrivate::DynamicObjectPrivate()
  : threadingModel(ObjectThreadingModel_Default)
  {
  }

  void DynamicObject::setManageable(Manageable* m)
  {
    _p->methodMap.insert(Manageable::manageableMmethodMap().begin(),
      Manageable::manageableMmethodMap().end());
    _p->meta = MetaObject::merge(_p->meta, Manageable::manageableMetaObject());
    Manageable::SignalMap& smap = Manageable::manageableSignalMap();
    // need to convert signal getters to signal, we have the instance
    for (Manageable::SignalMap::iterator it = smap.begin(); it != smap.end(); ++it)
    {
      SignalBase* sb = it->second(m);
      _p->signalMap[it->first] = std::make_pair(sb, false);
    }
  }

  DynamicObjectPrivate::~DynamicObjectPrivate()
  {
    //properties are also in signals, do not delete

    //only delete property we created
    for (PropertyMap::iterator it2 = propertyMap.begin(); it2 != propertyMap.end(); ++it2) {
      if (it2->second.second)
        delete it2->second.first;
    }

    //only delete signal we created
    for (SignalMap::iterator it = signalMap.begin(); it != signalMap.end(); ++it) {
      if (it->second.second)
        delete it->second.first;
    }
  }

  SignalBase* DynamicObjectPrivate::createSignal(unsigned int id)
  {

    SignalMap::iterator i = signalMap.find(id);
    if (i != signalMap.end())
      return i->second.first;
    if (meta.property(id))
    { // Replicate signal of prop in signalMap
      SignalBase* sb = property(id)->signal();
      signalMap[id] = std::make_pair(sb, false);
      return sb;
    }
    MetaSignal* sig = meta.signal(id);
    if (sig)
    {
      SignalBase* sb = new SignalBase(sig->parametersSignature());
      signalMap[id] = std::make_pair(sb, true);
      return sb;
    }
    return 0;
  }

  class DynamicObjectTypeInterface: public ObjectTypeInterface, public DefaultTypeImplMethods<DynamicObject>
  {
  public:
    DynamicObjectTypeInterface() {}
    virtual const MetaObject& metaObject(void* instance);
    virtual qi::Future<AnyReference> metaCall(void* instance, AnyObject context, unsigned int method, const GenericFunctionParameters& params, MetaCallType callType, Signature returnSignature);
    virtual void metaPost(void* instance, AnyObject context, unsigned int signal, const GenericFunctionParameters& params);
    virtual qi::Future<SignalLink> connect(void* instance, AnyObject context, unsigned int event, const SignalSubscriber& subscriber);
    /// Disconnect an event link. Returns if disconnection was successful.
    virtual qi::Future<void> disconnect(void* instance, AnyObject context, SignalLink linkId);
    virtual const std::vector<std::pair<TypeInterface*, int> >& parentTypes();
    virtual qi::Future<AnyValue> property(void* instance, unsigned int id);
    virtual qi::Future<void> setProperty(void* instance, unsigned int id, AnyValue val);
    _QI_BOUNCE_TYPE_METHODS(DefaultTypeImplMethods<DynamicObject>);
  };

  DynamicObject::DynamicObject()
  {
    _p = boost::make_shared<DynamicObjectPrivate>();
  }

  DynamicObject::~DynamicObject()
  {
  }

  void DynamicObject::setMetaObject(const MetaObject& m)
  {
    _p->meta = m;
    // We will populate stuff on demand
  }

  MetaObject& DynamicObject::metaObject()
  {
    return _p->meta;
  }

  void DynamicObject::setThreadingModel(ObjectThreadingModel model)
  {
    _p->threadingModel = model;
  }

  ObjectThreadingModel DynamicObject::threadingModel() const
  {
    return _p->threadingModel;
  }

  void DynamicObject::setMethod(unsigned int id, AnyFunction callable, MetaCallType threadingModel)
  {
    _p->methodMap[id] = std::make_pair(callable, threadingModel);
  }

  void DynamicObject::setSignal(unsigned int id, SignalBase* signal)
  {
    _p->signalMap[id] = std::make_pair(signal, false);
  }


  void DynamicObject::setProperty(unsigned int id, PropertyBase* property)
  {
    _p->propertyMap[id] = std::make_pair(property, false);
  }

  const AnyFunction& DynamicObject::method(unsigned int id) const
  {
    static AnyFunction empty;
    DynamicObjectPrivate::MethodMap::iterator i = _p->methodMap.find(id);
    if (i == _p->methodMap.end())
      return empty;
    else
      return i->second.first;
  }

  SignalBase* DynamicObject::signal(unsigned int id) const
  {
    if (_p->meta.property(id))
      return const_cast<DynamicObject*>(this)->property(id)->signal();
    DynamicObjectPrivate::SignalMap::iterator i = _p->signalMap.find(id);
    if (i == _p->signalMap.end())
      return 0;
    else
      return i->second.first;
  }

  PropertyBase* DynamicObject::property(unsigned int id) const
  {
    return _p->property(id);
  }

  PropertyBase* DynamicObjectPrivate::property(unsigned int id)
  {
    DynamicObjectPrivate::PropertyMap::iterator i = propertyMap.find(id);
    if (i == propertyMap.end())
    {
      MetaProperty* p = meta.property(id);
      if (!p)
        throw std::runtime_error("Id is not id of a property");
      // Fetch its type
      qi::Signature sig = p->signature();
      TypeInterface* type = TypeInterface::fromSignature(sig);
      if (!type)
        throw std::runtime_error("Unable to construct a type from " + sig.toString());
      PropertyBase* res = new GenericProperty(type);
      propertyMap[id] = std::make_pair(res, true);
      return res;
    }
    else
      return i->second.first;
  }

  qi::Future<AnyReference> DynamicObject::metaCall(AnyObject context, unsigned int method, const GenericFunctionParameters& params, MetaCallType callType, Signature returnSignature)
  {
    DynamicObjectPrivate::MethodMap::iterator i = _p->methodMap.find(method);
    if (i == _p->methodMap.end())
    {
      std::stringstream ss;
      ss << "Can't find methodID: " << method;
      return qi::makeFutureError<AnyReference>(ss.str());
    }
    if (returnSignature.isValid())
    {
      MetaMethod *mm = metaObject().method(method);
      if (!mm)
        return makeFutureError<AnyReference>("Unexpected error: MetaMethod not found");
      if (mm->returnSignature().isConvertibleTo(returnSignature) == 0)
      {
        if (returnSignature.isConvertibleTo(mm->returnSignature())==0)
          return makeFutureError<AnyReference>(
            "Call error: will not be able to convert return type from "
            + mm->returnSignature().toString()
            + " to " + returnSignature.toString());
        else
         qiLogWarning() << "Return signature might be incorrect depending on the value, from "
            + mm->returnSignature().toString()
            + " to " + returnSignature.toString();
      }
    }
    Manageable* m = static_cast<Manageable*>(context.asGenericObject());
    GenericFunctionParameters p;
    p.reserve(params.size()+1);
    if (method >= Manageable::startId && method < Manageable::endId)
      p.push_back(AnyReference::from(m));
    else
      p.push_back(AnyReference::from(this));
    p.insert(p.end(), params.begin(), params.end());
    return ::qi::metaCall(context.eventLoop(), _p->threadingModel,
      i->second.second, callType, context, method, i->second.first, p);
  }

  qi::Future<void> DynamicObject::metaSetProperty(unsigned int id, AnyValue val)
  {
    try
    {
      property(id)->setValue(val.asReference());
    }
    catch(const std::exception& e)
    {
      return qi::makeFutureError<void>(std::string("setProperty: ") + e.what());
    }
    return qi::Future<void>(0);
  }

  qi::Future<AnyValue> DynamicObject::metaProperty(unsigned int id)
  {
    return qi::Future<AnyValue>(property(id)->value());
  }

  static void reportError(qi::Future<AnyReference> fut) {
    if (fut.hasError()) {
      qiLogError() << fut.error();
      return;
    }
    qi::AnyReference ref = fut.value();
    ref.destroy();
  }

  void DynamicObject::metaPost(AnyObject context, unsigned int event, const GenericFunctionParameters& params)
  {
    SignalBase * s = _p->createSignal(event);
    if (s)
    { // signal is declared, create if needed
      s->trigger(params);
      return;
    }

    // Allow emit on a method
    if (metaObject().method(event)) {
      qi::Future<AnyReference> fut = metaCall(context, event, params, MetaCallType_Queued);
      fut.connect(&reportError);
      return;
    }
    qiLogError() << "No such event " << event;
    return;
  }

  qi::Future<SignalLink> DynamicObject::metaConnect(unsigned int event, const SignalSubscriber& subscriber)
  {
    SignalBase * s = _p->createSignal(event);
    if (!s)
      return qi::makeFutureError<SignalLink>("Cannot find signal");
    SignalLink l = s->connect(subscriber);
    if (l == SignalBase::invalidSignalLink)
      return qi::Future<SignalLink>(l);
    SignalLink link = ((SignalLink)event << 32) + l;
    assert(link >> 32 == event);
    assert((link & 0xFFFFFFFF) == l);
    qiLogDebug() << "New subscriber " << link <<" to event " << event;
    return qi::Future<SignalLink>(link);
  }

  qi::Future<void> DynamicObject::metaDisconnect(SignalLink linkId)
  {
    unsigned int event = linkId >> 32;
    unsigned int link = linkId & 0xFFFFFFFF;
    //TODO: weird to call createSignal in disconnect
    SignalBase* s = _p->createSignal(event);
    if (!s)
      return qi::makeFutureError<void>("Cannot find local signal connection.");
    bool b = s->disconnect(link);
    if (!b) {
      return qi::makeFutureError<void>("Cannot find local signal connection.");
    }
    return qi::Future<void>(0);
  }

  static AnyReference locked_call(AnyFunction& function,
                                     const GenericFunctionParameters& params,
                                     Manageable::TimedMutexPtr lock)
  {
    static long msWait = -1;
    if (msWait == -1)
    { // thread-safeness: worst case we set it twice
      std::string s = os::getenv("QI_DEADLOCK_TIMEOUT");
      if (s.empty())
        msWait = 30000; // default wait of 30 seconds
      else
        msWait = strtol(s.c_str(), 0, 0);
    }
    if (!msWait)
    {
       boost::recursive_timed_mutex::scoped_lock l(*lock);
       return function.call(params);
    }
    else
    {
      boost::system_time timeout = boost::get_system_time() + boost::posix_time::milliseconds(msWait);
      qiLogDebug() << "Aquiering module lock...";
      boost::recursive_timed_mutex::scoped_lock l(*lock, timeout);
      qiLogDebug() << "Checking lock acquisition...";
      if (!l.owns_lock())
      {
        qiLogWarning() << "Time-out acquiring object lock when calling method. Deadlock?";
        throw std::runtime_error("Time-out acquiring lock. Deadlock?");
      }
      qiLogDebug() << "Calling function";
      return function.call(params);
    }
  }
  namespace {

    static bool traceValidateSignature(const Signature& s)
    {
      // Refuse to trace unknown (not serializable), object (too expensive), raw (possibly big)
      if (s.type() == Signature::Type_Unknown
          || s.type() == Signature::Type_Object
          || s.type() == Signature::Type_Raw
          || s.type() == Signature::Type_Pointer)
        return false;
      const SignatureVector& c = s.children();
      //return std::all_of(c.begin(), c.end(), traceValidateSignature);
      for (unsigned i=0; i<c.size(); ++i)
        if (!traceValidateSignature(c[i]))
          return false;
      return true;
    }

    // validate v for transmission to a trace signal
    static const AnyValue& traceValidateValue(const AnyValue& v)
    {
      static AnyValue fallback = AnyValue(AnyReference::from("**UNSERIALIZABLE**"));
      Signature s = v.signature(true);
      return traceValidateSignature(s)? v:fallback;
    }

    inline void call(qi::Promise<AnyReference>& out,
                      AnyObject context,
                      bool lock,
                      const GenericFunctionParameters& params,
                      unsigned int methodId,
                      AnyFunction& func,
                      unsigned int callerContext,
                      qi::os::timeval postTimestamp
                      )
    {
      bool stats = context && context.isStatsEnabled();
      bool trace = context && context.isTraceEnabled();
      qi::AnyReference retref;
      int tid = 0; // trace call id, reused for result sending
      if (trace)
      {
        tid = context.asGenericObject()->_nextTraceId();
        qi::os::timeval tv;
        qi::os::gettimeofday(&tv);
        AnyValueVector args;
        args.resize(params.size()-1);
        for (unsigned i=0; i<params.size()-1; ++i)
        {
          if (!params[i+1].type())
            args[i] = AnyValue::from("<??" ">");
          else
          {
            switch(params[i+1].type()->kind())
            {
            case TypeKind_Int:
            case TypeKind_String:
            case TypeKind_Float:
            case TypeKind_VarArgs:
            case TypeKind_List:
            case TypeKind_Map:
            case TypeKind_Tuple:
            case TypeKind_Dynamic:
              args[i] = params[i+1];
              break;
            default:
              args[i] = AnyValue::from("<??" ">");
            }
          }
        }
        context.asGenericObject()->traceObject(EventTrace(
          tid, EventTrace::Event_Call, methodId, traceValidateValue(AnyValue::from(args)), tv,
          0,0, callerContext, qi::os::gettid(), postTimestamp));
      }

      qi::int64_t time = stats?qi::os::ustime():0;
      std::pair<int64_t, int64_t> cputime, cpuendtime;
      if (stats||trace)
         cputime = qi::os::cputime();

      bool success = false;
      try
      {
        qi::AnyReference ret;
        //the return value is destroyed by ServerResult in the future callback.
        if (lock)
          ret = locked_call(func, params, context.asGenericObject()->mutex());
        else
          ret = func.call(params);
        //copy the value for tracing later. (we want the tracing to happend after setValue
        if (trace)
          retref = ret.clone();
        //the reference, is dropped here... not cool man!
        out.setValue(ret);
        success = true;
      }
      catch(const std::exception& e)
      {
        success = false;
        out.setError(e.what());
      }
      catch(...)
      {
        success = false;
        out.setError("Unknown exception caught.");
      }

      if (stats||trace)
      {
        cpuendtime = qi::os::cputime();
        cpuendtime.first -= cputime.first;
        cpuendtime.second -= cputime.second;
      }

      if (stats)
        context.asGenericObject()->pushStats(methodId, (float)(qi::os::ustime() - time)/1e6f,
                           (float)cpuendtime.first / 1e6f,
                           (float)cpuendtime.second / 1e6f);


      if (trace)
      {
        qi::os::timeval tv;
        qi::os::gettimeofday(&tv);
        AnyValue val;
        if (success)
          val = AnyValue(retref, false, true);
        else
          val = AnyValue::from(out.future().error());
        context.asGenericObject()->traceObject(EventTrace(tid,
          success?EventTrace::Event_Result:EventTrace::Event_Error,
          methodId, traceValidateValue(val), tv, cpuendtime.first, cpuendtime.second, callerContext, qi::os::gettid(), postTimestamp));
      }
    }
  }

  class MFunctorCall
  {
  public:
    MFunctorCall(AnyFunction& func, GenericFunctionParameters& params,
       qi::Promise<AnyReference>* out, bool noCloneFirst,
       AnyObject context, unsigned int methodId, bool lock, unsigned int callerId, qi::os::timeval postTimestamp)
    : noCloneFirst(noCloneFirst)
    {
      this->out = out;
      this->methodId = methodId;
      this->context = context;
      this->lock = lock;
      this->callerId = callerId;
      std::swap(this->func, func);
      std::swap((AnyReferenceVector&) params,
        (AnyReferenceVector&) this->params);
      this->postTimestamp = postTimestamp;
    }
    MFunctorCall(const MFunctorCall& b)
    {
      (*this) = b;
    }
    void operator = (const MFunctorCall& b)
    {
      // Implement move semantic on =
      std::swap( (AnyReferenceVector&) params,
        (AnyReferenceVector&) b.params);
      std::swap(func, const_cast<MFunctorCall&>(b).func);
      context = b.context;
      methodId = b.methodId;
      this->lock = b.lock;
      this->out = b.out;
      noCloneFirst = b.noCloneFirst;
      callerId = b.callerId;
      this->postTimestamp = b.postTimestamp;
    }
    void operator()()
    {
      call(*out, context, lock, params, methodId, func, callerId, postTimestamp);
      params.destroy(noCloneFirst);
      delete out;
    }
    qi::Promise<AnyReference>* out;
    GenericFunctionParameters params;
    AnyFunction func;
    bool noCloneFirst;
    AnyObject context;
    bool lock;
    unsigned int methodId;
    unsigned int callerId;
    qi::os::timeval postTimestamp;
  };

  qi::Future<AnyReference> metaCall(EventLoop* el,
    ObjectThreadingModel objectThreadingModel,
    MetaCallType methodThreadingModel,
    MetaCallType callType,
    AnyObject context,
    unsigned int methodId,
    AnyFunction func, const GenericFunctionParameters& params, bool noCloneFirst,
    unsigned int callerId,
    qi::os::timeval postTimestamp)
  {
    // Implement rules described in header
    bool sync = true;
    if (el)
      sync = el->isInEventLoopThread();
    else if (methodThreadingModel != MetaCallType_Auto)
      sync = (methodThreadingModel == MetaCallType_Direct);
    else // callType default is synchronous
      sync = (callType != MetaCallType_Queued);

    bool elForced = el;
    if (!sync && !el)
      el = getEventLoop();
    qiLogDebug() << "metacall sync=" << sync << " el= " << el <<" ct= " << callType;
    bool doLock = (context && objectThreadingModel == ObjectThreadingModel_SingleThread
        && methodThreadingModel == MetaCallType_Auto);
    if (sync)
    {
      qi::Promise<AnyReference> out(FutureCallbackType_Sync);
      call(out, context, doLock, params, methodId, func, callerId?callerId:qi::os::gettid(), postTimestamp);
      return out.future();
    }
    else
    {
      // If call is handled by our thread pool, we can safely switch the promise
      // to synchronous mode.
      qi::Promise<AnyReference>* out = new qi::Promise<AnyReference>(
        elForced?FutureCallbackType_Async:FutureCallbackType_Sync);
      GenericFunctionParameters pCopy = params.copy(noCloneFirst);
      qi::Future<AnyReference> result = out->future();
      qi::os::timeval t;
      qi::os::gettimeofday(&t);
      el->post(MFunctorCall(func, pCopy, out, noCloneFirst, context, methodId, doLock, callerId?callerId:qi::os::gettid(), t));
      return result;
    }
  }

  //DynamicObjectTypeInterface implementation: just bounces everything to metaobject

  const MetaObject& DynamicObjectTypeInterface::metaObject(void* instance)
  {
    return reinterpret_cast<DynamicObject*>(instance)->metaObject();
  }

  qi::Future<AnyReference> DynamicObjectTypeInterface::metaCall(void* instance, AnyObject context, unsigned int method, const GenericFunctionParameters& params, MetaCallType callType, Signature returnSignature)
  {
    return reinterpret_cast<DynamicObject*>(instance)
      ->metaCall(context, method, params, callType, returnSignature);
  }

  void DynamicObjectTypeInterface::metaPost(void* instance, AnyObject context, unsigned int signal, const GenericFunctionParameters& params)
  {
    reinterpret_cast<DynamicObject*>(instance)->metaPost(context, signal, params);
  }

  qi::Future<SignalLink> DynamicObjectTypeInterface::connect(void* instance, AnyObject context, unsigned int event, const SignalSubscriber& subscriber)
  {
    return reinterpret_cast<DynamicObject*>(instance)->metaConnect(event, subscriber);
  }

  qi::Future<void> DynamicObjectTypeInterface::disconnect(void* instance, AnyObject context, SignalLink linkId)
  {
    return reinterpret_cast<DynamicObject*>(instance)->metaDisconnect(linkId);
  }

  const std::vector<std::pair<TypeInterface*, int> >& DynamicObjectTypeInterface::parentTypes()
  {
    static std::vector<std::pair<TypeInterface*, int> > empty;
    return empty;
  }

  qi::Future<AnyValue> DynamicObjectTypeInterface::property(void* instance, unsigned int id)
  {
    return reinterpret_cast<DynamicObject*>(instance)
      ->metaProperty(id);
  }

  qi::Future<void> DynamicObjectTypeInterface::setProperty(void* instance, unsigned int id, AnyValue value)
  {
    return reinterpret_cast<DynamicObject*>(instance)
      ->metaSetProperty(id, value);
  }

  static void cleanupDynamicObject(GenericObject *obj, bool destroyObject,
    boost::function<void (GenericObject*)> onDelete)
  {
    qiLogDebug() << "Cleaning up dynamic object " << obj << " delete=" << destroyObject
      << "  custom callback=" << !!onDelete;
    if (onDelete)
      onDelete(obj);
    if (destroyObject)
      delete reinterpret_cast<DynamicObject*>(obj->value);
    delete obj;
  }

  ObjectTypeInterface* getDynamicTypeInterface()
  {
    static DynamicObjectTypeInterface* type = 0;
    QI_THREADSAFE_NEW(type);
    return type;
  }

  AnyObject makeDynamicSharedAnyObjectImpl(DynamicObject* obj, boost::shared_ptr<Empty> other)
  {
    GenericObject* go = new GenericObject(getDynamicTypeInterface(), obj);
    return AnyObject(go, other);
  }

  AnyObject makeDynamicAnyObject(DynamicObject *obj, bool destroyObject,
    boost::function<void (GenericObject*)> onDelete)
  {
    ObjectTypeInterface* type = getDynamicTypeInterface();
    if (destroyObject || onDelete)
      return AnyObject(new GenericObject(type, obj),
        boost::bind(&cleanupDynamicObject, _1, destroyObject, onDelete));
    else
      return AnyObject(new GenericObject(type, obj), &AnyObject::deleteGenericObjectOnly);
  }

}
