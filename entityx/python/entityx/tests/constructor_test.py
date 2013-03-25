from entityx import Entity, Component
from entityx_python_test import Position


class ConstructorTest(Entity):
    position = Component(Position)

    def __init__(self, entity, x, y):
        super(ConstructorTest, self).__init__(entity)
        self.position.x = x
        self.position.y = y
