---
id: electronics/wp2-toroidal-inductors-and-transformers
title: Toroidal inductors and transformers
category: electronics
tags: [toroidal inductors and transformers, electronics]
summary: Toroidal inductors and transformers are inductors and transformers which use magnetic cores with a toroidal (ring or donut) shape.
source: https://en.wikipedia.org/w/index.php?title=Toroidal_inductors_and_transformers&oldid=1378024397
source_title: Wikipedia: Toroidal inductors and transformers
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Toroidal inductors and transformers", Wikipedia, The Free Encyclopedia (revision 1378024397), https://en.wikipedia.org/w/index.php?title=Toroidal_inductors_and_transformers&oldid=1378024397 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Toroidal inductors and transformers

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Toroidal inductors and transformers** are inductors and transformers which use magnetic cores with a toroidal (ring or donut) shape. They are passive electronic components, consisting of a circular ring or donut-shaped magnetic core of ferromagnetic material such as laminated iron, iron powder, or ferrite, around which wire is wound.

Although closed-core inductors and transformers often use cores with a rectangular shape, the use of toroidal-shaped cores sometimes provides superior electrical performance. The advantage of the toroidal shape is that, due to its symmetry, the amount of magnetic flux that escapes outside the core (leakage flux) can be made low, potentially making it more efficient and making it emit less electromagnetic interference (EMI).

Toroidal inductors and transformers are used in a wide range of electronic circuits: power supplies, inverters, and amplifiers, which in turn are used in the vast majority of electrical equipment: TVs, radios, computers, and audio power amplifiers.

## Advantages

In general, toroidal inductors and transformers are more compact than other shaped cores, resulting in up to a 50% lighter weight design. This is especially the case for power devices.

Because the toroid is a closed-loop core, it has a higher inductance and Q factor than an inductor of the same mass with a straight core (solenoid coils). This is because most of the magnetic field is contained within the core. By comparison, with an inductor with a straight core, the magnetic field emerging from one end of the core has a long path through air to enter the other end.

Because the windings are relatively short and the magnetic field is closed, a toroidal transformer has higher efficiency and electrical performance, and reduces effects such as distortion and fringing.

Due to the symmetry of a toroid, little magnetic flux escapes from the core (leakage flux). Thus, a toroidal inductor or transformer couples less electromagnetic interference (EMI) to adjacent circuits and is often chosen for highly concentrated environments. Manufacturers have adopted toroidal coils in recent years to comply with standards limiting the amount of electromagnetic field consumer electronics can produce.

## Total B field confinement

In some circumstances, the current in the winding of a toroidal inductor contributes only to the **B** field inside the windings. It does not contribute to the magnetic **B** field outside the windings. This is a consequence of symmetry and Ampère's circuital law.

### Sufficient conditions

The absence of circumferential current (the path of circumferential current is indicated by the red arrow in figure 3 of this section) and the axially symmetric layout of the conductors and magnetic materials are sufficient conditions for total internal confinement of the **B** field. (Some authors prefer to use the **H** field). Because of the symmetry, the lines of B flux must form circles of constant intensity centered on the axis of symmetry. The only lines of B flux that encircle any current are those that are inside the toroidal winding. Therefore, from Ampere's circuital law, the intensity of the B field must be zero outside the windings.

Figure 3 of this section shows the most common toroidal winding. It fails both requirements for total B field confinement. Looking out from the axis, sometimes the winding is on the inside of the core and sometimes on the outside of the core. It is not axially symmetric in the near region. However, at points a distance of several times the winding spacing, the toroid does look symmetric. There is still the problem of the circumferential current. No matter how many times the winding encircles the core and no matter how thin the wire, this toroidal inductor will still include a one coil loop in the plane of the toroid. This winding will also produce and be susceptible to an **E** field in the plane of the inductor.

Figures 4-6 show different ways to neutralize the circumferential current. Figure 4 is the simplest and has the advantage that the return wire can be added after the inductor is bought or built.

### External electric field

There will be a distribution of potential along the winding. This can lead to an **E**-Field in the plane of the toroid and also a susceptibility to an **E** field in the plane of the toroid, as shown in figure 7. This can be mitigated by using a return winding, as shown in Figure 8. With this winding, each place the winding crosses itself, the two parts will be at equal and opposite polarity, which substantially reduces the E field generated in the plane.

### Magnetic vector potential

See Feynman chapter 14 and 15 for a general discussion of magnetic vector potential. See Feynman page 15-11 for a diagram of the magnetic vector potential around a long thin solenoid which also exhibits total internal confinement of the **B** field, at least in the infinite limit.

The **A** field is accurate when using the assumption bfA = 0. This would be true under the following assumptions:

1. the Coulomb gauge is used
1. the Lorenz gauge is used and there is no distribution of charge, ρ = 0
1. the Lorenz gauge is used and zero frequency is assumed
1. the Lorenz gauge is used and a non-zero frequency that is low enough to neglect (1)/(c^2)(∂φ)/(∂ t) is assumed.

Number 4 will be presumed for the rest of this section and may be referred to the "quasi-static condition".

Although the axially symmetric toroidal inductor with no circumferential current totally confines the **B** field within the windings, the **A** field (magnetic vector potential) is not confined. Arrow #1 in the picture depicts the vector potential on the axis of symmetry. Radial current sections a and b are equal distances from the axis but pointed in opposite directions, so they cancel. Likewise, segments c and d cancel. All the radial current segments cancel. The situation for axial currents is different. The axial current on the outside of the toroid is pointed down and the axial current on the inside of the toroid is pointed up. Each axial current segment on the outside of the toroid can be matched with an equal but oppositely directed segment on the inside of the toroid. The segments on the inside are closer than the segments on the outside to the axis, therefore there is a net upward component of the **A** field along the axis of symmetry.

Since the equations nabla × A = B \, and nabla × B = µ_0j \ (assuming quasi-static conditions, i.e. (∂ E)/(∂ t)→ 0 ) have the same form, then the lines and contours of **A** relate to **B** like the lines and contours of **B** relate to **j**. Thus, a depiction of the **A** field around a loop of **B** flux (as would be produced in a toroidal inductor) is qualitatively the same as the **B** field around a loop of current. The figure to the left is an artist's depiction of the **A** field around a toroidal inductor. The thicker lines indicate paths of higher average intensity (shorter paths have higher intensity so that the path integral is the same). The lines are just drawn to look good and impart a general look of the **A** field.

### Transformer action

The **E** and **B** fields can be computed from the **A** and φ (scalar electric potential) fields
B = nabla × A. and :E = - nabla φ - ( ∂ A )/( ∂ t ) and so even if the region outside the windings is devoid of **B** field, it is filled with non-zero **E** field.

The quantity ( ∂ A )/( ∂ t ) is responsible for the desirable magnetic field coupling between primary and secondary while the quantity nabla φ is responsible for the undesirable electric field coupling between primary and secondary. Transformer designers attempt to minimize the electric field coupling. For the rest of this section, nabla φ will be assumed to be zero unless otherwise specified.

Stokes theorem applies, so that the path integral of **A** is equal to the enclosed **B** flux, just as the path integral **B** is equal to a constant times the enclosed current

The path integral of **E** along the secondary winding gives the secondary's induced EMF (electromotive force).

 EMF = oint_path E · rm dl = -oint_path ( ∂ A )/( ∂ t ) · rm dl = -( ∂ )/( ∂ t ) oint_path A· rm dl = -( ∂ )/( ∂ t ) ∫_surface B· rm ds

which says the EMF is equal to the time rate of change of the B flux enclosed by the winding, which is the usual result.

### Poynting vector coupling

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Total B field confinement.)_
