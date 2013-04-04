/**
 * Copyright (C) 2013 Alec Thomas <alec@swapoff.org>
 * All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.
 *
 * Author: Alec Thomas <alec@swapoff.org>
 */

#include <cassert>
#include <string>
#include <iostream>
#include <Python/Python.h>
#include "entityx/python/PythonSystem.h"

using namespace std;
using namespace boost;
namespace py = boost::python;

namespace entityx {
namespace python {


static const py::object None;


/**
 * A Component exposed to Python that can create new entities.
 */
class PythonEntityBuilderComponent : public entityx::Component<PythonEntityBuilderComponent> {
public:
  PythonEntityBuilderComponent(entityx::shared_ptr<EntityManager> entity_manager) : entity_manager_(entity_manager) {}

  template <typename ... Args>
  void build(py::object cls, Args ... args) {
    Entity entity = entity_manager_->create();
  }

private:
  entityx::shared_ptr<EntityManager> entity_manager_;
};


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

  void update(float dt) {}

  Entity _entity;
};


BOOST_PYTHON_MODULE(_entityx) {
  py::class_<PythonEntityXLogger>("_Logger", py::no_init)
    .def("write", &PythonEntityXLogger::write);
  py::class_<PythonEntity>("_Entity", py::init<Entity>())
    .def_readonly("_entity", &PythonEntity::_entity)
    .def("update", &PythonEntity::update);
  py::class_<Entity>("_RawEntity", py::no_init);
  py::class_<EntityManager, entityx::shared_ptr<EntityManager>, boost::noncopyable>("_EntityManager", py::no_init)
    .def("create", &EntityManager::create);
}


static void log_to_stderr(const std::string &text) {
  cerr << "python: " << text << endl;
}

static void log_to_stdout(const std::string &text) {
  cout << "python: " << text << endl;
}

// PythonSystem below here

bool PythonSystem::initialized_ = false;

PythonSystem::PythonSystem(entityx::shared_ptr<EntityManager> entity_manager, const std::vector<std::string> &python_paths)
    : entity_manager_(entity_manager), python_paths_(python_paths), stdout_(log_to_stdout), stderr_(log_to_stderr) {
  if (!initialized_) {
    initialize_python_module();
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

void PythonSystem::initialize_python_module() {
  assert(PyImport_AppendInittab("_entityx", init_entityx) != -1 && "Failed to initialize _entityx Python module");
}

void PythonSystem::configure(entityx::shared_ptr<EventManager> event_manager) {
  event_manager->subscribe<EntityDestroyedEvent>(*this);
  event_manager->subscribe<ComponentAddedEvent<PythonComponent>>(*this);

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

    py::object entityx = py::import("entityx");
    entityx.attr("entity_manager") = boost::ref(entity_manager_);
    // entityx.attr("event_manager") = boost::ref(event_manager);
  } catch (...) {
    PyErr_Print();
    PyErr_Clear();
    throw;
  }
}

void PythonSystem::update(entityx::shared_ptr<EntityManager> entity_manager, entityx::shared_ptr<EventManager> event_manager, double dt) {
  for (auto entity : entity_manager->entities_with_components<PythonComponent>()) {
    shared_ptr<PythonComponent> python = entity.component<PythonComponent>();

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

void PythonSystem::receive(const EntityDestroyedEvent &event) {
  for (auto proxy : event_proxies_) {
    proxy->delete_receiver(event.entity);
  }
}

void PythonSystem::receive(const ComponentAddedEvent<PythonComponent> &event) {
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

  for (auto proxy : event_proxies_) {
    if (proxy->can_send(event.component->object)) {
      proxy->add_receiver(event.entity);
    }
  }
}

}
}
