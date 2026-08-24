#!/usr/bin/env python3

import argparse
import cmath
import json
import math
from typing import Iterable


TWO_PI = 2.0 * math.pi
UNIT_ROUNDOFF_BINARY64 = 2.0**-53
UNIT_ROUNDOFF_BINARY32 = 2.0**-24


def parse_revolutions(value: str) -> list[int]:
    revolutions = [int(item) for item in value.split(",")]
    if not revolutions or any(item < 1 for item in revolutions):
        raise argparse.ArgumentTypeError("revolutions must be positive comma-separated integers")
    return revolutions


def complex_power(value: complex, exponent: int) -> complex:
    if value == 0.0j:
        return 1.0 + 0.0j if exponent == 0 else 0.0j
    log_magnitude = exponent * math.log(abs(value))
    if log_magnitude < math.log(float.fromhex("0x1p-1022")):
        return 0.0j
    magnitude = math.exp(log_magnitude)
    phase = math.remainder(exponent * cmath.phase(value), TWO_PI)
    return cmath.rect(magnitude, phase)


def adams_bashforth_two_roots(step_angle: float) -> tuple[complex, complex]:
    z = 1j * step_angle
    coefficient = 1.0 + 1.5 * z
    discriminant = coefficient * coefficient - 2.0 * z
    first = 0.5 * (coefficient + cmath.sqrt(discriminant))
    second = 0.5 * (coefficient - cmath.sqrt(discriminant))
    exact_step = cmath.exp(z)
    if abs(first - exact_step) <= abs(second - exact_step):
        return first, second
    return second, first


def adams_bashforth_two_model(
    frames_per_revolution: int,
    revolutions: Iterable[int],
) -> dict[str, object]:
    step_angle = TWO_PI / frames_per_revolution
    principal_root, parasitic_root = adams_bashforth_two_roots(step_angle)
    exact_first_step = cmath.exp(1j * step_angle)
    principal_weight = (exact_first_step - parasitic_root) / (principal_root - parasitic_root)
    parasitic_weight = 1.0 - principal_weight
    phase_error_per_step = cmath.phase(principal_root) - step_angle
    log_gain_per_step = math.log(abs(principal_root))
    roundoff_per_step = 64.0 * UNIT_ROUNDOFF_BINARY64
    endpoints = []
    for revolution_count in revolutions:
        frame_count = frames_per_revolution * revolution_count
        numerical = (
            principal_weight * complex_power(principal_root, frame_count)
            + parasitic_weight * complex_power(parasitic_root, frame_count)
        )
        endpoints.append(
            {
                "revolutions": revolution_count,
                "frames": frame_count,
                "amplitude": abs(numerical),
                "amplitude_error": abs(numerical) - 1.0,
                "wrapped_phase_error_radians": cmath.phase(numerical),
                "unwrapped_principal_phase_error_radians":
                    frame_count * phase_error_per_step + cmath.phase(principal_weight),
                "complex_state_error": abs(numerical - 1.0),
            }
        )

    projected_phase_bound = (
        frames_per_revolution * abs(phase_error_per_step)
        + frames_per_revolution * roundoff_per_step
    )
    return {
        "recurrence": "u[n+1]=(1+3*i*x/2)*u[n]-(i*x/2)*u[n-1]",
        "step_angle_radians": step_angle,
        "principal_root": {
            "real": principal_root.real,
            "imag": principal_root.imag,
            "magnitude": abs(principal_root),
            "phase_radians": cmath.phase(principal_root),
        },
        "parasitic_root_magnitude": abs(parasitic_root),
        "log_gain_per_step": log_gain_per_step,
        "log_gain_per_revolution": frames_per_revolution * log_gain_per_step,
        "phase_error_per_step_radians": phase_error_per_step,
        "phase_error_per_revolution_radians": frames_per_revolution * phase_error_per_step,
        "small_step_model": {
            "log_gain_per_step": "x^4/4 + O(x^6)",
            "phase_error_per_step": "5*x^3/12 + O(x^5)",
        },
        "endpoints": endpoints,
        "projection_only": {
            "amplitude_is_bounded": True,
            "phase_is_uniformly_bounded": False,
            "reason": "normalization removes gain drift but phase defect accumulates each step",
        },
        "projection_plus_reanchor_each_revolution": {
            "amplitude_is_bounded": True,
            "phase_is_uniformly_bounded": True,
            "uniform_phase_bound_radians": projected_phase_bound,
            "bound_model": "N*(abs(arg(r)-x)+64*u64)",
        },
    }


