from _entityx import _Entity


"""These classes provide a convenience layer on top of the raw entityx::python
primitives.

They allow you to declare your entities and components in an intuitive way:

    class Player(Entity):
        position = Component(Position)
        direction = Component(Direction)
        sprite = Component(Sprite, 'player.png')

        def update(self, dt):
            self.position.x += self.direction.x * dt
            self.position.x += self.direction.y * dt

Note that components assigned from C++ must be assigned prior to assigning
PythonComponent, otherwise they will be created by the Entity constructor.
"""


__all__ = ['Entity', 'Component']


class Component(object):
    """A field that manages Component creation/retrieval.

    Use like so:

    class Player(Entity):
        position = Component(Position)

        def move_to(self, x, y):
            self.position.x = x
            self.position.y = y
    """
    def __init__(self, cls, *args, **kwargs):
        self._cls = cls
        self._args = args
        self._kwargs = kwargs

    def _build(self, entity):
        component = self._cls.get_component(entity)
        if not component:
            component = self._cls(*self._args, **self._kwargs)
            component.assign_to(entity)
        return component


class EntityMetaClass(_Entity.__class__):
    """Collect registered components from class attributes.

    This is done at class creation time to reduce entity creation overhead.
    """

    def __new__(cls, name, bases, dct):
        dct['_components'] = components = {}
        # Collect components from base classes
        for base in bases:
            if '_components' in base.__dict__:
                components.update(base.__dict__['_components'])
        # Collect components
        for key, value in dct.items():
            if isinstance(value, Component):
                components[key] = value
        return type.__new__(cls, name, bases, dct)


class Entity(_Entity):
    """Base Entity class.

    Python Enitities differ in semantics from C++ components, in that they
    contain logic.
    """

    __metaclass__ = EntityMetaClass

    def __init__(self, entity):
        super(Entity, self).__init__(entity)
        # Retrieve or create all components associated with the entity.
        for k, v in self._components.items():
            setattr(self, k, v._build(self._entity))

    def update(self, dt):
        """Update method called each game tick."""
