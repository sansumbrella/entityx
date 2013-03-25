/**
 * Copyright (C) 2013 Alec Thomas <alec@swapoff.org>
 * All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.
 *
 * Author: Alec Thomas <alec@swapoff.org>
 */

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


class PythonSystem : public entityx::System<PythonSystem>, public entityx::Receiver<PythonSystem> {
public:
  typedef boost::function<void (const std::string &)> LoggerFunction;

  PythonSystem(const std::vector<std::string> &python_paths);
  virtual ~PythonSystem();

  virtual void configure(EventManager &events) override;
  virtual void update(EntityManager &entities, EventManager &events, double dt) override;
  void shutdown();

  void log_to(LoggerFunction stdout, LoggerFunction stderr);

  void receive(const EntityCreatedEvent &event);
  void receive(const EntityDestroyedEvent &event);
  void receive(const ComponentAddedEvent<PythonEntityComponent> &event);
private:
  void initialize();

  const std::vector<std::string> python_paths_;
  LoggerFunction stdout_, stderr_;
  static bool initialized_;
};

}
}
