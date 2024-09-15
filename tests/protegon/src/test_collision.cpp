#include "protegon/protegon.h"

using namespace ptgn;

class CollisionTest : public Scene {
 public:
  V2_float position1{200, 200};
  V2_float position3{300, 300};
  V2_float position4{200, 300};

  V2_float size1{130, 130};
  V2_float size2{30, 30};

  float radius1{30};
  float radius2{20};

  Color color1{color::Green};
  Color color2{color::Blue};

  int options{9};
  int option{0};

  int type{2};
  int types{3};

  CollisionTest() {
    game.window.SetTitle("'t'=shape type, 'g'=mode, 'r'=line origin");
    game.window.SetSize({600, 600});
  }

  void Update(float dt) final {
    auto mouse = game.input.GetMousePosition();

    if (game.input.KeyDown(Key::T)) {
      option++;
      option = option++ % options;
    }

    if (game.input.KeyDown(Key::G)) {
      type++;
      type = type++ % types;
    }

    if (game.input.KeyDown(Key::R)) {
      position4 = mouse;
    }

    V2_float position2 = mouse;

    auto acolor1 = color1;
    auto acolor2 = color2;

    Rectangle<float> aabb1{position1, size1};
    Rectangle<float> aabb2{position2, size2};

    Circle<float> circle1{position1, radius1};
    Circle<float> circle2{position2, radius2};

    Segment<float> line1{position1, position3};
    Segment<float> line2{position2, position4};

    if (type == 0) {  // overlap
      options = 9;
      if (option == 0) {
        if (game.collision.overlap.PointSegment(position2, line1)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawLine(line1.a, line2.b, acolor1);
        game.renderer.DrawPoint(position2, acolor2);
      } else if (option == 1) {
        if (game.collision.overlap.PointCircle(position2, circle1)) {
          bool test = game.collision.overlap.PointCircle(position2, circle1);
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawCircleHollow(circle1.center, circle1.radius, acolor1);
        game.renderer.DrawPoint(position2, acolor2);
      } else if (option == 2) {
        if (game.collision.overlap.PointRectangle(position2, aabb1)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
        game.renderer.DrawPoint(position2, acolor2);
      } else if (option == 3) {
        if (game.collision.overlap.SegmentSegment(line2, line1)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawLine(line1.a, line1.b, acolor1);
        game.renderer.DrawLine(line2.a, line2.b, acolor2);
      } else if (option == 4) {
        if (game.collision.overlap.SegmentCircle(line2, circle1)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawLine(line2.a, line2.b, acolor2);
        game.renderer.DrawCircleHollow(circle1.center, circle1.radius, acolor1);
      } else if (option == 5) {
        if (game.collision.overlap.SegmentRectangle(line2, aabb1)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawLine(line2.a, line2.b, acolor2);
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
      } else if (option == 6) {
        if (game.collision.overlap.CircleCircle(circle2, circle1)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor2);
        game.renderer.DrawCircleHollow(circle1.center, circle1.radius, acolor1);
      } else if (option == 7) {
        if (game.collision.overlap.CircleRectangle(circle2, aabb1)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor2);
      } else if (option == 8) {
        aabb2.pos = mouse;
        if (game.collision.overlap.RectangleRectangle(aabb1, aabb2)) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb2.pos, aabb2.size, acolor2);
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
      }
    } else if (type == 1) {  // intersect
      options = 3;
      const float slop{0.005f};
      game.collision.intersect.Collision c;
      if (option == 0) {
        // circle2.center = circle1.center;
        bool occured{game.collision.intersect.CircleCircle(circle2, circle1, c)};
        if (occured) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor2);
        game.renderer.DrawCircleHollow(circle1.center, circle1.radius, acolor1);
        if (occured) {
          Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
                                   circle2.radius};
          game.renderer.DrawCircleHollow(new_circle.center, new_circle.radius,
                                         color2);
          Segment<float> l{circle2.center, new_circle.center};
          game.renderer.DrawLine(l.a, l.b, color::Gold);
          if (game.collision.overlap.CircleCircle(new_circle, circle1)) {
            occured = game.collision.intersect.CircleCircle(new_circle, circle1, c);
            bool overlap{game.collision.overlap.CircleCircle(new_circle, circle1)};
            if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
            if (occured) PrintLine("Slop insufficient, intersect reoccurs");
          }
        }
      } else if (option == 1) {
        // circle2.center = aabb1.position;
        // circle2.center = aabb1.Center();
        bool occured{game.collision.intersect.CircleRectangle(circle2, aabb1, c)};
        if (occured) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor2);
        if (occured) {
          Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
                                   circle2.radius};
          game.renderer.DrawCircleHollow(new_circle.center, new_circle.radius,
                                         color2);
          Segment<float> l{circle2.center, new_circle.center};
          game.renderer.DrawLine(l.a, l.b, color::Gold);
          if (game.collision.overlap.CircleRectangle(new_circle, aabb1)) {
            occured = game.collision.intersect.CircleRectangle(new_circle, aabb1, c);
            bool overlap{game.collision.overlap.CircleRectangle(new_circle, aabb1)};
            if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
            if (occured) PrintLine("Slop insufficient, intersect reoccurs");
          }
        }
      } else if (option == 2) {
        aabb2.pos = mouse;
        // aabb2.position = aabb1.Center() - aabb2.Half();
        bool occured{game.collision.intersect.RectangleRectangle(aabb2, aabb1, c)};
        if (occured) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb2.pos, aabb2.size, acolor2);
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
        if (occured) {
          Rectangle<float> new_aabb{aabb2.pos + c.normal * (c.depth + slop),
                                    aabb2.size};
          game.renderer.DrawRectangleHollow(new_aabb.pos, new_aabb.size,
                                            color2);
          Segment<float> l{aabb2.Center(), new_aabb.Center()};
          game.renderer.DrawLine(l.a, l.b, color::Gold);
          if (game.collision.overlap.RectangleRectangle(new_aabb, aabb1)) {
            occured = game.collision.intersect.RectangleRectangle(new_aabb, aabb1, c);
            bool overlap{game.collision.overlap.RectangleRectangle(new_aabb, aabb1)};
            if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
            if (occured) PrintLine("Slop insufficient, intersect reoccurs");
          }
        }
      }
    } else if (type == 2) {  // dynamic
      options = 3;
      const float slop{0.005f};
      game.collision.dynamic.Collision c;
      if (option == 0) {
        circle2.center = position4;
        V2_float vel{mouse - circle2.center};
        Circle<float> potential{circle2.center + vel, circle2.radius};
        game.renderer.DrawCircleHollow(potential.center, potential.radius,
                                       color::Grey);
        Segment<float> l{circle2.center, potential.center};
        game.renderer.DrawLine(l.a, l.b, color::Grey);
        bool occured{game.collision.dynamic.CircleRectangle(circle2, vel, aabb1, c)};
        if (occured) {
          Circle<float> swept{circle2.center + vel * c.t, circle2.radius};
          Segment<float> normal{swept.center, swept.center + 50 * c.normal};
          game.renderer.DrawLine(normal.a, normal.b, color::Orange);
          game.renderer.DrawCircleHollow(swept.center, swept.radius,
                                         color::Green);
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor1);
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
      } else if (option == 1) {
        circle2.center = position4;
        V2_float vel{mouse - circle2.center};
        Circle<float> potential{circle2.center + vel, circle2.radius};
        game.renderer.DrawCircleHollow(potential.center, potential.radius,
                                       color::Grey);
        Segment<float> l{circle2.center, potential.center};
        game.renderer.DrawLine(l.a, l.b, color::Grey);
        bool occured{game.collision.dynamic.CircleCircle(circle2, vel, circle1, c)};
        if (occured) {
          Circle<float> swept{circle2.center + vel * c.t, circle2.radius};
          Segment<float> normal{swept.center, swept.center + 50 * c.normal};
          game.renderer.DrawLine(normal.a, normal.b, color::Orange);
          game.renderer.DrawCircleHollow(swept.center, swept.radius,
                                         color::Green);
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor1);
        game.renderer.DrawCircleHollow(circle1.center, circle1.radius, acolor1);
      } else if (option == 2) {
        aabb2.pos = position4;
        V2_float vel{mouse - aabb2.pos};
        Rectangle<float> potential{aabb2.pos + vel, aabb2.size};
        game.renderer.DrawRectangleHollow(potential.pos, potential.size,
                                          color::Grey);
        Segment<float> l{aabb2.Center(), potential.Center()};
        game.renderer.DrawLine(l.a, l.b, color::Grey);
        bool occured{game.collision.dynamic.RectangleRectangle(aabb2, vel, aabb1, c)};
        if (occured) {
          Rectangle<float> swept{aabb2.pos + vel * c.t, aabb2.size};
          Segment<float> normal{swept.Center(), swept.Center() + 50 * c.normal};
          game.renderer.DrawLine(normal.a, normal.b, color::Orange);
          game.renderer.DrawRectangleHollow(swept.pos, swept.size,
                                            color::Green);
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb2.pos, aabb2.size, acolor1);
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
      }

      /*
      if (option == 0) {
        // circle2.center = circle1.center;
        int occured{game.collision.dynamic.SegmentCircle(line2, circle1, c)};
        if (occured) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawLine(line2.a, line2.b, acolor2);
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor2);
        game.renderer.DrawCircleHollow(circle1.center, circle1.radius, acolor1);
        if (occured) {
          Circle<float> new_circle{circle2.center + line2.Direction() * c.t,
                                   circle2.radius};
          game.renderer.DrawCircleHollow(new_circle.center, new_circle.radius,
                                         acolor2);
        }
      }
      if (option == 0) {
        // circle2.center = circle1.center;
        bool occured{game.collision.dynamic.CircleCircle(circle2, circle1, c)};
        if (occured) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor2);
        game.renderer.DrawCircleHollow(circle1.center, circle1.radius, acolor1);
        if (occured) {
          Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
                                   circle2.radius};
          game.renderer.DrawCircleHollow(new_circle.center, new_circle.radius,
                                         color2);
          Segment<float> l{circle2.center, new_circle.center};
          game.renderer.DrawLine(l.a, l.b, color::Gold);
          if (game.collision.overlap.CircleCircle(new_circle, circle1)) {
            occured = game.collision.intersect.CircleCircle(new_circle, circle1, c);
            bool overlap{game.collision.overlap.CircleCircle(new_circle, circle1)};
            if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
            if (occured) PrintLine("Slop insufficient, intersect reoccurs");
          }
        }
      } else if (option == 1) {
        // circle2.center = aabb1.position;
        // circle2.center = aabb1.Center();
        bool occured{game.collision.intersect.CircleRectangle(circle2, aabb1, c)};
        if (occured) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
        game.renderer.DrawCircleHollow(circle2.center, circle2.radius, acolor2);
        if (occured) {
          Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
                                   circle2.radius};
          game.renderer.DrawCircleHollow(new_circle.center, new_circle.radius,
                                         color2);
          Segment<float> l{circle2.center, new_circle.center};
          game.renderer.DrawLine(l.a, l.b, color::Gold);
          if (game.collision.overlap.CircleRectangle(new_circle, aabb1)) {
            occured = game.collision.intersect.CircleRectangle(new_circle, aabb1, c);
            bool overlap{game.collision.overlap.CircleRectangle(new_circle, aabb1)};
            if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
            if (occured) PrintLine("Slop insufficient, intersect reoccurs");
          }
        }
      } else if (option == 2) {
        aabb2.pos = mouse - aabb2.Half();
        // aabb2.position = aabb1.Center() - aabb2.Half();
        bool occured{game.collision.intersect.RectangleRectangle(aabb2, aabb1, c)};
        if (occured) {
          acolor1 = color::Red;
          acolor2 = color::Red;
        }
        game.renderer.DrawRectangleHollow(aabb2.pos, aabb2.size, acolor2);
        game.renderer.DrawRectangleHollow(aabb1.pos, aabb1.size, acolor1);
        if (occured) {
          Rectangle<float> new_aabb{aabb2.pos + c.normal * (c.depth + slop),
                                    aabb2.size};
          game.renderer.DrawRectangleHollow(new_aabb.pos, new_aabb.size,
      color2); Segment<float> l{aabb2.Center(), new_aabb.Center()};
          game.renderer.DrawLine(l.a, l.b, color::Gold);
          if (game.collision.overlap.RectangleRectangle(new_aabb, aabb1)) {
            occured = game.collision.intersect.RectangleRectangle(new_aabb, aabb1, c);
            bool overlap{game.collision.overlap.RectangleRectangle(new_aabb, aabb1)};
            if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
            if (occured) PrintLine("Slop insufficient, intersect reoccurs");
          }
        }
      }*/
    }
  }
};

int main(int c, char** v) {
  game.Start<CollisionTest>();
  return 0;
}