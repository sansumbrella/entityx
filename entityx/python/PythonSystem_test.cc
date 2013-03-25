/**
 * Copyright (C) 2013 Alec Thomas <alec@swapoff.org>
 * All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.
 *
 * Author: Alec Thomas <alec@swapoff.org>
 */

#include <Python/Python.h>
#include <vector>
#include <string>
#include <gtest/gtest.h>
#include <boost/shared_ptr.hpp>
#include <boost/python.hpp>
#include "entityx/Entity.h"
#include "entityx/python/PythonSystem.h"

using namespace std;
using namespace boost;
namespace py = boost::python;
using namespace entityx;
using namespace entityx::python;

struct Position : public Component<Position> {
  Position(float x = 0.0, float y = 0.0) : x(x), y(y) {}

  float x, y;
};


struct PythonPosition : public Position, public enable_shared_from_this<PythonPosition> {
  PythonPosition(float x = 0.0, float y = 0.0) : Position(x, y) {}

  void assign_to(Entity &entity) {
    entity.assign<PythonPosition>(shared_from_this());
  }

  static shared_ptr<PythonPosition> get_component(Entity &entity) {
    return entity.component<PythonPosition>();
  }
};


BOOST_PYTHON_MODULE(entityx_python_test) {
  py::class_<PythonPosition, shared_ptr<PythonPosition>>("Position", py::init<py::optional<float, float>>())
    .def("assign_to", &PythonPosition::assign_to)
    .def("get_component", &PythonPosition::get_component)
    .staticmethod("get_component")
    .def_readwrite("x", &PythonPosition::x)
    .def_readwrite("y", &PythonPosition::y);
}


class PythonSystemTest : public ::testing::Test {
protected:
  PythonSystemTest() : em(ev) {
    CHECK(PyImport_AppendInittab("entityx_python_test", initentityx_python_test) != -1)
      << "Failed to initialize entityx_python_test Python module";
  }

  void SetUp() {
    vector<string> paths;
    paths.push_back(ENTITYX_PYTHON_TEST_DATA);
    system = make_shared<PythonSystem>(paths);
  }

  void TearDown() {
    system->shutdown();
    system.reset();
  }

  shared_ptr<PythonSystem> system;
  EventManager ev;
  EntityManager em;
};


TEST_F(PythonSystemTest, TestSystemUpdateCallsEntityUpdate) {
  try {
    system->configure(ev);
    Entity e = em.create();
    auto script = e.assign<PythonEntityComponent>("entityx.tests.update_test", "UpdateTest");
    ASSERT_FALSE(py::extract<bool>(script->object.attr("updated")));
    system->update(em, ev, 0.1);
    ASSERT_TRUE(py::extract<bool>(script->object.attr("updated")));
  } catch (...) {
    PyErr_Print();
    PyErr_Clear();
    throw;
  }
}


TEST_F(PythonSystemTest, TestComponentAssignmentCreationInPython) {
  try {
    system->configure(ev);
    Entity e = em.create();
    auto script = e.assign<PythonEntityComponent>("entityx.tests.assign_test", "AssignTest");
    ASSERT_FALSE(e.component<Position>());
    ASSERT_TRUE(script->object);
    ASSERT_TRUE(script->object.attr("test_assign_create"));
    script->object.attr("test_assign_create")();
    auto position = e.component<Position>();
    ASSERT_TRUE(position);
    ASSERT_EQ(position->x, 1.0);
    ASSERT_EQ(position->y, 2.0);
  } catch (...) {
    PyErr_Print();
    PyErr_Clear();
    throw;
  }
}


TEST_F(PythonSystemTest, TestComponentAssignmentCreationInCpp) {
  try {
    system->configure(ev);
    Entity e = em.create();
    e.assign<PythonPosition>(2, 3);
    auto script = e.assign<PythonEntityComponent>("entityx.tests.assign_test", "AssignTest");
    ASSERT_TRUE(e.component<Position>());
    ASSERT_TRUE(script->object);
    ASSERT_TRUE(script->object.attr("test_assign_existing"));
    script->object.attr("test_assign_existing")();
    auto position = e.component<Position>();
    ASSERT_TRUE(position);
    ASSERT_EQ(position->x, 3.0);
    ASSERT_EQ(position->y, 4.0);
  } catch (...) {
    PyErr_Print();
    PyErr_Clear();
    throw;
  }
}


TEST_F(PythonSystemTest, TestEntityConstructorArgs) {
  try {
    system->configure(ev);
    Entity e = em.create();
    auto script = e.assign<PythonEntityComponent>("entityx.tests.constructor_test", "ConstructorTest", 4.0, 5.0);
    auto position = e.component<Position>();
    ASSERT_TRUE(position);
    ASSERT_EQ(position->x, 4.0);
    ASSERT_EQ(position->y, 5.0);
  } catch (...) {
    PyErr_Print();
    PyErr_Clear();
    throw;
  }
}
