---
id: power/wp2-flywheel
title: Flywheel
category: power
tags: [flywheel, energy]
summary: A flywheel is a mechanical device that uses the conservation of angular momentum to store rotational energy, a form of kinetic energy proportional to the product of its moment of inertia and the square of its rotational speed.
source: https://en.wikipedia.org/w/index.php?title=Flywheel&oldid=1382741972
source_title: Wikipedia: Flywheel
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Flywheel", Wikipedia, The Free Encyclopedia (revision 1382741972), https://en.wikipedia.org/w/index.php?title=Flywheel&oldid=1382741972 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Flywheel

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **flywheel** is a mechanical device that uses the conservation of angular momentum to store rotational energy, a form of kinetic energy proportional to the product of its moment of inertia and the square of its rotational speed. In particular, assuming the flywheel's moment of inertia is constant (i.e., a flywheel with fixed mass and second moment of area revolving about some fixed axis) then the stored (rotational) energy is directly associated with the square of its rotational speed.

Since a flywheel serves to store mechanical energy for later use, it is natural to consider it as a kinetic energy analogue of an electrical inductor. Once suitably abstracted, this shared principle of energy storage is described in the generalized concept of an accumulator. As with other types of accumulators, a flywheel inherently smooths sufficiently small deviations in the power output of a system, thereby effectively playing the role of a low-pass filter with respect to the mechanical velocity (angular, or otherwise) of the system. More precisely, a flywheel's stored energy will donate a surge in power output upon a drop in power input and will conversely absorb any excess power input (system-generated power) in the form of rotational energy.

Common uses of a flywheel include smoothing a power output in reciprocating engines, flywheel energy storage, delivering energy at higher rates than the source, and controlling the orientation of a mechanical system using gyroscope and reaction wheel. Flywheels are typically made of steel and rotate on conventional bearings; these are generally limited to a maximum revolution rate of a few thousand RPM. High energy density flywheels can be made of carbon fiber composites and employ magnetic bearings, enabling them to revolve at speeds up to 60,000 RPM (1 kHz).

## Physics

The kinetic energy (or more specifically rotational energy) stored by the flywheel's rotor can be calculated by 1/2 I ω^2. ω is the angular velocity, and I is the moment of inertia of the flywheel about its axis of symmetry. The moment of inertia is a measure of resistance to torque applied on a spinning object (i.e. the higher the moment of inertia, the slower it will accelerate when a given torque is applied). The moment of inertia can be calculated for cylindrical shapes using mass (m) and radius (r). For a solid cylinder it is 1/2 mr^2, for a thin-walled empty cylinder it is approximately m r^2, and for a thick-walled empty cylinder with constant density it is 1/2 m(r_external^2 - r_internal^2).

For a given flywheel design, the kinetic energy is proportional to the ratio of the hoop stress to the material density and to the mass. The specific tensile strength of a flywheel can be defined as (σ_t)/(ρ). The flywheel material with the highest specific tensile strength will yield the highest energy storage per unit mass. This is one reason why carbon fiber is a material of interest. For a given design the stored energy is proportional to the hoop stress and the volume.

An electric motor-powered flywheel is common in practice. The output power of the electric motor is approximately equal to the output power of the flywheel. It can be calculated by (V_i)(V_t) ( (sin(δ))/(X_S) ), where V_i is the voltage of rotor winding, V_t is stator voltage, and δ is the angle between two voltages. Increasing amounts of rotation energy can be stored in the flywheel until the rotor shatters. This happens when the hoop stress within the rotor exceeds the ultimate tensile strength of the rotor material. Tensile stress can be calculated by ρ r^2 ω^2, where ρ is the density of the cylinder, r is the radius of the cylinder, and ω is the angular velocity of the cylinder.

## Design

A rimmed flywheel has a rim, a hub, and spokes. Calculation of the flywheel's moment of inertia can be more easily analysed by applying various simplifications. One method is to assume the spokes, shaft and hub have zero moments of inertia, and the flywheel's moment of inertia is from the rim alone. Another is to lump moments of inertia of spokes, hub and shaft into the rim. These may be estimated as a percentage of the flywheel's moment of inertia, with the majority from the rim, so that I_rim=KI_flywheel. For example, if the moments of inertia of hub, spokes and shaft are deemed negligible, and the rim's thickness is very small compared to its mean radius (R), the radius of rotation of the rim is equal to its mean radius and thus I_rim=M_rimR^2.

A shaftless flywheel eliminates the annulus holes, shaft or hub. It has higher energy density than conventional design but requires a specialized magnetic bearing and control system. The specific energy of a flywheel is determined byE/M = K (σ)/(ρ), in which K is the shape factor, σ the material's tensile strength and ρ the density. While a typical flywheel has a shape factor of 0.3, the shaftless flywheel has a shape factor close to 0.6, out of a theoretical limit of about 1.

A superflywheel consists of a solid core (hub) and multiple thin layers of high-strength flexible materials (such as special steels, carbon fiber composites, glass fiber, or graphene) wound around it. Compared to conventional flywheels, superflywheels can store more energy and are safer to operate. In case of failure, a superflywheel does not explode or burst into large shards like a regular flywheel, but instead splits into layers. The separated layers then slow a superflywheel down by sliding against the inner walls of the enclosure, thus preventing any further destruction. Although the exact value of energy density of a superflywheel would depend on the material used, it could theoretically be as high as 1200 Wh (4.4 MJ) per kg of mass for graphene superflywheels. The first superflywheel was patented in 1964 by the Soviet-Russian scientist Nurbei Guilia.

## Materials

Flywheels are made from many different materials; the application determines the choice of material. Small flywheels made of lead are found in children's toys. Cast iron flywheels are used in old steam engines. Flywheels used in car engines are made of cast or nodular iron, steel or aluminum. Flywheels made from high-strength steel or composites have been proposed for use in vehicle energy storage and braking systems.

The efficiency of a flywheel is determined by the maximum amount of energy it can store per unit weight. As the flywheel's rotational speed or angular velocity is increased, the stored energy increases; however, the stresses also increase. If the hoop stress surpass the tensile strength of the material, the flywheel will break apart. Thus, the tensile strength limits the amount of energy that a flywheel can store.

In this context, using lead for a flywheel in a child's toy is not efficient; however, the flywheel velocity never approaches its burst velocity because the limit in this case is the pulling-power of the child. In other applications, such as an automobile, the flywheel operates at a specified angular velocity and is constrained by the space it must fit in, so the goal is to maximize the stored energy per unit volume. The material selection therefore depends on the application.

_(Later sections of the original article were left out of this offline copy for length: Applications.)_