def phase_locked_model(frames_per_revolution: int) -> dict[str, object]:
    double_evaluation_budget = 128.0 * UNIT_ROUNDOFF_BINARY64
    vector_float_conversion_bound = math.sqrt(3.0) * UNIT_ROUNDOFF_BINARY32
    basis_float_conversion_bound = math.sqrt(6.0) * UNIT_ROUNDOFF_BINARY32
    vector_error_norm_bound = double_evaluation_budget + vector_float_conversion_bound
    return {
        "index_dynamics": "k[n+1]=(k[n]+1) mod N",
        "orientation": "B[n]=B0*R(axis,2*pi*k[n]/N)",
        "periodicity": "B[n+N]=B[n]",
        "error_dynamics": "e[n]=q[k[n]], so e[n+N]=e[n]",
        "frames_per_revolution": frames_per_revolution,
        "binary64_evaluation_budget": double_evaluation_budget,
        "binary32_vector_conversion_bound": vector_float_conversion_bound,
        "binary32_basis_conversion_bound": basis_float_conversion_bound,
        "conservative_vector_error_norm_bound": vector_error_norm_bound,
        "conservative_vector_angular_bound_radians":
            2.0 * math.asin(min(1.0, 0.5 * vector_error_norm_bound)),
        "conservative_basis_frobenius_bound":
            double_evaluation_budget + basis_float_conversion_bound,
        "scope": "uniform while the pointer and orbit anchor remain unchanged",
    }


def bounded_adams_bashforth_two_model(
    phase_locked: dict[str, object],
    adams_bashforth_two: dict[str, object],
    correction_gain: float,
    error_cap_radians: float,
) -> dict[str, object]:
    principal_root = adams_bashforth_two["principal_root"]
    root = complex(principal_root["real"], principal_root["imag"])
    step_angle = adams_bashforth_two["step_angle_radians"]
    local_principal_defect = abs(root - cmath.exp(1j * step_angle))
    roundoff_disturbance = 64.0 * UNIT_ROUNDOFF_BINARY64
    predictor_gain = abs(root)
    closed_loop_gain = (1.0 - correction_gain) * predictor_gain
    disturbance = local_principal_defect + roundoff_disturbance
    iss_bound = math.inf
    if closed_loop_gain < 1.0:
        iss_bound = (1.0 - correction_gain) * disturbance / (1.0 - closed_loop_gain)
    return {
        "predictor": "Adams-Bashforth 2 quaternion prediction",
        "projection": "normalize the predicted quaternion",
        "independent_reference": "anchor*axis_angle(2*pi*k/N), independent of AB history",
        "contractive_correction": "shortest-path quaternion SLERP toward the reference",
        "correction_gain": correction_gain,
        "predictor_gain_model": predictor_gain,
        "closed_loop_gain_model": closed_loop_gain,
        "local_principal_defect": local_principal_defect,
        "disturbance_budget": disturbance,
        "iss_reference_error_bound_radians": iss_bound,
        "runtime_error_cap_radians": error_cap_radians,
        "fail_closed_rule": "if corrected error exceeds the cap, replace state and AB history from the reference",
        "periodic_reanchor": "replace state and previous AB history at every revolution boundary",
        "total_error_bound_including_reference_radians":
            error_cap_radians + phase_locked["conservative_vector_angular_bound_radians"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare HexPuzzle's phase-locked orbit with Adams-Bashforth 2 error dynamics."
    )
    parser.add_argument("--frames-per-revolution", type=int, default=3606)
    parser.add_argument(
        "--revolutions",
        type=parse_revolutions,
        default=parse_revolutions("1,1000,1000000"),
    )
    parser.add_argument("--correction-gain", type=float, default=0.25)
    parser.add_argument("--error-cap-radians", type=float, default=0.000001)
    arguments = parser.parse_args()
    if arguments.frames_per_revolution < 2:
        parser.error("frames per revolution must be at least 2")
    if not 0.0 < arguments.correction_gain <= 1.0:
        parser.error("correction gain must be in (0, 1]")
    if not 0.0 < arguments.error_cap_radians < math.pi:
        parser.error("error cap must be in (0, pi)")

    phase_locked = phase_locked_model(arguments.frames_per_revolution)
    adams_bashforth_two = adams_bashforth_two_model(
        arguments.frames_per_revolution,
        arguments.revolutions,
    )
    result = {
        "schema": "hexpuzzle.orbit_error_model.v1",
        "parameters": {
            "frames_per_revolution": arguments.frames_per_revolution,
            "revolutions": arguments.revolutions,
        },
        "phase_locked_closed_form": phase_locked,
        "adams_bashforth_two": adams_bashforth_two,
        "bounded_adams_bashforth_two": bounded_adams_bashforth_two_model(
            phase_locked,
            adams_bashforth_two,
            arguments.correction_gain,
            arguments.error_cap_radians,
        ),
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
