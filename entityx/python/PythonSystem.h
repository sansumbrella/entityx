/**
 * Copyright (C) 2013 Alec Thomas <alec@swapoff.org>
 * All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.
 *
 * Author: Alec Thomas <alec@swapoff.org>
 */

#pragma once

#include <vector>
#include <string>
#include <boost/function.hpp>
#include <boost/python.hpp>
#include <Python/Python.h>
#include "entityx/System.h"
#include "entityx/Entity.h"
#include "entityx/Event.h"

namespace entityx {
namespace python {


class PythonEntityComponent : public entityx::Component<PythonEntityComponent> {
public:
  template <typename ...Args>
  PythonEntityComponent(const std::string &module, const std::string &cls, Args ... args) : module(module), cls(cls) {
    unpack_args(args...);
  }

  boost::python::object object;
  boost::python::list args;
  const std::string module, cls;

private:
  template <typename A, typename ...Args>
  void unpack_args(A &arg, Args ... remainder) {
    args.append(arg);
    unpack_args(remainder...);
  }

  void unpack_args() {}
};


class PythonEventProxy {
public:
  PythonEventProxy(const std::string &handler_name) : handler_name(handler_name) {}
  virtual ~PythonEventProxy() {}

  void add_receiver(Entity entity) {
    entities.push_back(entity);
  }

  void delete_receiver(Entity entity) {
    for (auto i = entities.begin(); i != entities.end(); ++i) {
      if (entity == *i) {
        entities.erase(i);
        break;
      }
    }
  }

  virtual bool can_send(const boost::python::object &object) const {
    return PyObject_HasAttrString(object.ptr(), handler_name.c_str());
  }

protected:
  std::vector<Entity> entities;
  const std::string handler_name;
};


/**
 * A PythonEventProxy that broadcasts events to all entities with a matching
 * handler method.
 */
template <typename Event>
class BroadcastPythonEventProxy : public PythonEventProxy, public Receiver<BroadcastPythonEventProxy<Event>> {
public:
  BroadcastPythonEventProxy(const std::string &handler_name) : PythonEventProxy(handler_name) {}
  virtual ~BroadcastPythonEventProxy() {}

  void receive(const Event &event) {
    for (auto entity : entities) {
      auto py_entity = entity.template component<PythonEntityComponent>();
      py_entity->object.attr(handler_name.c_str())(event);
    }
  }
};


class PythonSystem : public entityx::System<PythonSystem>, public entityx::Receiver<PythonSystem> {
public:
  typedef boost::function<void (const std::string &)> LoggerFunction;

  PythonSystem(const std::vector<std::string> &python_paths);
  virtual ~PythonSystem();

  virtual void configure(EventManager &events) override;
  virtual void update(EntityManager &entities, EventManager &events, double dt) override;
  void shutdown();

  void log_to(LoggerFunction stdout, LoggerFunction stderr);

  template <typename Event>
  void add_event_proxy(EventManager &event_manager, const std::string &handler_name) {
    auto proxy = boost::make_shared<BroadcastPythonEventProxy<Event>>(handler_name);
    event_manager.subscribe<Event>(*proxy);
    event_proxies_.push_back(boost::static_pointer_cast<PythonEventProxy>(proxy));
  }

  template <typename Event, typename Proxy>
  void add_event_proxy(EventManager &event_manager, boost::shared_ptr<Proxy> proxy) {
    event_manager.subscribe<Event>(*proxy);
    event_proxies_.push_back(boost::static_pointer_cast<PythonEventProxy>(proxy));
  }

  void receive(const EntityDestroyedEvent &event);
  void receive(const ComponentAddedEvent<PythonEntityComponent> &event);
private:
  void initialize();

  const std::vector<std::string> python_paths_;
  LoggerFunction stdout_, stderr_;
  static bool initialized_;
  std::vector<boost::shared_ptr<PythonEventProxy>> event_proxies_;
};

}
}
