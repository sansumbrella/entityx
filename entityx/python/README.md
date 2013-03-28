# Python Scripting System for EntityX

This system adds the ability to extend entity logic with Python scripts. The goal is to allow ad-hoc behaviour to be assigned to entities, in contract to the more pure entity-component system approach.

## Concepts

- Python scripts are attached to entities with `PythonEntityComponent`.
- Events are proxied directly to script methods through `PythonEventProxy` objects.
- `PythonSystem` manages scripted entity lifecycle and event delivery.

## Overview

To add scripting support to your system, something like the following steps should be followed:

1. Expose C++ `Component` and `Event` classes to Python with `BOOST_PYTHON_MODULE`.
2. Initialize the module with `PyImport_AppendInittab`.
3. Create a Python package.
4. Add classes to the package, inheriting from `entityx.Entity` and using the `entityx.Component` descriptor to assign components.
5. Create a `PythonSystem`, passing in the list of paths to add to Python's import search path.
6. Optionally attach any event proxies
4. Create an entity and associate it with a Python script by assigning `PythonEntityComponent`, passing it the package name, class name, and any constructor arguments.

## Interfacing with Python

Use standard `boost::python` to achieve this, with some caveats.

### Exposing C++ `Component` implementations to Python

To expose `Component`s to Python you will need to subclass the raw component and follow this pattern:

```c++
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

BOOST_PYTHON_MODULE(mygame) {
  py::class_<PythonPosition, shared_ptr<PythonPosition>>("Position", py::init<py::optional<float, float>>())
    .def("assign_to", &PythonPosition::assign_to)
    .def("get_component", &PythonPosition::get_component)
    .staticmethod("get_component")
    .def_readwrite("x", &PythonPosition::x)
    .def_readwrite("y", &PythonPosition::y);
}
```

Note that `PythonPosition` must:

1. Subclass from `enable_shared_from_this<PythonPosition>`.
2. Implement `void assign_to(Entity &entity)`. This is called from Python to assign a newly constructed component to an entity.
3. Implement `static shared_ptr<PythonPosition> get_component(Entity &entity)`. This is called from Python to retrieve an existing component from an entity.
4. Expose the class and its `assign_to` and `get_component` methods to Python, in addition to any other attributes (`x` and `y` in this example).
