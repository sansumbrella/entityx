from entityx import Entity, Component
from entityx_python_test import Position, Direction


class BaseEntity(Entity):
    direction = Component(Direction)


class DeepSubclassTest(BaseEntity):
    position = Component(Position)

    def test_deep_subclass(self):
        assert self.direction
        assert self.position
