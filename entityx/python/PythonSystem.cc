/**
 * Copyright (C) 2013 Alec Thomas <alec@swapoff.org>
 * All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.
 *
 * Author: Alec Thomas <alec@swapoff.org>
 */

#include <string>
#include <glog/logging.h>
#include <Python/Python.h>
#include "entityx/python/PythonSystem.h"

using namespace std;
using namespace boost;
namespace py = boost::python;

namespace entityx {
namespace python {


static const py::object None;


class PythonEntityXLogger {
public:
  PythonEntityXLogger() {}
  PythonEntityXLogger(PythonSystem::LoggerFunction logger) : logger_(logger) {}

  void write(const std::string &text) {
    logger_(text);
  }
private:
  PythonSystem::LoggerFunction logger_;
};


struct PythonEntity {
  PythonEntity(Entity entity) : _entity(entity) {}

  Entity _entity;
};


BOOST_PYTHON_MODULE(_entityx) {
  py::class_<PythonEntityXLogger>("_Logger")
    .def("write", &PythonEntityXLogger::write);
  py::class_<PythonEntity>("_Entity", py::init<Entity>())
    .def_readonly("_entity", &PythonEntity::_entity);
  py::class_<Entity>("_RawEntity");
}


static void log_to_stderr(const std::string &text) {
  LOG(WARNING) << "script: " << text;
}

static void log_to_stdout(const std::string &text) {
  LOG(INFO) << "script: " << text;
}

// PythonSystem below here

bool PythonSystem::initialized_ = false;

PythonSystem::PythonSystem(const std::vector<std::string> &python_paths)
    : python_paths_(python_paths), stdout_(log_to_stdout), stderr_(log_to_stderr) {
  if (!initialized_) {
    initialize();
  }
  Py_Initialize();
  if (!initialized_) {
    init_entityx();
    initialized_ = true;
  }
}

PythonSystem::~PythonSystem() {
  // FIXME: It would be good to do this, but it is not supported by boost::python:
  // http://www.boost.org/doc/libs/1_53_0/libs/python/todo.html#pyfinalize-safety
  // Py_Finalize();
}

void PythonSystem::initialize() {
  CHECK(PyImport_AppendInittab("_entityx", init_entityx) != -1)
    << "Failed to initialize _entityx Python module";
}

void PythonSystem::configure(EventManager &events) {
  events.subscribe<EntityCreatedEvent>(*this);
  events.subscribe<EntityDestroyedEvent>(*this);
  events.subscribe<ComponentAddedEvent<PythonEntityComponent>>(*this);

  try {
    py::object main_module = py::import("__main__");
    py::object main_namespace = main_module.attr("__dict__");

    // Initialize logging
    py::object sys = py::import("sys");
    sys.attr("stdout") = PythonEntityXLogger(stdout_);
    sys.attr("stderr") = PythonEntityXLogger(stderr_);

    // Add paths to interpreter sys.path
    for (auto path : python_paths_) {
      py::str dir = path.c_str();
      sys.attr("path").attr("insert")(0, dir);
    }
  } catch (...) {
    PyErr_Print();
    PyErr_Clear();
    throw;
  }
}

void PythonSystem::update(EntityManager &entities, EventManager &events, double dt) {
  for (auto entity : entities.entities_with_components<PythonEntityComponent>()) {
    shared_ptr<PythonEntityComponent> python = entity.component<PythonEntityComponent>();

    try {
      python->object.attr("update")(dt);
    } catch (...) {
      PyErr_Print();
      PyErr_Clear();
      throw;
    }
  }
}

void PythonSystem::shutdown() {
}

void PythonSystem::log_to(LoggerFunction stdout, LoggerFunction stderr) {
  stdout_ = stdout;
  stderr_ = stderr;
}

void PythonSystem::receive(const EntityCreatedEvent &event) {
}

void PythonSystem::receive(const EntityDestroyedEvent &event) {
}

void PythonSystem::receive(const ComponentAddedEvent<PythonEntityComponent> &event) {
  py::object module = py::import(event.component->module.c_str());
  py::object cls = module.attr(event.component->cls.c_str());
  if (py::len(event.component->args) == 0) {
    event.component->object = cls(event.entity);
  } else {
    py::list args;
    args.append(event.entity);
    args.extend(event.component->args);
    event.component->object = cls(*py::tuple(args));
  }
}

}
}
