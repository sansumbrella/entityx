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

`entityx::python` primarily uses standard `boost::python` to interface with Python, with some helper classes.

### Exposing C++ Components to Python

To expose `Component`s to Python you will need to subclass the raw component and conform to the following:

1. Subclass from `enable_shared_from_this<PythonPosition>`.
2. Implement `void assign_to(Entity &entity)`. This is called from Python to assign a newly constructed component to an entity.
3. Implement `static shared_ptr<PythonPosition> get_component(Entity &entity)`. This is called from Python to retrieve an existing component from an entity.
4. Expose the class and its `assign_to` and `get_component` methods to Python, in addition to any other attributes (`x` and `y` in this example).


```
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

Initialize the module once, before using `PythonSystem`, with something like this:

```
CHECK(PyImport_AppendInittab("mygame", initmygame) != -1)
  << "Failed to initialize mygame Python module";

```

### Delivering events to Python entities

Unlike in C++, where events are typically handled by systems, in Python
entities receive events. To bridge this gap, EntityX::Python provides a
`PythonEventProxy` class that receives C++ events and proxies them to
Python entities that can receive the event, as determined by the virtual
method `PythonEventProxy::can_send()` method.

A helper template class called `BroadcastPythonEventProxy<Event>` is provided
that will broadcast events to any entity with the corresponding handler method.

To implement more refined logic, subclass `PythonEventProxy` and operate on
the protected member `entities`. Here's a collision example:

```
struct CollisionEvent : public Event<CollisionEvent> {
  CollisionEvent(Entity a, Entity b) : a(a), b(b) {}

  Entity a, b;
};

struct CollisionEventProxy : public PythonEventProxy, public Receiver<CollisionEvent> {
  CollisionEventProxy() : PythonEventProxy("on_collision") {}

  void receive(const CollisionEvent &event) {
    // "entities" is a protected data member populated by
    // PythonSystem with Python entities that pass can_send().
    for (auto entity : entities) {
      auto py_entity = entity.template component<PythonEntityComponent>();
      if (entity == event.a || entity == event.b) {
        py_entity->object.attr(handler_name.c_str())(event);
      }
    }
  }
};

BOOST_PYTHON_MODULE(mygame) {
  py::class_<CollisionEvent>("Collision", py::init<Entity, Entity>())
    .def_readonly("a", &CollisionEvent::a)
    .def_readonly("b", &CollisionEvent::b);
}
```

Finally,

```
pysystem->add_event_proxy<CollisionEvent>(ev, boost::make_shared<CollisionEventProxy>());
```
