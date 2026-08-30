#include "Character.hpp"
#include <cmath>
#include <algorithm>

namespace bdae {

bool Character::bind(Model* model, const LevelRoom* room) {
    model_ = model; room_ = room;
    idle_ = model->findClip("idle_stand");
    walk_ = model->findClip("walk");
    run_  = model->findClip("run");
    if (!idle_) return false;
    player_.play(idle_);
    return true;
}

void Character::spawnAt(const Vec3& p, float yaw) {
    pos_ = p; yaw_ = yaw;
    Vec3 snapped;
    if (room_ && room_->nearestWalkable(p.x, p.y, 1200.0f, snapped)) { pos_ = snapped; grounded_ = true; }
}

const char* Character::stateName() const {
    switch (state_) {
        case Locomotion::Idle: return "idle_stand";
        case Locomotion::Walk: return "walk";
        default:               return "run";
    }
}

void Character::setState(Locomotion s) {
    if (s == state_) return;
    const Clip* next = (s == Locomotion::Idle) ? idle_ : (s == Locomotion::Walk ? walk_ : run_);
    if (!next) return;
    previous_ = player_;
    player_.play(next);
    blend_ = 0.0f;
    state_ = s;
}

void Character::update(float dt, float moveX, float moveY) {
    float mag = std::sqrt(moveX * moveX + moveY * moveY);
    if (mag > 1.0f) { moveX /= mag; moveY /= mag; mag = 1.0f; }

    if (mag < 0.05f)      setState(Locomotion::Idle);
    else if (mag < 0.55f) setState(Locomotion::Walk);
    else                  setState(Locomotion::Run);

    speed_ = (state_ == Locomotion::Idle) ? 0.0f
           : (state_ == Locomotion::Walk ? walkSpeed * (mag / 0.55f) : runSpeed * mag);

    if (mag > 0.05f) {
        float want = std::atan2(moveY, moveX);
        float d = want - yaw_;
        while (d >  3.14159265f) d -= 6.28318531f;
        while (d < -3.14159265f) d += 6.28318531f;
        yaw_ += d * std::min(1.0f, turnRate * dt);

        float nx = pos_.x + std::cos(yaw_) * speed_ * dt;
        float ny = pos_.y + std::sin(yaw_) * speed_ * dt;
        float z;
        if (room_ && room_->canStandAt(nx, ny, z)) { pos_.x = nx; pos_.y = ny; pos_.z = z; grounded_ = true; }
        else if (room_ && room_->canStandAt(nx, pos_.y, z)) { pos_.x = nx; pos_.z = z; }  // slide along X
        else if (room_ && room_->canStandAt(pos_.x, ny, z)) { pos_.y = ny; pos_.z = z; }  // slide along Y
        else if (!room_) { pos_.x = nx; pos_.y = ny; }
    }

    player_.advance(dt);
    if (blend_ < 1.0f) { previous_.advance(dt); blend_ = std::min(1.0f, blend_ + dt * 6.0f); }
    if (model_) {
        if (blend_ >= 1.0f || !previous_.clip) model_->poseAtTime(player_.timelineMs());
        else model_->poseBlend(previous_.timelineMs(), player_.timelineMs(), blend_);
    }
}

} // namespace bdae
