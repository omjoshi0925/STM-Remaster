// Character.hpp - locomotion + animation state machine driving a bdae::Model
// through the original Total Mayhem clip set.
#pragma once
#include "BDAEModel.hpp"
#include "Level.hpp"

namespace bdae {

enum class Locomotion { Idle, Walk, Run };

class Character {
public:
    // Authored clip names taken straight from spiderman_anim.bdae.
    bool bind(Model* model, const LevelRoom* room);
    void spawnAt(const Vec3& p, float yaw);

    // moveX/moveY: desired world-space movement direction, magnitude 0..1.
    void update(float dt, float moveX, float moveY);

    Vec3  position() const { return pos_; }
    float yaw() const { return yaw_; }
    Locomotion state() const { return state_; }
    const char* stateName() const;
    bool  grounded() const { return grounded_; }
    float speed() const { return speed_; }

    // Tunables, in original asset units (1 unit ~= 1 cm; Spider-Man is 174 tall).
    float walkSpeed = 170.0f;
    float runSpeed  = 520.0f;
    float turnRate  = 9.0f;

private:
    Model* model_ = nullptr;
    const LevelRoom* room_ = nullptr;
    const Clip* idle_ = nullptr;
    const Clip* walk_ = nullptr;
    const Clip* run_  = nullptr;
    ClipPlayer player_;
    ClipPlayer previous_;
    float blend_ = 1.0f;          // 0 = fully previous clip, 1 = fully current
    Locomotion state_ = Locomotion::Idle;
    Vec3  pos_{0, 0, 0};
    float yaw_ = 0, speed_ = 0;
    bool  grounded_ = false;
    void setState(Locomotion s);
};

} // namespace bdae
