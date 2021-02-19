#pragma once

#include <engine/Include.h>

struct HopperComponent {
    HopperComponent() = default;
    // Decent gains:
    //double k_gains[2][6] = { 0, 0, 4, 6.782, 0, 0, 0.265, 0.214, 0, 0, -0.814, -0.153 };
    double k_gains[2][6] = { 
    0, 0, 4, 6.782, 0, 0,
    0.231, 0.206, 0, 0, -0.841, -0.16
    };
    double sv[6][1] = { 0,0,0,0,0,0 };

    double thrust = 0;
    double max_thrust = 90.0;
    double com_to_tvc = 0.2;
    double control_angle = 0; // radians.
    V2_double thrust_vector;
    double control_torque;

    void Update(const V2_double& original_position, Body& b) {

        //AHHHHHHHHHHHHHHHHH idk a better way to assing matrix values in the loop
        sv[0][0] = b.position.x - original_position.x;
        sv[1][0] = b.velocity.x;
        sv[2][0] = b.position.y - original_position.y;
        sv[3][0] = b.velocity.y;
        sv[4][0] = -b.orientation;
        sv[5][0] = -b.angular_velocity;

        auto control_vector = ComputeControlVector();
        thrust = engine::math::Clamp(-control_vector.x, 0.0, max_thrust);
        // Clamped between +-15 degrees. == 0.261799 radians
        control_angle = engine::math::Clamp(control_vector.y, -0.261799, 0.261799);
        control_torque = std::sin(control_angle) * thrust * com_to_tvc;
        //LOG("orientation: " << b.orientation << ", ang_vel: " << b.angular_velocity << ", control_angle: " << control_angle << ", control_torque: " << control_torque << ", std::sin(control_angle): " << std::sin(control_angle) << ", thrust: " << thrust << ", com_to_tvc: " << com_to_tvc << ", force: " << b.force);
        //LOG("orientation: " << b.orientation);
        //LOG("ang_vel: " << b.angular_velocity);
        //LOG("control_angle: " << control_angle);

        //LOG("position:" << b.position.x - original_position.x << ", velocity:" << b.velocity.x << ", thrust:" << thrust << ", control_ang:" << control_angle);
        LOG("orientation:" << -b.orientation << ", ang_vel:" << -b.angular_velocity << ", thrust:" << thrust << ", control_ang:" << control_angle);


        thrust_vector = thrust * V2_double{ std::sin(b.orientation + control_angle), -std::cos(b.orientation + control_angle) };

        b.force += thrust_vector;
        //LOG("Control torque: " << control_torque << ", torque: " << b.torque);
        b.torque += control_torque;
    }

    int rowFirst = 2;
    int columnFirst = 6;
    int rowSecond = 6;
    int columnSecond = 1;
    V2_double ComputeControlVector() {
        double mult[2][1] = { 0, 0 };
        // Multiplying matrix firstMatrix and secondMatrix and storing in array mult.
        for (auto i = 0; i < rowFirst; ++i) {
            for (auto j = 0; j < columnSecond; ++j) {
                for (auto k = 0; k < columnFirst; ++k) {
                    mult[i][j] += -1.0 * k_gains[i][k] * sv[k][j];
                }
            }
        }
        return { mult[0][0], mult[1][0] };
    }
};