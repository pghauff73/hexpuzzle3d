# Adam Bashforth Bounded Orbit

## Scope

`BoundedAdamsBashforthOrbit` implements an error-bounded second-order
Adams-Bashforth rotation update for the stationary-pointer camera orbit.
“Adam Bashford” is treated as a reference to the Adams-Bashforth family of
explicit linear multistep methods.

The design combines all four controls needed for an indefinite conservative
orbit:

1. an AB2 predictor;
2. projection onto the unit-quaternion rotation group;
3. contractive correction toward an independent phase reference; and
4. periodic and fail-closed re-anchoring of both state and AB history.

## State and Independent Reference

Let `q[n]` be the unit quaternion that maps the local camera frame into world
space. For a stationary pointer, the camera fixes a unit world axis `a` and an
integer number of frames per revolution `N`. The angular-velocity quaternion is

```text
Omega = (0, a*2*pi/N),
```

and quaternion dynamics are

```text
q' = 0.5*Omega*q.
```

The independent reference does not use AB history:

```text
k[n+1] = (k[n] + 1) mod N
q_ref[n] = axis_angle(a, 2*pi*k[n]/N) * q_anchor.
```

Quaternion sign parity is tracked across revolutions so the internal quaternion
history remains continuous even though `q` and `-q` represent the same physical
orientation.

## Bounded AB2 Update

### Predictor

With one rendered frame as the time step, AB2 predicts

```text
q_raw[n+1] = q[n] + 1.5*f(q[n]) - 0.5*f(q[n-1]),
f(q) = 0.5*Omega*q.
```

### Projection

The raw quaternion is projected back onto the unit sphere:

```text
q_projected = q_raw / ||q_raw||.
```

All camera basis vectors are generated from this unit quaternion and then
orthogonalized, so norm and frame-shape drift are bounded independently of AB2
amplitude gain.

### Contractive Correction

The projected prediction is corrected with shortest-path quaternion SLERP:

```text
q_corrected = slerp(q_projected, q_ref, g),
g = 0.25.
```

For geodesic orientation distance `d` away from the quaternion antipode,

```text
d(q_corrected, q_ref) = (1-g)*d(q_projected, q_ref).
```

Thus the implemented correction contracts prediction error by `0.75` each
normal frame.

### Fail-Closed Error Cap

The corrected quaternion is compared with the independent reference using a
small-angle-stable quaternion chord metric. The configured cap is

```text
E_cap = 1e-6 radians.
```

If

```text
d(q_corrected, q_ref) > E_cap,
```

the prediction is rejected. The orientation and the previous AB state are both
reconstructed from the independent reference. Consequently the runtime
reference error satisfies

```text
d(q[n], q_ref[n]) <= E_cap
```

after every completed update, independent of AB stability assumptions.

### Periodic Re-anchoring

At every revolution boundary,

```text
q[n]   = q_ref[n]
q[n-1] = q_ref[n-1].
```

Resetting both values is important: resetting only the visible orientation would
leave contaminated multistep history in the next AB prediction. The complete
augmented AB state is therefore independent of prior revolutions.

The public camera frame repeats exactly after each revolution even though the
internal quaternion sign alternates:

```text
R(q[n+N]) = R(q[n]).
```

## Dynamic Error Bound

Let `e[n]` be geodesic prediction error relative to the independent reference.
Suppose the projected AB predictor has local Lipschitz gain `L` and total local
defect `delta`. Before the cap is applied,

```text
e_prediction[n+1] <= L*e[n] + delta.
```

Contractive correction gives

```text
e[n+1] <= (1-g)*L*e[n] + (1-g)*delta.
```

Define

```text
q = (1-g)*L.
```

When `q < 1`, the no-fallback input-to-state bound is

```text
e[n] <= q^n*e[0] + (1-g)*delta/(1-q).
```

The runtime cap is stronger than this model because it fails closed whenever the
measured error exceeds `E_cap`. Periodic re-anchoring additionally sets `e=0`
every `N` frames.

The independent reference has its own bounded evaluation and binary32 export
error `E_ref`. The total public-orientation bound is therefore

```text
E_total <= E_cap + E_ref.
```

Using the engineering reference budget in `tools/orbit_error_model.py` gives
approximately

```text
E_ref   <= 1.03e-7 radians
E_total <= 1.103e-6 radians.
```

A formal machine certificate would additionally require a documented `sin` and
`cos` error bound or exhaustive interval/MPFR evaluation of the finite `N`-phase
reference lattice.

## Measured Regression

For `N=3606`, a one-million-frame deterministic run measured:

```text
maximum AB2 prediction error = 2.2043e-9 radians
maximum corrected error      = 1.6532e-9 radians
periodic re-anchors           = 277
safety re-anchors             = 0
```

The maximum corrected error is below both the prediction error and the `1e-6`
radian fail-closed cap. Unit-vector and orthogonality errors remain below
`1e-12` in the double-precision integrator state.

## Plain Adams-Bashforth Comparison

For planar rotation `u'=i*omega*u`, plain AB2 has the recurrence

```text
u[n+1] = (1 + 3*i*x/2)u[n] - (i*x/2)u[n-1],
x = omega*h.
```

Its principal root has the small-step expansion

```text
log(r(x)) = i*x + i*(5/12)*x^3 + (1/4)*x^4 + O(x^5).
```

Thus uncorrected AB2 has amplitude growth and accumulating phase error. For
`N=3606`, the model gives a root magnitude of `1.0000000000023044` and a phase
error of approximately `7.948e-6` radians per revolution.

Projection alone removes amplitude drift but not accumulated phase drift. The
independent correction and complete periodic history reset are what make the
implemented method uniformly bounded.

## Runtime Evidence

Periodic camera-state events include:

```text
orbit_integrator=adams_bashforth_bounded_v1
orbit_prediction_error_radians=...
orbit_reference_error_radians=...
orbit_error_bound_radians=1e-6
orbit_max_reference_error_radians=...
orbit_periodic_reanchors=...
orbit_safety_reanchors=...
```

## Reproduction

Run the analytical model:

```bash
python3 tools/orbit_error_model.py --frames-per-revolution 3606
```

Run the deterministic implementation checks:

```bash
make test
```

## References

- D. Venturi, *Zero-stability and convergence of numerical methods for ODEs*.
- SUNDIALS CVODES documentation, *Mathematical Considerations* and local error control.
- M. Kobilarov, K. Crane, and M. Desbrun, *Lie Group Integrators for Animation and Control of Vehicles*, ACM Transactions on Graphics 28(2), 2009.
- D. R. Durran, *The Third-Order Adams-Bashforth Method: An Attractive Alternative to Leapfrog Time Differencing*, Monthly Weather Review 119(3), 1991.
