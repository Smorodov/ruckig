#pragma once

#include <array>
#include <limits>
#include <optional>

#include <ruckig/trajectory.hpp>


namespace ruckig {

//! Which times are possible for synchronization?
struct Block {
    struct Interval {
        double left, right; // [s]
        Profile profile; // Profile corresponding to right (end) time

        explicit Interval(double left, double right, const Profile& profile): left(left), right(right), profile(profile) { };
    };

    Profile p_min; // Save min profile so that it doesn't need to be recalculated in Step2
    double t_min; // [s]

    // Max. 2 intervals can be blocked: called a and b with corresponding profiles, order does not matter
    std::optional<Interval> a, b;

    explicit Block() { }
    explicit Block(const Profile& p_min): p_min(p_min), t_min(p_min.t_sum[6] + p_min.t_brake.value_or(0.0)) { }

    bool is_blocked(double t) const {
        return (t < t_min) || (a && a->left < t && t < a->right) || (b && b->left < t && t < b->right);
    }
};


//! Calculates (pre-) trajectory to get current state below the limits
class Brake {
    static constexpr double eps {2e-14};

    static void acceleration_brake(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax, std::array<double, 2>& t_brake, std::array<double, 2>& j_brake);
    static void velocity_brake(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax, std::array<double, 2>& t_brake, std::array<double, 2>& j_brake);

public:
    static void get_brake_trajectory(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax, std::array<double, 2>& t_brake, std::array<double, 2>& j_brake);
};


//! Mathematical equations for Step 1: Extremal profiles
class Step1 {
    using Limits = Profile::Limits;
    using JerkSigns = Profile::JerkSigns;

    double p0, v0, a0;
    double pf, vf, af;
    double vMax, vMin, aMax, aMin, jMax;

    // Pre-calculated expressions
    double pd;
    double v0_v0, vf_vf;
    double a0_a0, a0_p3, a0_p4, a0_p5, a0_p6;
    double af_af, af_p3, af_p4, af_p5, af_p6;
    double jMax_jMax;

    // Only a single velocity-limited profile can be valid
    bool has_up_vel {false}, has_down_vel {false};

    // Max 5 valid profiles
    std::array<Profile, 5> valid_profiles;
    size_t valid_profile_counter;

    void time_acc0_acc1_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    void time_acc1_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    void time_acc0_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    void time_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    void time_acc0_acc1(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    void time_acc1(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    void time_acc0(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    void time_none(Profile& profile, double vMax, double aMax, double aMin, double jMax);

    inline void add_profile(Profile profile, double jMax) {
        profile.direction = (jMax > 0) ? Profile::Direction::UP : Profile::Direction::DOWN;

        if (profile.limits == Limits::ACC0_ACC1_VEL || profile.limits == Limits::ACC0_VEL || profile.limits == Limits::ACC1_VEL || profile.limits == Limits::VEL) {
            switch (profile.direction) {
                case Profile::Direction::UP: has_up_vel = true; break;
                case Profile::Direction::DOWN: has_down_vel = true; break;
            }
        }

        valid_profiles[valid_profile_counter] = profile;
        ++valid_profile_counter;
    }

    inline void add_interval(std::optional<Block::Interval>& interval, size_t left, size_t right) const {
        const double left_duration = valid_profiles[left].t_sum[6] + valid_profiles[left].t_brake.value_or(0.0);
        const double right_duraction = valid_profiles[right].t_sum[6] + valid_profiles[right].t_brake.value_or(0.0);
        if (left_duration < right_duraction) {
            interval = Block::Interval(left_duration, right_duraction, valid_profiles[right]);
        } else {
            interval = Block::Interval(right_duraction, left_duration, valid_profiles[left]);
        }
    }

    bool calculate_block(Block& block) const;

public:
    explicit Step1(double p0, double v0, double a0, double pf, double vf, double af, double vMax, double vMin, double aMax, double aMin, double jMax);

    bool get_profile(const Profile& input, Block& block);
};


//! Mathematical equations for Step 2: Time synchronization
class Step2 {
    using Limits = Profile::Limits;
    using JerkSigns = Profile::JerkSigns;

    double tf;
    double p0, v0, a0;
    double pf, vf, af;
    double vMax, vMin, aMax, aMin, jMax;

    // Pre-calculated expressions
    double pd;
    double tf_tf, tf_p3, tf_p4;
    double vd, vd_vd;
    double ad, ad_ad;
    double v0_v0, vf_vf;
    double a0_a0, a0_p3, a0_p4, a0_p5, a0_p6;
    double af_af, af_p3, af_p4, af_p5, af_p6;
    double jMax_jMax;
    double g1, g2;

    bool time_acc0_acc1_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    bool time_acc1_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    bool time_acc0_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    bool time_vel(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    bool time_acc0_acc1(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    bool time_acc1(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    bool time_acc0(Profile& profile, double vMax, double aMax, double aMin, double jMax);
    bool time_none(Profile& profile, double vMax, double aMax, double aMin, double jMax);

    inline bool check_all(Profile& profile, double vMax, double aMax, double aMin, double jMax) {
        return time_acc0_acc1_vel(profile, vMax, aMax, aMin, jMax)
            || time_acc0_vel(profile, vMax, aMax, aMin, jMax)
            || time_acc1_vel(profile, vMax, aMax, aMin, jMax)
            || time_vel(profile, vMax, aMax, aMin, jMax)
            || time_acc0(profile, vMax, aMax, aMin, jMax)
            || time_acc1(profile, vMax, aMax, aMin, jMax)
            || time_acc0_acc1(profile, vMax, aMax, aMin, jMax)
            || time_none(profile, vMax, aMax, aMin, jMax);
    }

public:
    explicit Step2(double tf, double p0, double v0, double a0, double pf, double vf, double af, double vMax, double vMin, double aMax, double aMin, double jMax);

    bool get_profile(Profile& profile);
};

} // namespace ruckig
