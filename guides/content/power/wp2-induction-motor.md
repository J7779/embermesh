---
id: power/wp2-induction-motor
title: Induction motor
category: power
tags: [induction motor, asynchronous motor, induction machine, energy]
summary: An induction motor or asynchronous motor is an AC electric motor in which the electric current in the rotor that produces torque is obtained by electromagnetic induction from the magnetic field of the stator winding.
source: https://en.wikipedia.org/w/index.php?title=Induction_motor&oldid=1384577803
source_title: Wikipedia: Induction motor
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Induction motor", Wikipedia, The Free Encyclopedia (revision 1384577803), https://en.wikipedia.org/w/index.php?title=Induction_motor&oldid=1384577803 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Induction motor

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **induction motor** or **asynchronous motor** is an AC electric motor in which the electric current in the rotor that produces torque is obtained by electromagnetic induction from the magnetic field of the stator winding. An induction motor therefore needs no electrical connections to the rotor.

Three-phase squirrel-cage induction motors are widely used as industrial drives because they are self-starting, reliable, and economical. Single-phase induction motors are used extensively for smaller loads, such as garbage disposals and stationary power tools. Although traditionally used for constant-speed service, single- and three-phase induction motors are increasingly being installed in variable-speed applications using variable-frequency drives (VFD). VFD offers energy savings opportunities for induction motors in applications like fans, pumps, and compressors that have a variable load.

## Principle

### 3-phase motor

In both induction and synchronous motors, the AC power supplied to the motor's stator creates a magnetic field that rotates in synchronism with the AC oscillations. Whereas a synchronous motor's rotor turns at the same rate as the stator field, an induction motor's rotor rotates at a somewhat slower speed than the stator field. The induction motor's stator magnetic field is therefore changing or rotating relative to the rotor. This induces an opposing current in the rotor, in effect the motor's secondary winding. The rotating magnetic flux induces currents in the rotor windings, in a manner similar to currents induced in a transformer's secondary winding(s).

The induced currents in the rotor windings in turn create magnetic fields in the rotor that react against the stator field. The direction of the rotor magnetic field opposes the change in current through the rotor windings, following Lenz's Law. The cause of induced current in the rotor windings is the rotating stator magnetic field, so to oppose the change in rotor-winding currents the rotor turns in the direction of the stator magnetic field. The rotor accelerates until the magnitude of induced rotor current and torque balances the load on the rotor. Since rotation at synchronous speed does not induce rotor current, an induction motor always operates slightly slower than synchronous speed. The difference, or "slip," between actual and synchronous speed varies from about 0.5% to 5.0% for standard Design B torque curve induction motors. The induction motor's essential character is that torque is created solely by induction instead of the rotor being separately excited as in synchronous or DC machines or being self-magnetized as in permanent magnet motors.

For rotor currents to be induced, the speed of the physical rotor must be either slower or faster than that of the stator's rotating magnetic field (n_s); otherwise the magnetic field would not be moving relative to the rotor conductors and no currents would be induced. As the speed of the rotor drops below synchronous speed, the rotation rate of the magnetic field in the rotor increases, inducing more current in the windings and creating more torque. The ratio between the rotation rate of the magnetic field induced in the rotor and the rotation rate of the stator's rotating field is called "slip". Under load, the speed drops and the slip increases enough to create sufficient torque to turn the load. For this reason, induction motors are sometimes referred to as "asynchronous motors".

An induction motor can be used as an induction generator, or it can be unrolled to form a linear induction motor which can directly generate linear motion. The generating mode for induction motors is complicated by the need to excite the rotor, which begins with only residual magnetization. In some cases, that residual magnetization is enough to self-excite the motor under load. Therefore, it is necessary to either snap the motor and connect it momentarily to a live grid or to add capacitors charged initially by residual magnetism and providing the required reactive power during operation. Similar is the operation of the induction motor in parallel with a synchronous motor serving as a power factor compensator. A feature in the generator mode in parallel to the grid is that the rotor speed is higher than in the driving mode. Then active energy is being given to the grid. Another disadvantage of the induction motor generator is that it consumes a significant magnetizing current **I**₀ = (20–35)%.

### Synchronous speed

An AC motor's synchronous speed, f_s, is the rotation rate of the stator's magnetic field
f_s = 2f over p,
where f is the frequency of the power supply in hertz and p is the number of magnetic poles. For synchronous speed n_s in RPM, the formula becomes:
n_s = 2f/p· 60= (120 f)/(p).
For example, for a four-pole, three-phase motor, p = 4 and n_s = 120f over 4 = 1,500 RPM (for f = 50 Hz) and 1,800 RPM (for f = 60 Hz) synchronous speed.

The number of magnetic poles, p, is the number of north and south poles per phase. For example; a single-phase motor with 3 north and 3 south poles, having 6 poles per phase, is a 6-pole motor. A three-phase motor with 18 north and 18 south poles, having 6 poles per phase, is also a 6-pole motor. This industry standard method of counting poles results in the same synchronous speed for a given frequency regardless of polarity.

### Slip

Slip, s, is defined as the difference between synchronous speed and operating speed, at the same frequency, expressed in rpm, or in percentage or ratio of synchronous speed. Thus

s = (n_s-n_r)/(n_s)
where
n_s is stator electrical speed, n_r is rotor electrical speed.

For small amounts of slip, torque is approximately by:

T = (KsE^2_2R_2)/((R^2_2+ s^2X^2_2))

Where,
T: Running Torque
K: Transformer ratio = Rotor turns/stator turns
s: Slip of Motor
E_2: Rotor Voltage
R_2: Rotor Resistance
X_2: Rotor Reactance

The equation is correct for small and moderate amounts of slip, but not for large amounts of slip, such as a stopped rotor. Double revolving field theory (in the next section) provides a better approximation to actual torque. Slip, which varies from zero at synchronous speed and 1 when the rotor is stalled, influences the motor's torque.

Because the short-circuited rotor windings have small resistance, even a small slip induces a large current in the rotor and produces significant torque. At full rated load, slip varies from more than 5% for small or special purpose motors to less than 1% for large motors. These speed variations can cause load-sharing problems when differently sized motors are mechanically connected. Various methods are available to reduce slip, VFDs often offering the best solution.

### Double revolving field

 The rotor of an induction motor is subject to two rotating magnet fields, which rotate in opposite directions. Torque is the result of the difference between these two rotating fields.

The illustration shows torque as a function of slip. The green line shows torque developed by a single-phase induction motor, without a starting winding. No torque is developed when the rotor is stopped. A slight push in either direction will cause the rotor to accelerate in the direction that it was pushed.

Torque is on the vertical axis shown as a green line, and slip is on the horizontal axis. Slip ranges from zero to zero, corresponding to the reverse and forward directions of the rotor, with 1.0 slip in the center as the point where the rotor is stopped and no torque is produced.

Torque is the net result of two counter-rotating magnetic fields, with one shown as a blue line, and the other shown as a red line. The red and blue lines are mirror images of the typical torque curve shown in the previous section.

### Torque

#### Standard torque

The typical speed-torque relationship of a standard NEMA Design B polyphase induction motor is as shown in the curve at right. Suitable for most low performance loads such as centrifugal pumps and fans, Design B motors are constrained by the following typical torque ranges:

- *Breakdown torque* (peak torque), 175–300% of rated torque
- *Locked-rotor torque* (torque at 100% slip), 75–275% of rated torque
- Pull-up torque, 65–190% of rated torque.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Principle, Construction, Rotation reversal, Power factor, Efficiency, Steinmetz equivalent circuit, Linear induction motor.)_
